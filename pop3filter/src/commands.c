#include <stdlib.h>
#include <strings.h>
#include <memory.h>
#include <stdio.h>
#include "management.h"
#include "commands.h"
#include "parameters.h"
#include "media_types.h"
#include "metrics.h"

enum comm_status{
    COMM_OK                 = 0,
    COMM_ERR_MALLOC         = 1,
    COMM_ERR_WRONGARGS      = 2,
    COMM_ERR_NOT_RECOGNIZED = 3,
};

struct command{
    const char * comm;
    int          args;
    enum comm_status (*handler)(struct management * data);
};

enum comm_status hand_cmd(struct management * data){
    char ** cmd = data->cmd;
    char * str = malloc(sizeof(char) * strlen(cmd[1]) + 1);
    strcpy(str, cmd[1]);
    if (str == NULL)
        return COMM_ERR_MALLOC;
    if (parameters->filter_command != NULL)
        free(parameters->filter_command);
    parameters->filter_command = str;
    send_ok(data, "Done.");
    return COMM_OK;
}

enum comm_status hand_ext(struct management * data){
    parameters->et_activated = !parameters->et_activated;
    if (parameters->et_activated) {
        send_ok(data, "External transformations activated.");
    } else {
        send_ok(data, "External transformations deactivated.");
    }
    return COMM_OK;
}

enum comm_status hand_msg(struct management * data){
    char ** cmd = data->cmd;
    char * str = malloc(sizeof(char) * strlen(cmd[1]) + 1);
    if (str == NULL)
        return COMM_ERR_MALLOC;
    strcpy(str, cmd[1]);
    parameters->replacement_msg = cmd[1];
    send_ok(data, "Done.");
    return COMM_OK;
}

enum comm_status hand_list(struct management * data){
    char * msg = get_types_list(parameters->filtered_media_types, '\n');
    if (msg == NULL)
        return COMM_ERR_MALLOC;
    send_ok(data, msg);
    free(msg);
    return COMM_OK;
}

enum comm_status hand_ban(struct management * data){
    char ** cmd = data->cmd;
    char * str = malloc(sizeof(char) * strlen(cmd[1]) + 1);
    if (str == NULL)
        return COMM_ERR_MALLOC;
    strcpy(str, cmd[1]);
    char * type, * subtype;
    if (is_mime(str, &type, &subtype) < 0)
        return COMM_ERR_WRONGARGS;
    char * s = malloc(sizeof(char) * strlen(subtype) + 1);
    if (s == NULL){
        free(str);
        return COMM_ERR_MALLOC;
    }
    strcpy(s, subtype);
    if (add_media_type(parameters->filtered_media_types, type, s) < 0){
        send_error(data, "could not ban type");
    }else{
        send_ok(data, "type banned");
    }
    return COMM_OK;
}

enum comm_status hand_unban(struct management * data){
    char ** cmd = data->cmd;
    char * type, * subtype;
    if (is_mime(cmd[1], &type, &subtype) < 0)
        return COMM_ERR_WRONGARGS;
    if (delete_media_type(parameters->filtered_media_types, type, subtype) < 0){
        send_error(data, "could not unban type");
    }else{
        send_ok(data, "type unbanned");
    }
    return COMM_OK;
}

enum comm_status hand_stats(struct management * data){
    char msg[300];
    sprintf(msg, "\nMetrics\n"
                    "Concurrent connections: %u\n"
                    "Historical Access: %u\n"
                    "Transfered Bytes: %lld\n"
                    "Retrieved Messages: %u\n", metricas->concurrent_connections,
            metricas->historical_access, metricas->transferred_bytes,
            metricas->retrieved_messages);
    send_ok(data, msg);
    return COMM_OK;
}

static struct command comm_cmd = {
        .comm        = "CMD",
        .args        = 1,
        .handler     = &hand_cmd,
};

static struct command comm_ext = {
        .comm        = "EXT",
        .args        = 0,
        .handler     = &hand_ext,
};

static struct command comm_msg = {
        .comm        = "MSG",
        .args        = 1,
        .handler     = &hand_msg,
};

static struct command comm_list = {
        .comm        = "LIST",
        .args        = 0,
        .handler     = &hand_list,
};

static struct command comm_ban = {
        .comm        = "BAN",
        .args        = 1,
        .handler     = &hand_ban,
};

static struct command comm_unban = {
        .comm        = "UNBAN",
        .args        = 1,
        .handler     = &hand_unban,
};

static struct command comm_stats = {
        .comm        = "STATS",
        .args        = 0,
        .handler     = &hand_stats,
};

static struct command * command_list[] = {
        &comm_cmd,
        &comm_ext,
        &comm_msg,
        &comm_list,
        &comm_stats,
        &comm_ban,
        &comm_unban,
};

int parse_config(struct management *data){
    char ** cmd = data->cmd;
    enum comm_status st = COMM_ERR_NOT_RECOGNIZED;
    if (data->argc > 0){
        for (size_t i = 0; i < sizeof(command_list)/ sizeof(*command_list); i++){
            struct command * c = command_list[i];
            if (strcasecmp(c->comm, cmd[0]) == 0){
                if(c->args == data->argc - 1){
                    st = c->handler(data);
                }else{
                    send_error(data, "wrong number of arguments.");
                    return 0;
                }
            }
        }
    }
    switch (st){
        case COMM_ERR_NOT_RECOGNIZED:
            send_error(data, "not valid command.");
            break;
        case COMM_ERR_MALLOC:
            send_error(data, "server error. Try again later.");
            break;
        case COMM_ERR_WRONGARGS:
            send_error(data, "wrong arguments.");
            break;
        default:
            return 0;
    }
    return 0;
}