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

#define PENDING_CONNECTIONS 10
#define BUFFER_SIZE 1025

/**
 * Source: http://www.binarytides.com/multiple-socket-connections-fdset-select-linux/
 */

/**
 * creates a tcp socket connection to the pop3 origin server
 * @param origin_server ip of the origin server
 * @param origin_port port of the origin server
 * @return the file descriptor of the created socket
 */
//int create_server_socket(char * origin_server, int origin_port) {
//
//    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
//
//    if (sock < 0) {
//        perror("socket() failed");
//        exit(EXIT_FAILURE);
//    }
//
//    /* Construct the server address structure */
//    struct sockaddr_in servAddr;            /* Server address */
//    memset(&servAddr, 0, sizeof(servAddr)); /* Zero out structure */
//    servAddr.sin_family = AF_INET;          /* IPv4 address family */
//    // Convert address
//    int rtnVal = inet_pton(AF_INET, origin_server, &servAddr.sin_addr.s_addr);
//    if (rtnVal == 0) {
//        perror("inet_pton() failed - invalid address string");
//        exit(EXIT_FAILURE);
//    } else if (rtnVal < 0) {
//        perror("inet_pton() failed");
//        exit(EXIT_FAILURE);
//    }
//    servAddr.sin_port = htons(origin_port);    /* Server port */
//
//    /* Establish the connection to the echo server */
//    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
//        perror("Connection to origin server failed");
//        exit(EXIT_FAILURE);
//    }
//
//    return sock;
//}


//int add_to_set(fd_set *readfds, int max_sd, const int sockets[], int max_clients) {
//
//    for (int i = 0 ; i < max_clients ; i++) {
//        // socket descriptor
//        int sd = sockets[i];
//
//        // if valid socket descriptor then add to read list
//        if (sd > 0) {
//            FD_SET(sd, readfds);
//        }
//
//        // highest file descriptor number, need it for the select function
//        if (sd > max_sd) {
//            max_sd = sd;
//        }
//    }
//
//    return max_sd;
//}

//void end_connection(struct sockaddr_in address, int client, int sd,
//                    int *client_socket,
//                    int *server_socket) {
//    int addrlen = sizeof(address);
//    //Somebody disconnected , get his details and print
//    getpeername(sd, (struct sockaddr*)&address,
//                (socklen_t*)&addrlen);
//    printf("Client disconnected , ip %s , port %d \n",
//           inet_ntoa(address.sin_addr),
//           ntohs(address.sin_port));
//
//    //Close the socket and mark as 0 in list for reuse
//    close(sd);
//    close(server_socket[client]);
//    client_socket[client] = 0;
//    server_socket[client] = 0;
//}

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

    printf("Listening on STCP port %d \n", opt->management_port);

    //accept the incoming connection
    puts("Waiting for connections ...");

    close(0);

    const char       *err_msg;
    selector_status   ss_pop3      = SELECTOR_SUCCESS;
    fd_selector selector_pop3      = NULL;
    selector_status   ss_manag     = SELECTOR_SUCCESS;
    fd_selector selector_manag     = NULL;

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

    selector_pop3 = selector_new(1024);
    if(selector_pop3 == NULL) {
        err_msg = "unable to create selector_pop3";
        goto finally;
    }

    selector_manag = selector_new(1024);
    if(selector_manag == NULL) {
        err_msg = "unable to create selector_pop3";
        goto finally;
    }
    const struct fd_handler pop3_handler = {
            .handle_read       = &pop3_accept_connection,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };

    const struct fd_handler management_handler = {
            .handle_read       = NULL,
            .handle_write      = NULL,
            .handle_close      = NULL,
    };

    ss_pop3 = selector_register(selector_pop3, master_tcp_socket, &pop3_handler, OP_READ, NULL);

    ss_manag = selector_register(selector_manag, master_sctp_socket, &management_handler, OP_READ, NULL);

    if(ss_pop3 != SELECTOR_SUCCESS || ss_manag != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    for(;;) {
        err_msg  = NULL;
        ss_pop3  = selector_select(selector_pop3);
        ss_manag = selector_select(selector_manag);
        if(ss_pop3 != SELECTOR_SUCCESS || ss_manag != SELECTOR_SUCCESS) {
            err_msg = "serving";
            break;
        }
    }

    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;

    finally:
    if(ss_pop3 != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                ss_pop3 == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss_pop3));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector_pop3 != NULL) {
        selector_destroy(selector_pop3);
    }
    selector_close();

    if(master_tcp_socket >= 0) {
        close(master_tcp_socket);
    }
    return ret;

//    while(true) {
//
//        //clear the socket set
//        FD_ZERO(&readfds);
//        FD_ZERO(&sctpfds);
//
//        //add master socket to set
//        FD_SET(master_tcp_socket, &readfds);
//        int max_sd = master_tcp_socket;
//        max_sd = add_to_set(&readfds, max_sd, client_socket, max_clients);
//        max_sd = add_to_set(&readfds, max_sd, server_socket, max_clients);
//
//        // Wait for an activity on one of the sockets. Timeout is NULL, so
//        // wait indefinitely
//        struct timespec waitd = {0, 1000000};          // the max wait time for an event
//
//        int activity1 = pselect( max_sd + 1 , &readfds , NULL , NULL , &waitd, NULL);
//
//        if ((activity1 < 0) && (errno!=EINTR)) {
//            printf("select error");
//        }
//
//        FD_SET(master_sctp_socket, &sctpfds);
//        int max_sd_sctp = master_sctp_socket;
//        max_sd_sctp = add_to_set(&sctpfds, max_sd_sctp, sctp_socket, max_clients);
//
//        int activity2 = pselect( max_sd_sctp + 1 , &sctpfds , NULL , NULL , &waitd, NULL);
//
//        if ((activity2 < 0) && (errno!=EINTR)) {
//            printf("select error");
//        }
//
//        // If something happened on the master socket, then its an incoming
//        // connection
//        if (FD_ISSET(master_tcp_socket, &readfds)) {
//            int new_socket;
//            if ((new_socket = accept(master_tcp_socket, (struct sockaddr *)&address_proxy,
//                                     (socklen_t*)&addrlen))<0) {
//                perror("accept");
//                exit(EXIT_FAILURE);
//            }
//
//            //inform user of socket number - used in send and receive commands
//            printf("New TCP connection, socket fd is %d , ip is : %s , port : %d \n",
//                   new_socket,
//                   inet_ntoa(address_proxy.sin_addr),
//                   ntohs(address_proxy.sin_port));
//
//            //send new connection greeting message
//            if( send(new_socket, message, strlen(message), 0) !=
//                    strlen(message) ) {
//                perror("send");
//            }
//
//            puts("Welcome message sent successfully");
//
//            //add new socket to array of sockets
//            for (int i = 0; i < max_clients; i++) {
//                //if position is empty
//                if ( client_socket[i] == 0 ) {
//                    client_socket[i] = new_socket;
//                    printf("Adding to list of client sockets as %d\n" , i);
//                    //Create pop3 socket
//                    if ((server_socket[i] = create_server_socket(
//                            opt->origin_server, opt->origin_port)) == 0){
//                        perror("socket failed");
//                        exit(EXIT_FAILURE);
//                    }
//                    break;
//                }
//            }
//        }
//
//        if (FD_ISSET(master_sctp_socket, &sctpfds)){
//            int new_socket;
//            if ((new_socket = accept(master_sctp_socket, (struct sockaddr *)&address_management,
//                                     (socklen_t*)&addrlen))<0) {
//                perror("accept");
//                exit(EXIT_FAILURE);
//            }
//
//            //inform user of socket number - used in send and receive commands
//            printf("New SCTP connection, socket fd is %d , ip is : %s , port : %d \n",
//                   new_socket,
//                   inet_ntoa(address_management.sin_addr),
//                   ntohs(address_management.sin_port));
//
//            //send new connection greeting message
//            if( send(new_socket, message, strlen(message), 0) !=
//                strlen(message) ) {
//                perror("send");
//            }
//
//            puts("Welcome message sent successfully");
//
//            //add new socket to array of sockets
//            for (int i = 0; i < max_clients; i++) {
//                //if position is empty
//                if ( sctp_socket[i] == 0 ) {
//                    sctp_socket[i] = new_socket;
//                    printf("Adding to list of client sockets as %d\n" , i);
//                    break;
//                }
//            }
//        }
//
//        //else its some IO operation on some other socket :)
//        for (int i = 0; i < max_clients; i++) {
//            int server_sd = server_socket[i];
//            int client_sd = client_socket[i];
//            int sctp_sd = sctp_socket[i];
//            ssize_t valread;
//
//            // if a client descriptor was set
//            if (FD_ISSET(client_sd, &readfds)) {
//                //Check if it was for closing and also read the incoming message
//                if ((valread = read(client_sd, buffer, BUFFER_SIZE)) == 0) {
//                    end_connection(address_proxy, i, client_sd, client_socket,
//                                   server_socket);
//                }
//                    //Send to server
//                else {
//                    // set the string terminating NULL byte on the end of the
//                    // data read
//                    buffer[valread] = '\0';
//                    send(server_sd, buffer, strlen(buffer), 0);
//                }
//                // if a server descriptor was set
//            } else if (FD_ISSET(server_sd, &readfds)) {
//
//                if ((valread = read(server_sd, buffer, BUFFER_SIZE)) == 0) {
//                    end_connection(address_proxy, i, server_sd, client_socket,
//                                   server_socket);
//                }
//                    //Send to client
//                else {
//                    // set the string terminating NULL byte on the
//                    // end of the data read
//                    buffer[valread] = '\0';
//                    send(client_sd, buffer, strlen(buffer), 0);
//                }
//            } else if (FD_ISSET(sctp_sd, &sctpfds)){
//                if ((valread = read(sctp_sd, buffer, BUFFER_SIZE)) == 0) {
//                    printf("Client disconnected , ip %s , port %d \n",
//                           inet_ntoa(address_management.sin_addr),
//                           ntohs(address_management.sin_port));
//                    close(sctp_sd);
//                }
//                    //Send to client
//                else {
//                    // set the string terminating NULL byte on the
//                    // end of the data read
//                    buffer[valread] = '\0';
//                    send(sctp_sd, buffer, strlen(buffer), 0);
//                }
//            }
//        }
//    }
}