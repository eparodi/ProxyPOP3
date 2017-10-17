#include <stdint.h>
#include <netinet/in.h>

#ifndef TPE_PROTOS_MAIN_H
#define TPE_PROTOS_MAIN_H

typedef struct {
    int port;
    char * error_file;
    char * listen_address;
    char * management_address;
    int management_port;
    char * replacement_msg;
    char * filtered_media_types;
    char * origin_server;
    int origin_port;
    char *filter_command;

} options;

#endif //TPE_PROTOS_MAIN_H
