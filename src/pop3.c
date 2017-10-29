/**
 * pop3.c  - controla el flujo de un proxy POP3 (sockets no bloqueantes)
 */
#include<stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>

#include <arpa/inet.h>

#include "hello.h"
#include "request.h"
#include "buffer.h"

#include "stm.h"
#include "pop3.h"
#include "parameters.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))


/** maquina de estados general */
enum pop3_state {
    /**
     *
     */
            CONNECTING,
    /**
     *
     */
            HELLO_READ,
    /**
     *
     */
            HELLO_WRITE,
    /**
     *
     */
            REQUEST_READ,
    /**
     *
     */
            REQUEST_WRITE,
    /**
     *
     */
            RESPONSE_READ,
    /**
     *
     */
            RESPONSE_WRITE,

    // estados terminales
            DONE,
    ERROR,
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado

/** usado por CONNECTING */
// TODO remove no se usa nunca
struct connecting_st {
    buffer     *wb;
    const int  *client_fd;
    const int  *origin_fd;
};

/** usado por HELLO_READ, HELLO_WRITE */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer * rb;
};


/** usado por REQUEST_READ y REQUEST_WRITE */
struct request_st {
    /** buffer utilizado para I/O */
    buffer                     *wb;

    /** parser */
    struct request             request;
    struct request_parser      parser;

    /** el resumen del respuesta a enviar*/
    enum socks_response_status status;          //TODO en nuestro caso es +OK o -ERR

    const int                 *client_fd;
    int                       *origin_fd;
};

/** usado por RESPONSE_READ y RESPONSE_WRITE */
struct response_st {
    /** buffer utilizado para I/O */
    buffer                     *rb;
};

/*
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */
struct pop3 {
    /** información del cliente */
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    /** resolución de la dirección del origin server */
    struct addrinfo              *origin_resolution;
    /** intento actual de la dirección del origin server */
    struct addrinfo              *origin_resolution_current;

    /** información del origin server */
    // TODO ver si necesitamos guardar esto porque ya esta en parameters (variable goblal), tal vez lo necesitemos cuando haya que resolver nombres
    // pero no nos combiene resolver el nombre una sola vez al principio en vez de hacerlo por cada conexion?
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;


    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        struct connecting_st      conn;
        struct request_st         request;
    } client;
    /** estados para el origin_fd */
    union {
        struct hello_st           hello;
        struct response_st        response;
    } orig;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct pop3 *next;
};


/**
 * Pool de `struct pop3', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned  max_pool  = 50; // tamaño máximo
static unsigned        pool_size = 0;  // tamaño actual
static struct pop3     *pool     = 0;  // pool propiamente dicho

static const struct state_definition *
pop3_describe_states(void);

/** crea un nuevo `struct pop3' */
static struct pop3 *
pop3_new(int client_fd) {
    struct pop3 *ret;

    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret       = pool;
        pool      = pool->next;
        ret->next = 0;
    }
    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->origin_fd       = -1;
    ret->client_fd       = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    ret->stm    .initial   = CONNECTING;
    ret->stm    .max_state = ERROR;
    ret->stm    .states    = pop3_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references = 1;
    finally:
    return ret;
}

/** realmente destruye */
static void
pop3_destroy_(struct pop3 *s) {
    if(s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    free(s);
}

/**
 * destruye un  `struct pop3', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
pop3_destroy(struct pop3 *s) {
    if(s == NULL) {
        // nada para hacer
    } else if(s->references == 1) {
        if(s != NULL) {
            if(pool_size < max_pool) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                pop3_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void
pop3_pool_destroy(void) {
    struct pop3 *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
}

/** obtiene el struct (pop3 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3 *)(key)->data)

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void pop3_read(struct selector_key *key);
static void pop3_write(struct selector_key *key);
static void pop3_block(struct selector_key *key);
static void pop3_close(struct selector_key *key);
static const struct fd_handler pop3_handler = {
        .handle_read   = pop3_read,
        .handle_write  = pop3_write,
        .handle_close  = pop3_close,
        .handle_block  = pop3_block,
};

/** Intenta aceptar la nueva conexión entrante*/
void
pop3_passive_accept(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct pop3                *state           = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                              &client_addr_len);

    printf("client socket: %d\n", client);

    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    state = pop3_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &pop3_handler,
                                             OP_WRITE, state)) {
        goto fail;
    }
    return ;
    fail:
    if(client != -1) {
        close(client);
    }
    pop3_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// CONNECTING
////////////////////////////////////////////////////////////////////////////////

int origin_connect(struct selector_key *key);

void
connecting_init(const unsigned state, struct selector_key *key) {
    struct connecting_st *c = &ATTACHMENT(key)->client.conn;

    c->client_fd = &ATTACHMENT(key)->client_fd;
    c->origin_fd = &ATTACHMENT(key)->origin_fd;
    c->wb        = &ATTACHMENT(key)->write_buffer;
}

/** intenta establecer una conexión con el origin server */
unsigned
connecting(struct selector_key *key) {
    struct connecting_st *c = &ATTACHMENT(key)->client.conn;
    origin_connect(key);
    return HELLO_READ;
}


////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables de los estados HELLO_… */
static void
hello_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    d->rb = &(ATTACHMENT(key)->read_buffer);
}

/** lee todos los bytes del mensaje de tipo `hello' en server_fd*/
static unsigned
hello_read(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;
    unsigned  ret      = HELLO_READ;
    bool  error        = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);

    printf("HELLO_READ: %d\n", key->fd);
    printf("%s\n", ptr);

    if(n > 0) {
        buffer_write_adv(d->rb, n);
        //ret = hello_read_process();
        if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_NOOP)) {
            if (SELECTOR_SUCCESS == selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)) {
                ret = HELLO_WRITE;
            } else {
                ret = ERROR;
            }
        } else {
            ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

/** escribe todos los bytes del mensaje `hello' en client_fd */
static unsigned
hello_write(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    unsigned  ret      = HELLO_WRITE;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(d->rb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    printf("HELLO_WRITE: %d\n", key->fd);
    printf("%s\n", ptr);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->rb, n);
        //ret = hello_write_process();
        if(!buffer_can_read(d->rb)) {
            // el server_fd ya esta en NOOP (seteado en hello_read)
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = REQUEST_READ;
            } else {
                ret = ERROR;
            }
        }
    }

    return ret;
}

static void
hello_close(const unsigned state, struct selector_key *key) {
    struct hello_st * d = &ATTACHMENT(key)->orig.hello;
    buffer_reset(d->rb);
    //buffer_reset(d->wb);
}


////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

static unsigned request_process(struct selector_key *key, struct request_st * d);

/** inicializa las variables de los estados REQUEST_… */
static void
request_init(const unsigned state, struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;

    d->wb              = &(ATTACHMENT(key)->write_buffer);
    d->parser.request  = &d->request;
    request_parser_init(&d->parser);
    d->client_fd       = &ATTACHMENT(key)->client_fd;
    d->origin_fd       = &ATTACHMENT(key)->origin_fd;
}

/** Lee la request del cliente */
static unsigned
request_read(struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;

    buffer *b     = d->wb;
    unsigned  ret   = REQUEST_READ;
    bool  error = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    printf("REQ_READ: %d\n", key->fd);
    printf("%s\n", ptr);

    if(n > 0) {
        buffer_write_adv(b, n);
        ret = request_process(key, d); //
        //TODO descomentar lo siguiente cuando este implementado nuestro parser de request
//        enum request_state st = request_consume(b, &d->parser, &error);
//        if(request_is_done(st, 0)) {
//            ret = request_process(key, d);
//        }
    } else {
        ret = ERROR;
    }

    //return error ? ERROR : ret; // TODO cuando hagamos nuestro request_parser descomentar esto
    return ret;
}

static unsigned
request_process(struct selector_key *key, struct request_st * d) {
    enum pop3_state ret = REQUEST_WRITE;

    // transparente
    if (ret == REQUEST_WRITE) {
        if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_NOOP)) {
            if (SELECTOR_SUCCESS != selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE)) {
                ret = ERROR;
            }
        } else {
            ret = ERROR;
        }
    }

    return ret;
}
/** Escrible la request en el server */
static unsigned
request_write(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    unsigned  ret      = REQUEST_WRITE;
    buffer *b          = d->wb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    printf("REQ_WRITE: %d\n", key->fd);
    printf("%s\n", ptr);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if(!buffer_can_read(b)) {
            // el client_fd ya esta en NOOP (seteado en request_read)
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = RESPONSE_READ;
            } else {
                ret = ERROR;
            }
        }
    }

    return ret;
}

static void
request_close_(const unsigned state, struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;
    buffer_reset(d->wb);
    //buffer_reset(d->rb);
}

////////////////////////////////////////////////////////////////////////////////
// RESPONSE
////////////////////////////////////////////////////////////////////////////////

static unsigned response_process(struct selector_key *key, struct response_st * d);

/** inicializa las variables de los estados RESPONSE_… */
static void
response_init(const unsigned state, struct selector_key *key) {
    struct response_st * d = &ATTACHMENT(key)->orig.response;

    d->rb = &(ATTACHMENT(key)->read_buffer);
}

/** Lee la respuesta del origin server*/
static unsigned
response_read(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    unsigned  ret      = RESPONSE_READ;
    bool  error        = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);

    printf("RESP_READ: %d\n", key->fd);
    printf("%s\n", ptr);

    if(n > 0) {
        buffer_write_adv(d->rb, n);
        ret = response_process(key, &ATTACHMENT(key)->orig.response);
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

static unsigned
response_process(struct selector_key *key, struct response_st * d) {
    enum pop3_state ret = RESPONSE_WRITE;

    if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_NOOP)) {
        if (SELECTOR_SUCCESS != selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE)) {
            ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

/** Escrible la respuesta en el cliente */
static unsigned
response_write(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;

    enum pop3_state  ret      = RESPONSE_WRITE;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(d->rb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    printf("RESP_WRITE: %d\n", key->fd);
    printf("%s\n", ptr);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->rb, n);
        if (!buffer_can_read(d->rb)) {
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = REQUEST_READ;
            } else {
                ret = ERROR;
            }
        }
    }

    return ret;
}

static void
response_close(const unsigned state, struct selector_key *key) {
    struct response_st * d = &ATTACHMENT(key)->orig.response;
    buffer_reset(d->rb);
    //buffer_reset(d->wb);
}

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
        {
                .state            = CONNECTING,
                .on_arrival       = connecting_init,
                .on_write_ready   = connecting,
        },{
                .state            = HELLO_READ,
                .on_arrival       = hello_init,
                .on_read_ready    = hello_read,
        },{
                .state            = HELLO_WRITE,
                .on_write_ready   = hello_write,
                .on_departure     = hello_close,
        },{
                .state            = REQUEST_READ,
                .on_arrival       = request_init,
                .on_read_ready    = request_read,

        },{
                .state            = REQUEST_WRITE,
                .on_write_ready   = request_write,
                .on_departure     = request_close_,
        },{
                .state            = RESPONSE_READ,
                .on_arrival       = response_init,
                .on_read_ready    = response_read,
        },{
                .state            = RESPONSE_WRITE,
                .on_write_ready   = response_write,
                .on_departure     = response_close,
        },{
                .state            = DONE,

        },{
                .state            = ERROR,
        }
};
static const struct state_definition *
pop3_describe_states(void) {
    return client_statbl;
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.

static void
pop3_done(struct selector_key *key);

static void
pop3_read(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st = stm_handler_read(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st = stm_handler_write(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_block(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st = stm_handler_block(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_close(struct selector_key *key) {
    pop3_destroy(ATTACHMENT(key));
}

static void
pop3_done(struct selector_key *key) {
    const int fds[] = {
            ATTACHMENT(key)->client_fd,
            ATTACHMENT(key)->origin_fd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            close(fds[i]);
        }
    }
}

// Connection utilities

int
origin_connect(struct selector_key *key) {

    char * origin_server = parameters->origin_server;
    uint16_t origin_port = parameters->origin_port;

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    ATTACHMENT(key)->origin_fd = sock;

    printf("server socket: %d\n", sock);

    if (sock < 0) {
        perror("socket() failed");
        return sock;
    }

    if (selector_fd_set_nio(sock) == -1) {
        goto error;
    }

    /* Construct the server address structure */
    struct sockaddr_in servAddr;            /* Server address */
    memset(&servAddr, 0, sizeof(servAddr)); /* Zero out structure */
    servAddr.sin_family = AF_INET;          /* IPv4 address family */
    // Convert address
    int rtnVal = inet_pton(AF_INET, origin_server, &servAddr.sin_addr.s_addr);
    if (rtnVal == 0) {
        perror("inet_pton() failed - invalid address string");
        goto error;
    } else if (rtnVal < 0) {
        perror("inet_pton() failed");
        goto error;
    }
    servAddr.sin_port = htons(origin_port);    /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        if(errno == EINPROGRESS) {
            // es esperable,  tenemos que esperar a la conexión

            // dejamos de pollear el socket del cliente
            selector_status st = selector_set_interest_key(key, OP_NOOP);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }

            // esperamos la conexion en el nuevo socket
            st = selector_register(key->s, sock, &pop3_handler, OP_READ, key->data);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }
            ATTACHMENT(key)->references += 1;
        } else {
            goto error;
        }
    } else {
        // estamos conectados sin esperar... no parece posible
        // saltaríamos directamente a COPY
        abort();
    }

    return sock;

    error:
        if (sock != -1) {
            close(sock);
        }
        return -1;
}