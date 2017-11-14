#ifndef TPE_PROTOS_LOG_H
#define TPE_PROTOS_LOG_H

#include <arpa/inet.h>
#include <stdbool.h>
#include "request.h"
#include "response.h"

/** loguea cuando se abre o cierra una conexion a stdout */
void log_connection(bool opened, const struct sockaddr* clientaddr, const struct sockaddr* originaddr);

/** loguea una request correspondiente a un comando pop3 */
void log_request(struct pop3_request *r);

/** loguea la respuesta a un comando valido pop3 */
void log_response(const struct pop3_response *r);

#endif //TPE_PROTOS_LOG_H
