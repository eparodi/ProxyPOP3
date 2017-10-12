#include <stdint.h>
#include <netinet/in.h>

#ifndef TPE_PROTOS_MAIN_H
#define TPE_PROTOS_MAIN_H

typedef struct {
    in_addr_t pop3_ip;         /* pop3 ip */
    in_port_t pop3_port;
    in_port_t port;       /* proxy listen port */

} options;

#endif //TPE_PROTOS_MAIN_H
