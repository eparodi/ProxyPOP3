/**
 * pop3.c  - controla el flujo de un proxy POP3 (sockets no bloqueantes)
 */
#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <unistd.h>  // close

#include <arpa/inet.h>
#include <pthread.h>

#include "pop3_session.h"
#include "buffer.h"
#include "stm.h"

#include "pop3.h"
#include "parameters.h"
#include "request_parser.h"
#include "response_parser.h"
#include "media_types.h"
#include "log.h"
#include "pop3_multi.h"
#include "metrics.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

/** maquina de estados general */
enum pop3_state {
    /**
     *  Resuelve el nombre del origin server
     *
     *  Transiciones:
     *      - CONNECTING    una vez que se resuelve el nombre
     *
     */
            ORIGIN_RESOLV,
    /**
     *  Espera que se establezca la conexion con el origin server
     *
     *  Transiciones:
     *      - HELLO    una vez que se establecio la conexion
     */
            CONNECTING,
    /**
     *  Lee el mensaje de bienvenida del origin server
     *
     *  Transiciones:
     *      - HELLO         mientras el mensaje no este completo
     *      - REQUEST       cuando está completo
     *      - ERROR         ante cualquier error (IO/parseo)
     *
     */
            HELLO,
    /**
     *  Lee una request del cliente y la manda al origin server
     */
            REQUEST,
    /**
     *  Lee la respuesta del origin server y se la envia al cliente
     */
            RESPONSE,
     /**
     *  Ejecuta una transformacion externa sobre un mail
     */
            EXTERNAL_TRANSFORMATION,

    // estados terminales
            DONE,
            ERROR,
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado


/** usado por HELLO */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer * wb;
};

/** usado por REQUEST */
struct request_st {
    /** buffer utilizado para I/O */
    buffer                     *rb, *wb;

    /** parser */
    struct pop3_request         request;
    struct request_parser       request_parser;

};

/** usado por RESPONSE */
struct response_st {
    buffer                      *rb, *wb;

    struct pop3_request         *request;
    struct response_parser      response_parser;
};

/** usado por EXTERNAL_TRANSFORMATION */
enum et_status {
    et_status_ok,
    et_status_err,
    et_status_done,
};

struct external_transformation {
    enum et_status              status;

    buffer                      *rb, *wb;
    buffer                      *ext_rb, *ext_wb;

    int                         *client_fd, *origin_fd;
    int                         *ext_read_fd, *ext_write_fd;

    struct parser               *parser;
};


/** Tamanio de los buffers de I/O */
#define BUFFER_SIZE 2048

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
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;

    int                           extern_read_fd;
    int                           extern_write_fd;

    struct pop3_session           session;

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        struct request_st         request;
    } client;
    /** estados para el origin_fd */
    union {
        struct hello_st           hello;
        struct response_st        response;
    } orig;

    struct external_transformation  et;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[BUFFER_SIZE], raw_buff_b[BUFFER_SIZE];
    buffer read_buffer, write_buffer;

    uint8_t raw_super_buffer[BUFFER_SIZE];
    buffer super_buffer;

    uint8_t raw_extern_read_buffer[BUFFER_SIZE];
    buffer extern_read_buffer;

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

    ret->stm    .initial   = ORIGIN_RESOLV;
    ret->stm    .max_state = ERROR;
    ret->stm    .states    = pop3_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    buffer_init(&ret->super_buffer,  N(ret->raw_super_buffer), ret->raw_super_buffer);
    buffer_init(&ret->extern_read_buffer,  N(ret->raw_extern_read_buffer), ret->raw_extern_read_buffer);

    pop3_session_init(&ret->session, false);

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
    metricas->concurrent_connections++;
    metricas->historical_access++;

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

/** Used before changing state to set the interests of both ends (client_fd, origin_fd) */
selector_status set_interests(fd_selector s, int client_fd, int origin_fd, enum pop3_state state) {
    fd_interest client_interest = OP_NOOP, origin_interest = OP_NOOP;
    selector_status status = SELECTOR_SUCCESS;

    switch (state) {
        case ORIGIN_RESOLV:
            break;
        case CONNECTING:
            break;
        case HELLO:
            origin_interest = OP_READ;
            break;
        case REQUEST:
            client_interest = OP_READ;
            break;
        case RESPONSE:
            origin_interest = OP_READ;
            break;
        case EXTERNAL_TRANSFORMATION:
            origin_interest = OP_NOOP;
            client_interest = OP_NOOP;
            break;
        default:
            break;
    }

    status |= selector_set_interest(s, client_fd, client_interest);
    status |= selector_set_interest(s, origin_fd, origin_interest);

    return status;
}

////////////////////////////////////////////////////////////////////////////////
// ORIGIN_RESOLV
////////////////////////////////////////////////////////////////////////////////

static void * origin_resolv_blocking(void *data);

static unsigned origin_connect(struct selector_key *key);

unsigned
origin_resolv(struct selector_key *key){

    pthread_t tid;
    struct selector_key* k = malloc(sizeof(*key));

    if(k == NULL) {
        //ret       = REQUEST_WRITE;
        //d->status = status_general_SOCKS_server_failure;
        //selector_set_interest_key(key, OP_WRITE);
    } else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0,
                                origin_resolv_blocking, k)) {
            //ret       = REQUEST_WRITE;
            //d->status = status_general_SOCKS_server_failure;
            //selector_set_interest_key(key, OP_WRITE);
        } else{
            //ret = REQUEST_RESOLV;
            selector_set_interest_key(key, OP_NOOP);
        }
    }


    return ORIGIN_RESOLV;
}

/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void *
origin_resolv_blocking(void *data) {
    struct selector_key *key = (struct selector_key *) data;
    struct pop3       *s   = ATTACHMENT(key);

    pthread_detach(pthread_self());
    s->origin_resolution = 0;
    struct addrinfo hints = {
            .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%hu",
             parameters->origin_port);

    if (0 != getaddrinfo(parameters->origin_server, buff, &hints,
                         &s->origin_resolution)){
        fprintf(stderr,"Domain name resolution error\n");
    }

    selector_notify_block(key->s, key->fd);

    free(data);

    return 0;
}

static unsigned
origin_resolv_done(struct selector_key *key) {
    struct pop3 *s      =  ATTACHMENT(key);

    if(s->origin_resolution == 0) {
        // TODO: error
    } else {
        s->origin_domain   = s->origin_resolution->ai_family;
        s->origin_addr_len = s->origin_resolution->ai_addrlen;
        memcpy(&s->origin_addr,
               s->origin_resolution->ai_addr,
               s->origin_resolution->ai_addrlen);
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }

    if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE)) {
        return ERROR;
    }

    return origin_connect(key);
}

static unsigned
origin_connect(struct selector_key *key) {

    int sock = socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, IPPROTO_TCP);
    //ATTACHMENT(key)->origin_fd = sock;

    printf("server socket: %d\n", sock);

    if (sock < 0) {
        perror("socket() failed");
        return ERROR;
    }

    if (selector_fd_set_nio(sock) == -1) {
        goto error;
    }

    /* Establish the connection to the origin server */
    if (-1 == connect(sock,
                      (const struct sockaddr *)&ATTACHMENT(key)->origin_addr,
                      ATTACHMENT(key)->origin_addr_len)) {
        if(errno == EINPROGRESS) {
            // es esperable,  tenemos que esperar a la conexión

            // dejamos de pollear el socket del cliente
            selector_status st = selector_set_interest_key(key, OP_NOOP);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }

            // esperamos la conexion en el nuevo socket
            st = selector_register(key->s, sock, &pop3_handler,
                                   OP_WRITE, key->data);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }
            ATTACHMENT(key)->references += 1;
        } else {
            goto error;
        }
    } else {
        // estamos conectados sin esperar... no parece posible
        abort();
    }

    return CONNECTING;

    error:
    if (sock != -1) {
        close(sock);
        //ATTACHMENT(key)->origin_fd = -1;
    }
    return ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// CONNECTING
////////////////////////////////////////////////////////////////////////////////

void
connecting_init(const unsigned state, struct selector_key *key) {
    // nada por hacer
}

void send_error_(int fd, const char * error) {
    send(fd, error, strlen(error), 0);
}

unsigned
connecting(struct selector_key *key) {
    int error;
    socklen_t len = sizeof(error);
    struct pop3 *d = ATTACHMENT(key);

    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        send_error_(d->client_fd, "-ERR Connection refused.\r\n");
        fprintf(stderr, "Connection to origin server failed\n");
        return ERROR;
    } else {
        if(error == 0) {
            d->origin_fd = key->fd;
        } else {
            send_error_(d->client_fd, "-ERR Connection refused.\r\n");
            fprintf(stderr, "Connection to origin server failed\n");
            return ERROR;
        }
    }

    log_connection(true, (const struct sockaddr *)&ATTACHMENT(key)->client_addr,
                   (const struct sockaddr *)&ATTACHMENT(key)->origin_addr);

    //TODO preguntar al server por pipelining
    bool pipelining = false;
    pop3_session_init(&ATTACHMENT(key)->session, pipelining);

    selector_status s;

    s = set_interests(key->s, d->client_fd, key->fd, HELLO);
    return SELECTOR_SUCCESS == s ? HELLO : ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables del estado HELLO */
static void
hello_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    d->wb = &(ATTACHMENT(key)->write_buffer);
}

/** Lee todos los bytes del mensaje de tipo `hello' de server_fd */
static unsigned
hello_read(struct selector_key *key) {
    struct hello_st *d      = &ATTACHMENT(key)->orig.hello;
    enum pop3_state  ret    = HELLO;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ///////////////////////////////////////////////////////
    //Proxy welcome message
    ptr = buffer_write_ptr(d->wb, &count);
    const char * msg = "+OK Proxy server POP3 ready.\r\n";
    n = strlen(msg);
    memccpy(ptr, msg, 0, count);
    buffer_write_adv(d->wb, n);
    //////////////////////////////////////////////////////

    ptr = buffer_write_ptr(d->wb, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(d->wb, 0);

        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        if (ss != SELECTOR_SUCCESS) {
            ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

/** Escribe todos los bytes del mensaje `hello' en client_fd */
static unsigned
hello_write(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    unsigned  ret      = HELLO;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->wb, n);
        if(!buffer_can_read(d->wb)) {
            // el server_fd ya esta en NOOP (seteado en hello_read)
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = REQUEST;
            } else {
                ret = ERROR;
            }
        }
    }

    return ret;
}

static void
hello_close(const unsigned state, struct selector_key *key) {
    //nada por hacer
}


////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

enum pop3_state request_process(struct selector_key *key, struct request_st * d);

/** inicializa las variables de los estados REQUEST y RESPONSE */
static void
request_init(const unsigned state, struct selector_key *key) {
    struct request_st * d = &ATTACHMENT(key)->client.request;

    d->rb              = &(ATTACHMENT(key)->read_buffer);
    d->wb              = &(ATTACHMENT(key)->write_buffer);

    d->request_parser.request  = &d->request;
    request_parser_init(&d->request_parser);
}

/** Lee la request del cliente */
static unsigned
request_read(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;
    enum pop3_state ret  = REQUEST;

    buffer *b            = d->rb;
    bool  error          = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0 || buffer_can_read(b)) {
        buffer_write_adv(b, n);
        enum request_state st = request_consume(b, &d->request_parser, &error);
        if (request_is_done(st, 0)) {
            ret = request_process(key, d);
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

#define MAX_CONCURRENT_INVALID_COMMANDS 3

// procesa una request ya parseada
enum pop3_state
request_process(struct selector_key *key, struct request_st * d) {
    enum pop3_state ret = REQUEST;

    if (d->request_parser.state >= request_error) {
        char * msg = NULL;

        //no la mandamos, le mandamos un mensaje de error al cliente y volvemos a leer de client_fd
        switch (d->request_parser.state) {
            case request_error:
                msg = "-ERR Unknown command. (POPG)\r\n";
                break;
            case request_error_cmd_too_long:
                msg = "-ERR Command too long.\r\n";
                break;
            case request_error_param_too_long:
                msg = "-ERR Parameter too long.\r\n";
                break;
            default:
                break;
        }

        send(key->fd, msg, strlen(msg), 0);

        ATTACHMENT(key)->session.concurrent_invalid_commands++;
        int cic = ATTACHMENT(key)->session.concurrent_invalid_commands;
        if (cic >= MAX_CONCURRENT_INVALID_COMMANDS) {
            msg = "-ERR Too many invalid commands. (POPG)\n";
            send(key->fd, msg, strlen(msg), 0);
            return DONE;
        }

        //reseteamos el parser
        request_parser_init(&d->request_parser);
        //set_interests(key->s, key->fd, ATTACHMENT(key)->origin_fd, REQUEST);
        return REQUEST;
    }

    ATTACHMENT(key)->session.concurrent_invalid_commands = 0;

    //por ahora asumimos que el origin server no soporta pipelining
    selector_status s = SELECTOR_SUCCESS;
    s |= selector_set_interest_key(key, OP_NOOP);
    s |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);

    ret = SELECTOR_SUCCESS == s ? ret : ERROR;

    return ret;
}

/** Escrible la request en el server */
static unsigned
request_write(struct selector_key *key) {
    struct request_st *d = &ATTACHMENT(key)->client.request;

    unsigned  ret      = REQUEST;
    buffer *b          = d->wb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    // por ahora asumimos que el server no soporta pipelining entonces mandamos las requests de a una.
    // si el server soporta pipelinig tendriamos que guardar las requests en request_read y crear un solo paquete aca
    // para mandarselo al server
    struct pop3_request *r = &d->request;

    if (-1 == request_marshall(r, b)) {
        ret = ERROR;
    }

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if(!buffer_can_read(b)) {
            log_request(r);
            // el client_fd ya esta en NOOP (seteado en request_read)
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = RESPONSE;
            } else {
                ret = ERROR;
            }
        }
    }

    return ret;
}

static void
request_close(const unsigned state, struct selector_key *key) {
    //struct request_st * d = &ATTACHMENT(key)->client.request;
    //request_parser_close(&d->request_parser); //TODO: UNDEFINED??????
}

////////////////////////////////////////////////////////////////////////////////
// RESPONSE
////////////////////////////////////////////////////////////////////////////////

enum pop3_state response_process(struct selector_key *key, struct response_st * d);

void
response_init(const unsigned state, struct selector_key *key) {
    struct response_st * d = &ATTACHMENT(key)->orig.response;

    d->rb                       = &ATTACHMENT(key)->write_buffer;
    d->wb                       = &ATTACHMENT(key)->super_buffer;
    d->request                  = &ATTACHMENT(key)->client.request.request;
    d->response_parser.request  = d->request;
    response_parser_init(&d->response_parser);
}

/**
 * Lee la respuesta del origin server. Si la respuesta corresponde al comando retr y se cumplen las condiciones,
 *  se ejecuta una transformacion externa
 */
static unsigned
response_read(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    unsigned  ret      = RESPONSE;
    bool  error        = false;

    buffer  *b         = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(b, n);
        enum response_state st = response_consume(b, d->wb, &d->response_parser, &error);

        // se termino de leer la primera linea
        if (d->response_parser.first_line_done) {
            d->response_parser.first_line_done = false;

            // si el comando era un retr y se cumplen las condiciones, disparamos la transformacion externa
            if (st == response_mail && d->request->response->status == response_status_ok
                && d->request->cmd->id == retr) {
                if (parameters->et_activated && parameters->filter_command != NULL) {
                    selector_status ss = SELECTOR_SUCCESS;
                    ss |= selector_set_interest_key(key, OP_NOOP);
                    ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_NOOP);

                    // consumimos la primera linea
                    while (buffer_can_read(d->wb)) {
                        buffer_read(d->wb);
                    }

                    return ss == SELECTOR_SUCCESS ? EXTERNAL_TRANSFORMATION : ERROR;
                }
            }

            //consumimos el resto de la respuesta
            st = response_consume(b, d->wb, &d->response_parser, &error);
        }

        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;

        if (response_is_done(st, 0)) {
            log_response(d->request->response);
        }
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

/** Escrible la respuesta en el cliente */
static unsigned
response_write(struct selector_key *key) {
    struct response_st *d = &ATTACHMENT(key)->orig.response;

    enum pop3_state  ret = RESPONSE;

    buffer *b = d->wb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b)) {
            if (d->response_parser.state != response_done) {
                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
                ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
            } else {
                ret = response_process(key, d);
            }
        }
    }

    return ret;
}

enum pop3_state
response_process(struct selector_key *key, struct response_st * d) {
    enum pop3_state ret;

    switch (d->request->cmd->id) {
        case quit:
            selector_set_interest_key(key, OP_NOOP);
            return DONE;
        case user:
            ATTACHMENT(key)->session.user = d->request->args;
            break;
        case pass:
            if (d->request->response->status == response_status_ok)
                ATTACHMENT(key)->session.state = POP3_TRANSACTION;
            break;
        case capa:
            //todo buscar pipelining en la respuesta, y agregarselo si no está
            // o cachear la respuesta al principio y mandarle la respuesta cacheada
            break;
        default:
            break;
    }

    if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
        ret = REQUEST;
    } else {
        ret = ERROR;
    }

    return ret;
}

static void
response_close(const unsigned state, struct selector_key *key) {
    //struct response_st * d = &ATTACHMENT(key)->orig.response;
    //response_parser_close(&d->response_parser);   //TODO: undefined GG again
}

////////////////////////////////////////////////////////////////////////////////
// EXTERNAL TRANSFORMATION
////////////////////////////////////////////////////////////////////////////////

enum et_status open_external_transformation(struct selector_key * key, struct pop3_session * session);

/** Inicializa las variables del estado EXTERNAL_TRANSFORMATION */
static void
external_transformation_init(const unsigned state, struct selector_key *key) {
    struct external_transformation *et = &ATTACHMENT(key)->et;

    et->rb           = &ATTACHMENT(key)->write_buffer;
    et->wb           = &ATTACHMENT(key)->extern_read_buffer;
    et->ext_rb       = &ATTACHMENT(key)->extern_read_buffer;
    et->ext_wb       = &ATTACHMENT(key)->write_buffer;

    et->origin_fd    = &ATTACHMENT(key)->origin_fd;
    et->client_fd    = &ATTACHMENT(key)->client_fd;
    et->ext_read_fd  = &ATTACHMENT(key)->extern_read_fd;
    et->ext_write_fd = &ATTACHMENT(key)->extern_write_fd;

    if (et->parser == NULL) {
        et->parser = parser_init(parser_no_classes(), pop3_multi_parser());
    }

    parser_reset(et->parser);

    et->status = open_external_transformation(key, &ATTACHMENT(key)->session);

    buffer  *b = et->wb;
    char * ptr;
    size_t   count;
    const char * err_msg = "-ERR could not open external transformation.\r\n";
    const char * ok_msg  = "+OK sending mail.\r\n";

    ptr = (char*) buffer_write_ptr(b, &count);
    if (et->status == et_status_err) {
        sprintf(ptr, "-ERR could not open external transformation.\r\n");
        buffer_write_adv(b, strlen(err_msg));

        selector_set_interest(key->s, *et->client_fd, OP_WRITE);
    } else {
        sprintf(ptr, "+OK sending mail.\r\n");
        buffer_write_adv(b, strlen(ok_msg));
    }
}

/** Lee el mail del server */
static unsigned
external_transformation_read(struct selector_key *key) {
    struct external_transformation *et  = &ATTACHMENT(key)->et;
    enum pop3_state ret                 = EXTERNAL_TRANSFORMATION;

    buffer  *b                          = et->rb;
    uint8_t *ptr;
    size_t   count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n   = recv(*et->origin_fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(b, n);
        selector_set_interest(key->s, *et->ext_write_fd, OP_WRITE);
        selector_set_interest(key->s, *et->origin_fd, OP_NOOP);
    } else {
        ret = ERROR;
    }

    return ret;
}

//escribir en el cliente
static unsigned
external_transformation_write(struct selector_key *key) {
    struct external_transformation *et  = &ATTACHMENT(key)->et;
    enum pop3_state ret                 = EXTERNAL_TRANSFORMATION;

    buffer  *b                          = et->wb;
    uint8_t *ptr;
    size_t   count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n   = send(*et->client_fd, ptr, count, 0);

    if(n > 0) {
        buffer_read_adv(b, n);
        selector_set_interest(key->s, *et->ext_read_fd, OP_READ);
        selector_set_interest(key->s, *et->client_fd, OP_NOOP);
    } else if (et->status == et_status_done) {
        selector_set_interest(key->s, *et->origin_fd, OP_NOOP);
        selector_set_interest(key->s, *et->client_fd, OP_READ);
        ret = REQUEST;
    } else {
        ret = ERROR;
    }

    return ret;
}

static void
external_transformation_close(const unsigned state, struct selector_key *key) {

}

////////////////////////////////////////////////////////////////////////////////
// EXTERNAL TRANSFORMATION HANDLERS
////////////////////////////////////////////////////////////////////////////////

void ext_read(struct selector_key * key) {
    struct external_transformation *et  = &ATTACHMENT(key)->et;

    buffer  *b                          = et->ext_rb;
    uint8_t *ptr;
    size_t   count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n   = read(*et->ext_read_fd, ptr, count);

    if (n <= 0) {
        //perror("read");
        selector_unregister_fd(key->s, key->fd);
    }
    else if  (n > 0) {
        selector_set_interest(key->s, *et->ext_read_fd, OP_NOOP);
        buffer_write_adv(b, n);
    }

    selector_set_interest(key->s, *et->client_fd, OP_WRITE);
}

void ext_write(struct selector_key * key){
    struct external_transformation *et  = &ATTACHMENT(key)->et;

    buffer  *b                          = et->ext_wb;
    uint8_t *ptr;
    size_t   count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n   = write(*et->ext_write_fd, ptr, count);

    if (n > 0) {

        while (buffer_can_read(b)) {
            uint8_t c = buffer_read(b);
            const struct parser_event *e = parser_feed(et->parser, c);
            //buffer_write(b, c);
            switch (e->type) {
                case POP3_MULTI_FIN:
                    log_response(ATTACHMENT(key)->orig.response.request->response);
                    selector_unregister_fd(key->s, *et->ext_write_fd);
                    et->status = et_status_done;
                    return;
            }
        }

        selector_set_interest(key->s, *et->ext_write_fd, OP_NOOP);
        selector_set_interest(key->s, *et->origin_fd, OP_READ);
    }
}

void ext_close(struct selector_key * key) {
    close(key->fd);
}

static const struct fd_handler ext_handler = {
        .handle_read   = ext_read,
        .handle_write  = ext_write,
        .handle_close  = ext_close,
        .handle_block  = NULL,
};

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
        {
                .state            = ORIGIN_RESOLV,
                .on_write_ready   = origin_resolv,
                .on_block_ready   = origin_resolv_done,
        },{
                .state            = CONNECTING,
                .on_arrival       = connecting_init,
                .on_write_ready   = connecting,
        },{
                .state            = HELLO,
                .on_arrival       = hello_init,
                .on_read_ready    = hello_read,
                .on_write_ready   = hello_write,
                .on_departure     = hello_close,
        },{
                .state            = REQUEST,
                .on_arrival       = request_init,
                .on_read_ready    = request_read,
                .on_write_ready   = request_write,
                .on_departure     = request_close,
        },{
                .state            = RESPONSE,
                .on_arrival       = response_init,
                .on_read_ready    = response_read,
                .on_write_ready   = response_write,
                .on_departure     = response_close,
        },{
                .state            = EXTERNAL_TRANSFORMATION,
                .on_arrival       = external_transformation_init,
                .on_read_ready    = external_transformation_read,
                .on_write_ready   = external_transformation_write,
                .on_departure     = external_transformation_close,
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
    const enum pop3_state st    = (enum pop3_state)stm_handler_read(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st    = (enum pop3_state)stm_handler_write(stm, key);

    if(ERROR == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_block(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st    = (enum pop3_state)stm_handler_block(stm, key);

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
    metricas->concurrent_connections--;
    log_connection(false, (const struct sockaddr *)&ATTACHMENT(key)->client_addr,
                   (const struct sockaddr *)&ATTACHMENT(key)->origin_addr);
}

////////////////////////////////////////////////////////////////////////////////
// EXTERNAL TRANSFORMATIONS
////////////////////////////////////////////////////////////////////////////////

char *concat_string(char *variable, char *value) {
    char * ret =  malloc((strlen(variable) + strlen(value)+1)*sizeof(char));
    ret[0] = '\0';
    strcat(ret, variable);
    strcat(ret, value);

    return ret;
}

enum et_status
open_external_transformation(struct selector_key * key, struct pop3_session * session) {

    char *medias               = get_types_list(parameters->filtered_media_types, ',');
    char *filter_medias         = concat_string("FILTER_MEDIAS=", medias);
    char *filter_msg            = concat_string("FILTER_MSG=",
                                                parameters->replacement_msg);
    char *pop3_filter_version   = concat_string("POP3_FILTER_VERSION=",
                                                parameters->version);
    char *pop3_username         = concat_string("POP3_USERNAME=", session->user);
    char *pop3_server           = concat_string("POP3_SERVER=", parameters->origin_server);

    free(medias);

    pid_t pid;
    char * args[4];
    args[0] = "bash";
    args[1] = "-c";
    args[2] = parameters->filter_command;
    args[3] = NULL;
    char *const environment[6]  = {filter_medias, filter_msg,
                                   pop3_filter_version,
                                   pop3_username, pop3_server, NULL};

    int fd_read[2];
    int fd_write[2];

    pipe(fd_read);
    pipe(fd_write);

    if ((pid = fork()) == -1)
        perror("fork error");
    else if (pid == 0) {
        dup2(fd_write[0], STDIN_FILENO);
        dup2(fd_read[1], STDOUT_FILENO);
        close(fd_write[1]);
        close(fd_read[0]);
        int value = execve("/bin/bash", args, environment);
        perror("execve");
        if (value == -1){
            fprintf(stderr, "Error\n");
        }
    }else{
        close(fd_write[0]);
        close(fd_read[1]);
        int i = 0;
        while(environment[i] != NULL){
            free(environment[i++]);
        }
        struct pop3 * data = ATTACHMENT(key);
        if (selector_register(key->s, fd_read[0], &ext_handler, OP_READ, data) == 0 &&
                selector_fd_set_nio(fd_read[0]) == 0){
            data->extern_read_fd = fd_read[0];
        }else{
            close(fd_read[0]);
            close(fd_write[1]);
            return et_status_err;
        } // read from.
        if (selector_register(key->s, fd_write[1], &ext_handler, OP_WRITE, data) == 0 &&
                selector_fd_set_nio(fd_write[1]) == 0){
            data->extern_write_fd = fd_write[1];
        }else{
            selector_unregister_fd(key->s, fd_write[1]);
            close(fd_read[0]);
            close(fd_write[1]);
            return et_status_err;
        } // write to.

        return et_status_ok;
    }
    return et_status_err;
}

