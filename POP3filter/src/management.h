#ifndef TPE_PROTOS_MANAGEMENT_H
#define TPE_PROTOS_MANAGEMENT_H

#include <netinet/in.h>
#include "selector.h"
#include "buffer.h"
#include "parse_helpers.h"

#define BUFFER_SIZE 1024

/**
 * POPG: Post Office Protocol confiGuration
 */

typedef enum parse_status {
    ST_HELO = 0,
    ST_USER = 1,
    ST_PASS = 2,
    ST_CONFIG = 3,
    ST_ERROR = 4,
} parse_status;

struct management{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    buffer                        buffer_write, buffer_read;
    uint8_t                       raw_buffer_write[BUFFER_SIZE], raw_buffer_read[BUFFER_SIZE];

    parse_status                  status;
    int                           argc;
    char **                       cmd;
    enum helper_errors            error;

    char *                        user;
};

void management_accept_connection(struct selector_key *key);

int split_commands(struct management *data);

int parse_commands(struct management *data);

#endif
