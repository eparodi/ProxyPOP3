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

struct pop3_request_cmd {
    const enum pop3_cmd_id 	id;
    const char 				*name;
    //todo podriamos agregar una funcion de ejecucion aca
};

struct pop3_request {
    const struct pop3_request_cmd   *cmd;
    char                            *args;

    enum pop3_response_status       status;
};

/** Traduce un string a struct cmd */
const struct pop3_request_cmd * parse_cmd(const char *cmd);

// TODO el estado lo obtendria de la sesion pop3
enum pop3_cmd_id parse_cmd_2(unsigned state, const char *cmd);

#endif