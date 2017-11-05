#include <sys/socket.h>
#include <zconf.h>
#include <memory.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "selector.h"
#include "parameters.h"
#include "utils.h"
#include "management.h"
#include "parse_helpers.h"

#define ATTACHMENT(key) ( (struct management *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))

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
    ret->status = ST_HELO;
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
    selector_set_interest(key->s, key->fd, OP_WRITE);
    struct management *data = ATTACHMENT(key);
    if (parse_commands(data) < 0){
        selector_unregister_fd(key->s, data->client_fd);
    };
}

void
management_write(struct selector_key *key){
    selector_set_interest(key->s, key->fd, OP_READ);
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

int parse_hello(struct management *data, char ** cmd);
int parse_user(struct management *data, char ** cmd);
int parse_pass(struct management *data, char ** cmd);
int parse_config(struct management *data, char ** cmd);

struct parse_action{
    parse_status status;
    int (* action)(struct management * data, char ** cmd);
    int args;
};

static struct parse_action hello_action = {
        .status      = ST_HELO,
        .action      = parse_hello,
        .args        = 1,
};

static struct parse_action user_action = {
        .status      = ST_USER,
        .action      = parse_user,
        .args        = 2,
};

static struct parse_action pass_action = {
        .status      = ST_PASS,
        .action      = parse_pass,
        .args        = 2,
};

static struct parse_action config_action = {
        .status      = ST_CONFIG,
        .action      = parse_config,
        .args        = 0,
};

static struct parse_action * action_list[] = {
        &hello_action,
        &user_action,
        &pass_action,
        &config_action,
};

int parse_commands(struct management *data){
    char ** cmd;
    struct parse_action * act = action_list[data->status];
    int status = 0;
    int st_err;
    data->argc = 0;
    cmd = parse_cmd(&data->buffer_read, data, &data->argc, &st_err);
    if (act->args != 0 && act->args != data->argc)
        st_err = ERROR_WRONGARGS;
    switch (st_err){
        case PARSE_OK:
            status = act->action(data, cmd);
            free_cmd(cmd, act->args);
            break;
        case ERROR_WRONGARGS:
            send_error(data, "wrong command or wrong number of arguments.");
            break;
        case ERROR_MALLOC:
            send_error(data, "server error. Try again later.");
        case ERROR_DISCONNECT:
            return -1;
        default:
            break;
    }
    return status;
}

int parse_hello(struct management *data, char ** cmd){
    if (strcasecmp("HELO", cmd[0]) == 0){
        send_ok(data, "hello!");
        data->status = ST_USER;
    }else{
        send_error(data, "command not recognized.");
    }
    return 0;
}

int parse_user(struct management *data, char ** cmd){
    if (strcasecmp("USER", cmd[0]) == 0){
        char welcome[] = "Welcome ";
        char * msg = malloc(strlen(welcome) + strlen(cmd[1]) + 2);
        strcpy(msg, welcome);
        strcat(msg, cmd[1]);
        strcat(msg, "\0");
        send_ok(data, msg);
        free(msg);
        data->status = ST_PASS;
    }else{
        send_error(data, "command not recognized.");
    }
    return 0;
}

int parse_pass(struct management *data, char ** cmd){
    if (strcasecmp("PASS", cmd[0]) == 0){
        if (strcmp("123456", cmd[1]) == 0){
            send_ok(data, "Logged in.");
            data->status = ST_CONFIG;
        }else{
            send_error(data, "Authentification failed. Try again.");
            data->status = ST_USER;
        }
    }else{
        send_error(data, "command not recognized.");
    }
    return 0;
}

int parse_config(struct management *data, char ** cmd){
    if (strcasecmp("QUIT", cmd[0]) == 0){
        if(data->argc != 1)
            goto fail;
        send_ok(data, "Goodbye.");
        close(data->client_fd);
        return -1;
    }else if(strcasecmp("CMD", cmd[0]) == 0){
        if (data->argc != 2)
            goto fail;
        char * str = malloc(sizeof(char) * strlen(cmd[1]));
        strcpy(str, cmd[1]);
        if (parameters->filter_command != NULL)
            free(parameters->filter_command);
        parameters->filter_command = str;
        send_ok(data, "Done.");
        return 0;
    }
    fail:
    send_error(data, "command not recognized.");
    return 0;
}