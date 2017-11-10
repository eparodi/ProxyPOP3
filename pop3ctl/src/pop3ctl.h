#ifndef POP3CTL_POP3CTL_H
#define POP3CTL_POP3CTL_H

struct options {
    char* management_address;
    char* management_port;
    struct addrinfo * managementaddrinfo;
};

typedef struct options * options;

options parse_options(int argc, char **argv);

extern options ctl_parameters;

#endif //POP3CTL_POP3CTL_H
