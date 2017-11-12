#ifndef TPE_PROTOS_LOG_H
#define TPE_PROTOS_LOG_H

#include <arpa/inet.h>
#include <stdbool.h>
#include "request.h"
#include "response.h"

/** loguea la conexion a stdout */
void log_connection(bool opened, const struct sockaddr* clientaddr, const struct sockaddr* originaddr);

void log_request(struct pop3_request *r);

void log_response(const struct pop3_response *r);

#endif //TPE_PROTOS_LOG_H
