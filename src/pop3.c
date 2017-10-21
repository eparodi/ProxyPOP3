#include <sys/socket.h>
#include <zconf.h>
#include <memory.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <error.h>
#include "selector.h"
#include "parameters.h"

#define ATTACHMENT(key) ( (struct pop3 *)(key)->data)

options parameters;

void pop3_read(struct selector_key *key);

void pop3_write(struct selector_key *key);

void pop3_block(struct selector_key *key);

void pop3_close(struct selector_key *key);

static const struct fd_handler pop3_handler = {
        .handle_read   = pop3_read,
        .handle_write  = pop3_write,
        .handle_close  = pop3_close,
        .handle_block  = pop3_block,
};

struct pop3{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    int                           server_fd;
    char                          buffer[100];
    size_t                        buffer_length;
};

// pop3 struct functions.
struct pop3 *
pop3_new(const int client_fd){
    struct pop3 *ret;
    ret = malloc(sizeof(*ret));
    if (ret == NULL){
        return ret;
    }

    ret->client_fd     = client_fd;
    ret->buffer_length = 100;

    return ret;
}

void
pop3_destroy(struct pop3 * corpse){
    free(corpse);
    close(corpse->server_fd);
}

// handler functions.
char *message = "POP3 Proxy v1.0 \r\n";

void print_connection_status(const char * msg, struct sockaddr_storage addr);
int create_server_socket(char * origin_server, int origin_port, struct selector_key *key, struct pop3 *state);

void pop3_accept_connection(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct pop3                  *state           = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                              &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    print_connection_status("Connection established", client_addr);
    printf("File descriptor: %d\n", client);
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
                                             OP_READ, state)) {
        goto fail;
    }
    send(client, message, strlen(message), 0);
    int server_socket = create_server_socket(parameters->origin_server, parameters->origin_port, key, state);
    selector_set_interest_key(key, OP_READ);

    if (server_socket < 0){
        goto fail;
    }

    printf("Connection POP3 established. IP: %s, PORT: %d\n", parameters->origin_server, parameters->origin_port);
    printf("File descriptor: %d\n", server_socket);

    state->server_fd = server_socket;
    return ;

    fail:
    if(client != -1) {
        close(client);
    }
    pop3_destroy(state);
}

void
pop3_read(struct selector_key *key){
    struct pop3 *data = ATTACHMENT(key);
    ssize_t length = read(data->client_fd, data->buffer, 99);
    data->buffer[length] = '\0';
    send(data->server_fd, data->buffer, strlen(data->buffer), 0);
}

void
pop3_write(struct selector_key *key){
    struct pop3 *data = ATTACHMENT(key);
    ssize_t length = read(data->server_fd, data->buffer, 99);
    if (length == -1)  // TODO: @ELISEO PARODI ALMARAZ FIX THIS OR YOU ARE GONNA DIE AND THEN RECURSE, YOUR PAST YOU.
        return;
    data->buffer[length] = '\0';
    send(data->client_fd, data->buffer, strlen(data->buffer), 0);
}

void
pop3_block(struct selector_key *key){

}

void
pop3_close(struct selector_key *key){
    struct pop3 * data = ATTACHMENT(key);
    print_connection_status("Connection disconnected", data->client_addr);
    pop3_destroy(data);
}

// Connection utilities

int create_server_socket(char * origin_server, int origin_port, struct selector_key *key, struct pop3 *state) {

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

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
            st = selector_register(key->s, sock, &pop3_handler,
                                   OP_WRITE, state);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }
        } else {
            goto error;
        }
        perror("Connection to origin server failed");
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

void print_connection_status(const char * msg, struct sockaddr_storage addr){
    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];

    getnameinfo((struct sockaddr *)&addr,
                sizeof(addr), hoststr, sizeof(hoststr), portstr, sizeof(portstr),
                NI_NUMERICHOST | NI_NUMERICSERV);

    printf("%s: ip %s , port %s \n",
           msg,
           hoststr,
           portstr);
}