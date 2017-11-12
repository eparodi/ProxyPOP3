#include <time.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "utils.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

/** loguea la conexion a stdout */
void
log_connection(bool opened, const struct sockaddr* clientaddr,
               const struct sockaddr* originaddr) {
    char cbuff[SOCKADDR_TO_HUMAN_MIN * 2 + 2 + 32] = { 0 };
    unsigned n = N(cbuff);
    time_t now = 0;
    time(&now);

    // tendriamos que usar gmtime_r pero no está disponible en C99
    strftime(cbuff, n, "%FT%TZ\t", gmtime(&now));
    size_t len = strlen(cbuff);
    sockaddr_to_human(cbuff + len, N(cbuff) - len, clientaddr);
    strncat(cbuff, "\t", n-1);
    cbuff[n-1] = 0;
    len = strlen(cbuff);
    sockaddr_to_human(cbuff + len, N(cbuff) - len, originaddr);

    fprintf(stdout, "[%c] %s\n", opened? '+':'-', cbuff);
}

void log_request(struct pop3_request *r) {
    const struct pop3_request_cmd *cmd = r->cmd;
    fprintf(stdout, "request:  %s %s\n", cmd->name, ((r->args == NULL) ? "" : r->args));
}

void log_response(const struct pop3_response *r) {
    fprintf(stdout, "response: %s\n", r == NULL ? "" : r->name);
}