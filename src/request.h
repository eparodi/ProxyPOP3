#ifndef POP3_REQUEST_H_
#define POP3_REQUEST_H_

#include "response.h"

enum pop3_cmd_id {

    error = -1,

    /* valid in the AUTHORIZATION state */
    user, pass,	// supported
    apop,  		// unsupported

    /* valid in the TRANSACTION state */
    stat, list, retr, dele, noop, rset, // supported
    top, uidl, 							// unsupported

    /* other */
    quit, capa, // supported
};

/** Puntero a funcion usado cuando se ejecutan los comandos */
typedef void (*command_fn)(struct pop3_request *r, int client_fd, int origin_fd);

struct pop3_request_cmd {
    const enum pop3_cmd_id 	id;
    const char 				*name;
    command_fn              fn;
};

struct pop3_request {
    const struct pop3_request_cmd   *cmd;
    char                            *args;

    const struct pop3_response      *response;
};


/** Traduce un string a struct cmd */
const struct pop3_request_cmd * get_cmd(const char *cmd);

// TODO el estado lo obtendria de la sesion pop3
//enum pop3_cmd_id parse_cmd_2(unsigned state, const char *cmd);

#endif