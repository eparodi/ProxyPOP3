#ifndef TPE_PROTOS_PARSE_HELPERS_H
#define TPE_PROTOS_PARSE_HELPERS_H

#include "buffer.h"
#include "management.h"

enum helper_errors{
    PARSE_OK         = 0,
    ERROR_MALLOC     = 1,
    ERROR_DISCONNECT = 2,
    ERROR_WRONGARGS  = 3,
};


struct management;

// char ** parse_text(buffer * b, char ** cmd, int args, int * args_found, bool * cmd_found);
char ** sctp_parse_cmd(buffer *b, struct management *data, int *args, int *st_err);

void free_cmd(char ** cmd, int args);

void send_error(struct management * data, const char * text);

void send_ok(struct management * data, const char * text);

#endif //TPE_PROTOS_PARSE_HELPERS_H
