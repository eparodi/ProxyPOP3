#ifndef TPE_PROTOS_POP3_SESSION_H
#define TPE_PROTOS_POP3_SESSION_H

#include <stdbool.h>

// estados independientes de la maquina de estados general
enum pop3_session_state {
    POP3_AUTHORIZATION,
    POP3_TRANSACTION,
    POP3_UPDATE,
    POP3_DONE,
};

//TODO agregar intentos de comandos invalidos (3 como dovecot)
// representa una sesion pop3
struct pop3_session {
    // long maxima: 512 bytes segun rfc de pop3
    char *user;
    char *password;

    enum pop3_session_state state;
    unsigned concurrent_invalid_commands;

    // podria ser una variable global?? -> no porque me pueden cambiar el origin server?
    bool pipelining;
};

void pop3_session_init(struct pop3_session *s, bool pipelining);

#endif //TPE_PROTOS_POP3_SESSION_H
