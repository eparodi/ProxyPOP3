#ifndef TPE_PROTOS_PARAMETERS_C_H
#define TPE_PROTOS_PARAMETERS_C_H

#include <stdint.h>
#include <netinet/in.h>

struct options {
    uint16_t port;
    char * error_file;
    char * listen_address;
    char * management_address;
    uint16_t management_port;
    char * replacement_msg;
    struct media_types * filtered_media_types;
    char * origin_server;
    uint16_t origin_port;
    char * filter_command;
    char * version;
    struct addrinfo * listenadddrinfo;
    struct addrinfo * managementaddrinfo;
};

typedef struct options * options;

options parse_options(int argc, char **argv);

extern options parameters;

#endif
