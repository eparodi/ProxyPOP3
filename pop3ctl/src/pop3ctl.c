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
#include "pop3ctl.h"
#define MAX_BUFFER 1024

options ctl_parameters;

void resolv_addr() {

    ctl_parameters->managementaddrinfo = 0;

    char mgmt_buff[7];
    snprintf(buff, sizeof(mgmt_buff), "%hu",
             ctl_parameters->management_port);


    struct addrinfo hints2 = {
            .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };

    if (0 != getaddrinfo(ctl_parameters->management_address, mgmt_buff, &hints2,
                         &ctl_parameters->managementaddrinfo)){
        sprintf(stderr,"Domain name resolution error\n");
    }
}

options parse_ctl_options(int argc, char **argv) {

    /* Initialize default values */
    ctl_parameters                          = malloc(sizeof(*ctl_parameters));
    ctl_parameters->management_address      = inet_addr("127.0.0.1");
    ctl_parameters->management_port         = 9090;
    ctl_parameters->listen_family           = AF_INET;

    int c;

    opterr = 0;

    /* e: option e requires argument e:: optional argument */
    while ((c = getopt (argc, argv, "L:o:")) != -1){
        switch (c) {
            /* Management listen address */
            case 'L':
                ctl_parameters->management_address = inet_addr(optarg);

                if (ctl_parameters->management_address == INADDR_NONE) {
                    fprintf(stderr, "Invalid management address\n");
                    exit(1);
                }
                break;
                /* Management SCTP port */
            case 'o':
                ctl_parameters->management_port = (uint16_t) atoi(optarg);
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

    return ctl_parameters;
}

int
main (int argc, char* argv[]){

    options opt = parse_ctl_options(argc,argv);

    int connection_socket, ret;
    struct sockaddr_in servaddr;
    struct sctp_status status;
    char buffer[MAX_BUFFER + 1];
    int datalen = 0;

    connection_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    if (connection_socket == -1){
        printf("Socket creation failed\n");
        perror("connection_socket()");
        exit(1);
    }

    bzero ((void *) &servaddr, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(opt->management_port);
    servaddr.sin_addr.s_addr = opt->management_address;

    ret = connect (connection_socket,
                   (struct sockaddr *) &servaddr, sizeof (servaddr));

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
    printf(recv_buffer);

    while(true){

        fgets(buffer, MAX_BUFFER, stdin);
        /* Clear the newline or carriage return from the end*/
        buffer[strcspn(buffer, "\r\n")] = 0;
        /* Sample input */
        //strncpy (buffer, "Hello Server", 12);
        //buffer[12] = '\0';
        datalen = strlen(buffer);

        ret = sctp_sendmsg(connection_socket, (void *) buffer, (size_t) datalen,
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
        printf(recv_buffer);
    }
}