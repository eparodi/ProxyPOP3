#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>

#include "selector.h"
#include "parameters.h"
#include "pop3.h"
#include "management.h"

#define PENDING_CONNECTIONS 10

int create_master_socket(int protocol, struct sockaddr_in * addr){
    int master_socket;
    int sock_opt = true;
    struct sockaddr_in address = * addr;

    // create a master tcp socket
    if ((master_socket = socket(AF_INET , SOCK_STREAM , protocol)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections
    // this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&sock_opt, sizeof(sock_opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //bind the socket to localhost port
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    return master_socket;
}

int main (int argc, char ** argv) {

    options opt = parse_options(argc,argv);

    struct sockaddr_in address_proxy;

    //type of socket created
    address_proxy.sin_family      = AF_INET;
    address_proxy.sin_addr.s_addr = INADDR_ANY;
    address_proxy.sin_port        = htons(opt->port);

    struct sockaddr_in address_management;

    address_management.sin_family      = AF_INET;
    address_management.sin_addr.s_addr = INADDR_ANY;
    address_management.sin_port        = htons(opt->management_port);

    int master_tcp_socket = create_master_socket(IPPROTO_TCP, &address_proxy);
    int master_sctp_socket = create_master_socket(IPPROTO_SCTP, &address_management);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_tcp_socket, PENDING_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on TCP port %d \n", opt->port);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_sctp_socket, PENDING_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on SCTP port %d \n", opt->management_port);

    //accept the incoming connection
    puts("Waiting for connections ...");

    close(0);

    const char       *err_msg;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector_pop3";
        goto finally;
    }

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

    const struct fd_handler pop3_handler = {
            .handle_read       = &pop3_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };

    const struct fd_handler management_handler = {
            .handle_read       = &management_accept_connection,
            .handle_write      = NULL,
            .handle_close      = NULL,
    };

    selector_status ss_pop3 = selector_register(selector, master_tcp_socket, &pop3_handler, OP_READ, NULL);

    selector_status ss_manag = selector_register(selector, master_sctp_socket, &management_handler, OP_READ, NULL);

    if(ss_pop3 != SELECTOR_SUCCESS || ss_manag != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    for(;;) {
        err_msg  = NULL;
        ss  = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            break;
        }
    }

    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;

    finally:
    if(ss!= SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                ss_pop3 == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss_pop3));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector!= NULL) {
        selector_destroy(selector);
    }
    selector_close();

    pop3_pool_destroy();

    if(master_tcp_socket >= 0) {
        close(master_tcp_socket);
    }
    return ret;

}