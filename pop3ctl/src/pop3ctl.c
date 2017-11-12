#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <stdbool.h>
#include <ctype.h>
#include <netdb.h>
#include <limits.h>
#include <errno.h>
#include "pop3ctl.h"
#define MAX_BUFFER 1024

options ctl_parameters;

long parse_port(char * port_name, char *optarg) ;

void resolv_addr() {

    ctl_parameters->managementaddrinfo = 0;

    char mgmt_buff[7];
    snprintf(mgmt_buff, sizeof(mgmt_buff), "%hu",
             ctl_parameters->management_port);


    struct addrinfo hints = {
            .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };

    if (0 != getaddrinfo(ctl_parameters->management_address, mgmt_buff, &hints,
                         &ctl_parameters->managementaddrinfo)){
        fprintf(stderr,"Domain name resolution error\n");
    }
}

options parse_ctl_options(int argc, char **argv) {

    /* Initialize default values */
    ctl_parameters                          = malloc(sizeof(*ctl_parameters));
    ctl_parameters->management_address      = "127.0.0.1";
    ctl_parameters->management_port         = 9090;

    int c;

    opterr = 0;

    /* e: option e requires argument e:: optional argument */
    while ((c = getopt (argc, argv, "L:o:")) != -1){
        switch (c) {
            /* Management listen address */
            case 'L':
                ctl_parameters->management_address = optarg;
                break;
                /* Management SCTP port */
            case 'o':
                ctl_parameters->management_port = (uint16_t) parse_port("Management", optarg);
                break;
            case '?':
                if (optopt == 'L' || optopt == 'o')
                    fprintf (stderr, "Option -%c requires an argument.\n",
                             optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                             "Unknown option character `\\x%x'.\n",
                             optopt);
                exit(1);
            default:
                abort ();
        }
    }

    resolv_addr();

    return ctl_parameters;
}

int
main (int argc, char* argv[]){

    parse_ctl_options(argc,argv);

    int connection_socket, ret;
    //struct sockaddr_in server_address;
    //struct sctp_status status;
    char buffer[MAX_BUFFER + 1];
    size_t datalen = 0;

    connection_socket = socket (ctl_parameters->managementaddrinfo->ai_family,
                                SOCK_STREAM, IPPROTO_SCTP);

    if (connection_socket == -1){
        printf("Socket creation failed\n");
        perror("connection_socket()");
        exit(1);
    }

    ret = connect (connection_socket,
                   ctl_parameters->managementaddrinfo->ai_addr,
                   ctl_parameters->managementaddrinfo->ai_addrlen);

    if (ret == -1){
        fprintf(stderr, "Connection failed\n");
        perror("connect()");
        close(connection_socket);
        exit(1);
    }

    char  recv_buffer[MAX_BUFFER];
    /* Receive hello */
    sctp_recvmsg(connection_socket,
                 (void *) recv_buffer, MAX_BUFFER, NULL, 0, 0, 0);
    printf("%s", recv_buffer);

    while(true){

        fgets(buffer, MAX_BUFFER, stdin);
        /* Clear the newline or carriage return from the end*/
        buffer[strcspn(buffer, "\r\n")] = 0;
        /* Sample input */
        //strncpy (buffer, "Hello Server", 12);
        //buffer[12] = '\0';
        datalen = strlen(buffer);

        ret = sctp_sendmsg(connection_socket, (void *) buffer, datalen,
                           NULL, 0, 0, 0, 0, 0, 0);

        if (ret == 0){
            fprintf(stderr, "Can't send sctp msg");
            close(connection_socket);
            break;
        }


        ret = sctp_recvmsg(connection_socket,
                           (void *) recv_buffer, MAX_BUFFER, NULL, 0, 0, 0);

        if (ret == 0){
            close(connection_socket);
            exit(0);
        }

        recv_buffer[ret] = '\0';

        printf("%s", recv_buffer);

        if (strcmp(recv_buffer,"+OK: Goodbye.\n") == 0){
            close(connection_socket);
            exit(0);
        }
    }
}

long parse_port(char * port_name, char *optarg) {

    char *end     = 0;
    const long sl = strtol(optarg, &end, 10);

    if (end == optarg|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "%s port should be an integer: %s\n", port_name, optarg);
        exit(1);
    }

    return sl;
}