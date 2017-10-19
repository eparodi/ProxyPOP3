#include <stdint.h>
#include <netinet/in.h>

#ifndef TPE_PROTOS_MAIN_H
#define TPE_PROTOS_MAIN_H

typedef struct {
    uint16_t port;
    char * error_file;
    char * listen_address;
    char * management_address;
    uint16_t management_port;
    char * replacement_msg;
    char * filtered_media_types;
    char * origin_server;
    uint16_t origin_port;
    char *filter_command;

} options;

#endif //TPE_PROTOS_MAIN_H
