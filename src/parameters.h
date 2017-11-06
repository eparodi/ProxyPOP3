#ifndef TPE_PROTOS_PARAMETERS_C_H
#define TPE_PROTOS_PARAMETERS_C_H

#include <stdint.h>

struct options {
    uint16_t port;
    char * error_file;
    in_addr_t listen_address;
    in_addr_t management_address;
    uint16_t management_port;
    char * replacement_msg;
    char * filtered_media_types;
    char * origin_server;
    uint16_t origin_port;
    char * filter_command;

};

typedef struct options * options;

options parse_options(int argc, char **argv);

extern options parameters;

#endif
