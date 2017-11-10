#ifndef POP3CTL_POP3CTL_H
#define POP3CTL_POP3CTL_H

struct options {
    in_addr_t management_address;
    uint16_t management_port;
    int listen_family;
};

typedef struct options * options;

options parse_options(int argc, char **argv);

extern options ctl_parameters;

#endif //POP3CTL_POP3CTL_H
