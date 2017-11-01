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

#include <arpa/inet.h>

#include "request.h"
#include "buffer.h"

#include "stm.h"
#include "pop3.h"
#include "parameters.h"
#include "utils.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

//TODO PIPELINING
/** maquina de estados general */
enum pop3_state {
    /**
     *  Espera que se establezca la conexion con el origin server
     *
     *  Transiciones:
     *      - HELLO_READ    una vez que se establecio la conexion
     */
            CONNECTING,
    /**
     *  Lee el mensaje de bienvenida del origin server
     *
     *  Intereses:
     *      - OP_READ sobre origin_fd
     *      - OP_NOOP sobre client_fd
     *
     *  Transiciones:
     *      - HELLO_READ    mientras el mensaje no este completo
     *      - HELLO_WRITE   cuando está completo
     *      - ERROR         ante cualquier error (IO/parseo)
     *
     */
            HELLO_READ,
    /**
     *  Envia el mensaje de bienvenida al cliente
     *
     *  Intereses:
     *      - OP_WRITE   sobre client_fd
     *      - OP_NOOP    sobre origin_fd
     *
     * Transiciones:
     *      - HELLO_WRITE  mientras queden bytes por enviar
     *      - REQUEST_READ cuando se enviaron todos los bytes
     *      - ERROR        ante cualquier error (IO/parseo)
     */
            HELLO_WRITE,
    /**
     *  Lee una request del cliente (todas las requests terminan en '\n')
     */
            REQUEST_READ,
    /**
     *  Le envia las requests al origin server
     */
            REQUEST_WRITE,
    /**
     *  Lee la respuesta del origin server
     */
            RESPONSE_READ,
    /**
     *  Le envia la respuesta al cliente
     */
            RESPONSE_WRITE,

    // estados terminales
            DONE,
    ERROR,
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado


/** usado por HELLO_READ, HELLO_WRITE */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer * wb;
};

/** usado por REQUEST_READ, REQUEST_WRITE, RESPONSE_READ y RESPONSE_WRITE */
struct request_st {
    /** buffer utilizado para I/O */
    buffer                     *rb, *wb;

    /** parser */
    struct request             request;
    struct request_parser      parser;

    /** el resumen del respuesta a enviar*/
    enum pop3_response_status status;          //TODO en nuestro caso es +OK o -ERR

    const int                 *client_fd;
    int                       *origin_fd;
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
    // TODO resolucion de nombres por cada conexion entrante (puede cambiar la ip de un host)
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        struct request_st         request;
    } client;
    /** estados para el origin_fd */
    union {
        struct hello_st           hello;
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

void log_connection(bool opened, const struct sockaddr* clientaddr,
            const struct sockaddr* originaddr);

int origin_connect(struct selector_key *key);

/** intenta establecer una conexión con el origin server */
unsigned
connect_(struct selector_key *key) {
    //TODO validar conexion exitosa
    origin_connect(key);

    log_connection(true, (const struct sockaddr *)&ATTACHMENT(key)->client_addr,
                (const struct sockaddr *)&ATTACHMENT(key)->origin_addr);

    //print_connection_status("origin server", ATTACHMENT(key)->origin_addr);
    //print_connection_status("client", ATTACHMENT(key)->client_addr);

    return HELLO_READ;
}


////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables de los estados HELLO_… */
static void
hello_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    d->wb = &(ATTACHMENT(key)->write_buffer);
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

    ///////////////////////////////////////////////////////
    //Proxy welcome message
    ptr = buffer_write_ptr(d->wb, &count);
    const char * msg = "Proxy server POP3 - POPG version 1.0\n";
    n = strlen(msg);
    memccpy(ptr, msg, 0, count);
    buffer_write_adv(d->wb, n);
    //////////////////////////////////////////////////////

    ptr = buffer_write_ptr(d->wb, &count);
    n = recv(key->fd, ptr, count, 0);

    //printf("HELLO_READ: %d\n", key->fd);
    //printf("%s\n", ptr);

    if(n > 0) {
        buffer_write_adv(d->wb, n);
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

    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    //printf("HELLO_WRITE: %d\n", key->fd);
    //printf("%s\n", ptr);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->wb, n);
        //ret = hello_write_process();
        if(!buffer_can_read(d->wb)) {
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
    buffer_reset(d->wb);
    //buffer_reset(d->wb);
}


////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

static unsigned request_process(struct selector_key *key, struct request_st * d);

/** inicializa las variables de los estados REQUEST_… y RESPONSE_ */
static void
request_init(const unsigned state, struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;

    d->rb              = &(ATTACHMENT(key)->read_buffer);
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

    buffer *b     = d->rb;
    unsigned  ret   = REQUEST_READ;
    bool  error = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    //printf("REQ_READ: %d\n", key->fd);
    //printf("%s\n", ptr);

    if(n > 0) {
        buffer_write_adv(b, n);
//        ret = request_process(key, d);
        enum request_state st = request_consume(b, &d->parser, &error);
        if(request_is_done(st, 0)) {
            ret = request_process(key, d);
        }
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

static unsigned
request_process(struct selector_key *key, struct request_st * d) {
    enum pop3_state ret = REQUEST_WRITE;

    if (ret == REQUEST_WRITE) {
        selector_status s = 0;
        s |= selector_set_interest_key(key, OP_NOOP);
        s |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);

        ret = SELECTOR_SUCCESS == s ? REQUEST_WRITE : ERROR;
    }

    if (-1 == request_marshall(&d->request, d->rb)) {
        ret = ERROR;
    }

   return ret;
}
/** Escrible la request en el server */
static unsigned
request_write(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    unsigned  ret      = REQUEST_WRITE;
    buffer *b          = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    //printf("REQ_WRITE: %d\n", key->fd);
    //printf("%s\n", ptr);

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
    buffer_reset(d->rb);
}

////////////////////////////////////////////////////////////////////////////////
// RESPONSE
////////////////////////////////////////////////////////////////////////////////

static unsigned response_process(struct selector_key *key, struct request_st * d);

/** Lee la respuesta del origin server*/
static unsigned
response_read(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;
    unsigned  ret      = RESPONSE_READ;
    bool  error        = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(d->wb, &count);
    n = recv(key->fd, ptr, count, 0);

    //printf("RESP_READ: %d\n", key->fd);
    //printf("%s\n", ptr);

    if(n > 0) {
        buffer_write_adv(d->wb, n);
        ret = response_process(key, &ATTACHMENT(key)->client.request);
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

static unsigned
response_process(struct selector_key *key, struct request_st * d) {
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
    struct request_st *d = &ATTACHMENT(key)->client.request;

    enum pop3_state  ret      = RESPONSE_WRITE;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    //printf("RESP_WRITE: %d\n", key->fd);
    //printf("%s\n", ptr);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->wb, n);
        if (!buffer_can_read(d->wb)) {
            if (d->request.cmd == pop3_req_quit) {
                ret = DONE;
            }
            else if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
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
    struct request_st * d = &ATTACHMENT(key)->client.request;
    buffer_reset(d->wb);
}

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
        {
                .state            = CONNECTING,
                .on_write_ready   = connect_,
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

    log_connection(false, (const struct sockaddr *)&ATTACHMENT(key)->client_addr,
                   (const struct sockaddr *)&ATTACHMENT(key)->origin_addr);
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
    struct sockaddr_in origin_addr;            /* Server address */
    socklen_t origin_addr_len;

    memset(&origin_addr, 0, sizeof(origin_addr)); /* Zero out structure */
    origin_addr.sin_family = AF_INET;          /* IPv4 address family */
    // Convert address
    int rtnVal = inet_pton(AF_INET, origin_server, &origin_addr.sin_addr.s_addr);
    if (rtnVal == 0) {
        perror("inet_pton() failed - invalid address string");
        goto error;
    } else if (rtnVal < 0) {
        perror("inet_pton() failed");
        goto error;
    }
    origin_addr.sin_port = htons(origin_port);    /* Server port */

    origin_addr_len = sizeof(origin_addr);

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &origin_addr, origin_addr_len) < 0) {
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
            memcpy(&ATTACHMENT(key)->origin_addr, &origin_addr, origin_addr_len);
            ATTACHMENT(key)->origin_addr_len = origin_addr_len;
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

/** loguea la conexion a stdout */
void
log_connection(bool opened, const struct sockaddr* clientaddr,
               const struct sockaddr* originaddr) {
    char cbuff[SOCKADDR_TO_HUMAN_MIN * 2 + 2 + 32] = { 0 };
    unsigned n = N(cbuff);
    time_t now = 0;
    time(&now);

    // tendriamos que usar gmtime_r pero no está disponible en C99
    strftime(cbuff, n, "%FT%TZ\t", gmtime(&now));
    size_t len = strlen(cbuff);
    sockaddr_to_human(cbuff + len, N(cbuff) - len, clientaddr);
    strncat(cbuff, "\t", n-1);
    cbuff[n-1] = 0;
    len = strlen(cbuff);
    sockaddr_to_human(cbuff + len, N(cbuff) - len, originaddr);

    fprintf(stdout, "[%c] %s\n", opened? '+':'-', cbuff);
}

void log_request() {

}

void log_response() {

}
