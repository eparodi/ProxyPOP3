#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close

#include "main.h"

#define TRUE   1
#define FALSE  0
#define PORT 8888
#define POP3_PORT 110

int parseArguments(int, char **, options *);

int pop3_connection(options opt) {

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    //fcntl(sock, F_SETFL, O_NONBLOCK); // set this little pretty socket to not blocking.

    if (sock < 0) {
        printf("socket() failed\n");
        exit(-1);
    }

    // Construct the server address structure
    struct sockaddr_in servAddr;            // Server address
    memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
    servAddr.sin_family = AF_INET;          // IPv4 address family
    // Convert address
    int rtnVal = inet_pton(AF_INET, "127.0.0.1", &servAddr.sin_addr.s_addr);
    if (rtnVal == 0) {
        printf("inet_pton() failed - invalid address string\n");
        return -1;
    } else if (rtnVal < 0) {
        printf("inet_pton() failed\n");
        return -1;
    }
    servAddr.sin_port = htons(opt.pop3_port);    // Server port

    // Establish the connection to the echo server
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        printf("connect() failed\n");
        return -1;
    }

    return sock;
}

size_t send_to_server(int server_socket, char * cmd) {

    size_t echoStringLen = strlen(cmd); // Determine input length

    // Send the string to the server
    ssize_t numBytes = send(server_socket, cmd, echoStringLen, 0);

    if (numBytes < 0)
        printf("send() failed\n");
    else if (numBytes != echoStringLen)
        printf("sent unexpected number of bytes\n");

    fputc('\n', stdout); // Print a final linefeed

    return echoStringLen;
}

void receive (int pop3_socket, size_t echoStringLen, char * buffer){
    // Receive the same string back from the server
    unsigned int totalBytesRcvd = 0; // Count of total bytes received
    fputs("Received: ", stdout);     // Setup to print the echoed string
    ssize_t numBytes;
    while (totalBytesRcvd < echoStringLen) {
        /* Receive up to the buffer size (minus 1 to leave space for
        a null terminator) bytes from the sender */
        numBytes = recv(pop3_socket, buffer, 1025 - 1, 0);
    if (numBytes < 0)
        printf("recv() failed\n");
    else if (numBytes == 0)
        printf("recv() - connection closed prematurely\n");
        totalBytesRcvd += numBytes; // Keep tally of total bytes
        buffer[numBytes] = '\0';    // Terminate the string!
    }
}

int main(int argc , char *argv[])
{
    int sock_opt = TRUE;
    int master_socket , addrlen , new_socket , client_socket[30] , max_clients = 30 , activity, i, sd;
    ssize_t valread;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025];  //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;

    //a message
    char *message = "POP3 Proxy v1.0 \r\n";

    options opt;
    opt.pop3_ip = inet_addr("127.0.0.1");
    opt.pop3_port = POP3_PORT;
    opt.port = PORT;

    if (parseArguments(argc, argv, &opt) < 0) {
        exit(-1);
    }

    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , IPPROTO_SCTP)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&sock_opt, sizeof(sock_opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(TRUE)
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++)
        {
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);

            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR))
        {
            printf("select error");
        }

        int pop3_socket;

        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

            //create pop3 connection
            pop3_socket = pop3_connection(opt);

            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) )
            {
                perror("send");
            }

            puts("Welcome message sent successfully");

            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++)
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);

                    break;
                }
            }
        }

        //else its some IO operation on some other socket :)
        for (i = 0; i < max_clients; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET( sd , &readfds))
            {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, 1024)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }

                //Echo back the message that came in
                else
                {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    size_t bytesSent = send_to_server(pop3_socket, buffer); // send client cmd to server
                    char response_buffer[1025];
                    receive(pop3_socket, bytesSent,response_buffer); // receive response from server
                    send(sd , response_buffer, strlen(response_buffer) , 0 ); // send response to client
                }
            }
        }
    }
}


// TODO: buscar parser libreria
int parseArguments(int argc, char **argv, options *opt) {

    return 1;
}