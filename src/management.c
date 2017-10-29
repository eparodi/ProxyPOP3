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
#include "management.h"

#define ATTACHMENT(key) ( (struct management *)(key)->data)
#define N(x) (sizeof(x)/sizeof(x[0]))

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

// management struct functions.
struct management *
management_new(const int client_fd){
    struct management *ret;
    ret = malloc(sizeof(*ret));
    if (ret == NULL){
        return ret;
    }

    ret->client_fd     = client_fd;
    buffer_init(&ret->buffer_write, N(ret->raw_buffer_write),ret->raw_buffer_write);
    buffer_init(&ret->buffer_read , N(ret->raw_buffer_read) ,ret->raw_buffer_read);
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
    if (parse_commands(data) < 0){
        selector_unregister_fd(key->s, data->client_fd);
    };
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

/* PARSER */

void parse_hello(struct management * data);

struct parse_action{
    parse_status status;
    void (* action)(struct management * data);
};

static const struct parse_action hello_action = {
        .status      = ST_HELO,
        .action      = parse_hello,
};

//static const struct parse_action action_list[] = {
//        hello_action,
//};

int
parse_commands(struct management *data){
    size_t count;
    uint8_t * ptr;
    ptr = buffer_write_ptr(&data->buffer_read, &count);
    ssize_t length = recv(data->client_fd, ptr, count, 0);
    if (length > 0){
        buffer_write_adv(&data->buffer_read, length);
        ptr = buffer_read_ptr(&data->buffer_read, &count);
        length = send(data->client_fd, ptr, count, MSG_NOSIGNAL);
        buffer_read_adv(&data->buffer_read, length);
    }else{
        return -1;
    }
    return 0;
}

void parse_hello(struct management * data){

}