#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <errno.h>
#include <ctype.h>

#define TRUE   1
#define FALSE  0
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1025

/**
 * creates a tcp socket connection to the pop3 origin server
 * @param origin_server ip of the origin server
 * @param origin_port port of the origin server
 * @return the file descriptor of the created socket
 */
int create_server_socket(char * origin_server, int origin_port) {

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    /* Construct the server address structure */
    struct sockaddr_in servAddr;            /* Server address */
    memset(&servAddr, 0, sizeof(servAddr)); /* Zero out structure */
    servAddr.sin_family = AF_INET;          /* IPv4 address family */
    // Convert address
    int rtnVal = inet_pton(AF_INET, origin_server, &servAddr.sin_addr.s_addr);
    if (rtnVal == 0) {
        perror("inet_pton() failed - invalid address string");
        exit(EXIT_FAILURE);
    } else if (rtnVal < 0) {
        perror("inet_pton() failed");
        exit(EXIT_FAILURE);
    }
    servAddr.sin_port = htons(origin_port);    /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("Connection to origin server failed");
        exit(EXIT_FAILURE);
    }

    return sock;
}

/* TODO: help */
/**
 * Prints the help
 */
void print_help() {
    printf("HELP");
}

/**
 * Prints the proxy version
 */
void print_version() {
    printf("POP3 Proxy 1.0");
}

int add_to_set(fd_set *readfds, int max_sd, const int sockets[]) {

    for (int i = 0 ; i < MAX_CLIENTS ; i++) {
        // socket descriptor
        int sd = sockets[i];

        // if valid socket descriptor then add to read list
        if (sd > 0) {
            FD_SET(sd, readfds);
        }

        // highest file descriptor number, need it for the select function
        if (sd > max_sd) {
            max_sd = sd;
        }
    }

    return max_sd;
}

int main (int argc, char ** argv) {

    int port = 1110;
    char *error_file            = "/dev/null";
    char *listen_address        = INADDR_ANY;
    char *management_address    = "127.0.0.1";
    int management_port         = 9090;
    char *replacement_msg       = "Parte reemplazada.";
    char *filtered_media_types  = "text/plain,image/*";
    char *origin_server;
    int origin_port             = 110;
    char *filter_command;
    int index                   = 0;
    int c;

    opterr = 0;

    /* e: option e requires argument e:: optional argument */
    while ((c = getopt (argc, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1){
        switch (c)
        {
            /* Error file */
            case 'e':
                error_file = optarg;
                break;
                /* Print help and quit */
            case 'h':
                print_help();
                exit(0);
                break;
                /* Listen address */
            case 'l':
                listen_address = optarg;
                break;
                /* Management listen address */
            case 'L':
                management_address = optarg;
                break;
                /* Replacement message */
            case 'm':
                replacement_msg = optarg;
                break;
            case 'M':
                filtered_media_types = optarg;
                break;
                /* Management SCTP port */
            case 'o':
                management_port = atoi(optarg);
                break;
                /* proxy port */
            case 'p':
                port = atoi(optarg);
                break;
                /* pop3 server port*/
            case 'P':
                origin_port = atoi(optarg);
                break;
            case 't': {
                filter_command = optarg;
            }
                break;
            case 'v':
                print_version();
                exit(0);
                break;
            case '?':
                /* TODO: add every option */
                if (optopt == 'e' || optopt == 'l' || optopt == 'L')
                    fprintf (stderr, "Option -%c requires an argument.\n",
                             optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                             "Unknown option character `\\x%x'.\n",
                             optopt);
                return 1;
            default:
                abort ();
        }
    }

    index = optind;

    if (argc-index == 1){
        origin_server = argv[index];
    }else {
        fprintf(stderr, "Usage: %s [ POSIX style options ] <origin-server>\n",
                argv[0]);
        return 1;
    }

    int opt         = TRUE;
    int max_clients = MAX_CLIENTS;
    // initialise all client_socket[] and server_socket[] to 0 so not checked
    int client_socket[MAX_CLIENTS] = {0};
    int server_socket[MAX_CLIENTS] = {0};
    int max_sd;
    struct sockaddr_in address;

    char buffer[BUFFER_SIZE];  //data buffer of 1K

    // set of socket descriptors
    fd_set readfds;

    // a message
    char *message = "POP3 Proxy v1.0 \r\n";

    int master_socket;

    // create a master socket
    if ((master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections
    // this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons( port );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d \n", port);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    int addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(TRUE)
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
        max_sd = add_to_set(&readfds, max_sd, client_socket);
        max_sd = add_to_set(&readfds, max_sd, server_socket);

        // Wait for an activity on one of the sockets. Timeout is NULL, so
        // wait indefinitely
        int activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)) {
            printf("select error");
        }

        // If something happened on the master socket, then its an incoming
        // connection
        if (FD_ISSET(master_socket, &readfds)) {
            int new_socket;
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address,
                                     (socklen_t*)&addrlen))<0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection, socket fd is %d , ip is : %s , port : %d \n",
                   new_socket,
                   inet_ntoa(address.sin_addr),
                   ntohs(address.sin_port));

            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) !=
                    strlen(message) ) {
                perror("send");
            }

            puts("Welcome message sent successfully");

            //add new socket to array of sockets
            for (int i = 0; i < max_clients; i++) {
                //if position is empty
                if ( client_socket[i] == 0 ) {
                    client_socket[i] = new_socket;
                    printf("Adding to list of client sockets as %d\n" , i);
                    //Create pop3 socket
                    if ((server_socket[i] = create_server_socket(
                            origin_server, origin_port)) == 0){
                        perror("socket failed");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
            }
        }

        //else its some IO operation on some other socket :)
        for (int i = 0; i < max_clients; i++) {
            int server_sd = server_socket[i];
            int client_sd = client_socket[i];
            ssize_t valread;

            // if a client descriptor was set
            if (FD_ISSET( client_sd , &readfds)) {
                //Check if it was for closing and also read the incoming message
                if ((valread = read( client_sd , buffer, BUFFER_SIZE)) == 0) {

                    //Somebody disconnected , get his details and print
                    getpeername(client_sd , (struct sockaddr*)&address,
                                (socklen_t*)&addrlen);
                    printf("Client disconnected , ip %s , port %d \n",
                           inet_ntoa(address.sin_addr),
                           ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close( client_sd );
                    close(server_sd);
                    client_socket[i] = 0;
                    server_socket[i] = 0;
                }

                    //Send to server
                else {
                    // set the string terminating NULL byte on the end of the
                    // data read
                    buffer[valread] = '\0';

                    send(server_sd, buffer, strlen(buffer), 0);

                }
                // if a server descriptor was set
            } else if (FD_ISSET( server_sd , &readfds)) {

                if ((valread = read(server_sd, buffer, BUFFER_SIZE)) == 0) {
                    //Server disconnected , get his details and print
                    getpeername(server_sd, (struct sockaddr *) &address,
                                (socklen_t *) &addrlen);
                    printf("Server disconnected , ip %s , port %d \n",
                           inet_ntoa(address.sin_addr),
                           ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close(client_sd);
                    close(server_sd);
                    client_socket[i] = 0;
                    server_socket[i] = 0;
                }
                    //Send to client
                else {
                    // set the string terminating NULL byte on the
                    // end of the data read
                    buffer[valread] = '\0';
                    send(client_sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
}