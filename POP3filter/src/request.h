#ifndef POP3_REQUEST_H_
#define POP3_REQUEST_H_

#include "response.h"

enum pop3_cmd_id {

    error = -1,

    /* valid in the AUTHORIZATION state */
    user, pass,
    apop,

    /* valid in the TRANSACTION state */
    stat, list, retr, dele, noop, rset,
    top, uidl,

    /* other */
    quit, capa,
};

struct pop3_request_cmd {
    const enum pop3_cmd_id 	id;
    const char 				*name;
};

struct pop3_request {
    const struct pop3_request_cmd   *cmd;
    char                            *args;

    const struct pop3_response      *response;
};


/** Traduce un string a struct cmd */
const struct pop3_request_cmd * get_cmd(const char *cmd);

struct pop3_request * new_request(const struct pop3_request_cmd * cmd, char * args);

void destroy_request(struct pop3_request *r);

#endif