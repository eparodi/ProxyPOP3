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
#include "utils.h"
#define ATTACHMENT(key) ( (struct management *)(key)->data)

options parameters;

void management_read(struct selector_key *key);

void management_write(struct selector_key *key);

void management_block(struct selector_key *key);

void management_close(struct selector_key *key);

static const struct fd_handler management_handler = {
        .handle_read   = management_read,
        .handle_write  = management_write,
        .handle_close  = management_close,
        .handle_block  = management_block,
};

struct management{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    char                          buffer[100];
    size_t                        buffer_length;
};

// management struct functions.
struct management *
management_new(const int client_fd){
    struct management *ret;
    ret = malloc(sizeof(*ret));
    if (ret == NULL){
        return ret;
    }

    ret->client_fd     = client_fd;
    ret->buffer_length = 100;

    return ret;
}

void
management_destroy(struct management * corpse){
    free(corpse);
}

// handler functions

void management_accept_connection(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct management                  *state           = NULL;

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
    state = management_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }

    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &management_handler,
                                             OP_READ, state)) {
        goto fail;
    }

    char * message = "POP3 Proxy Management Server.\n";
    send(client, message, strlen(message), 0);
    return ;

    fail:
    if(client != -1) {
        close(client);
    }
    management_destroy(state);
}

void
management_read(struct selector_key *key){
    struct management *data = ATTACHMENT(key);
    ssize_t length = read(data->client_fd, data->buffer, 99);
    data->buffer[length] = '\0';
    send(data->client_fd, data->buffer, strlen(data->buffer), 0);
}

void
management_write(struct selector_key *key){

}

void
management_block(struct selector_key *key){

}

void
management_close(struct selector_key *key){
    struct management * data = ATTACHMENT(key);
    print_connection_status("Connection disconnected", data->client_addr);
    management_destroy(data);
}
