#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <memory.h>
#include <netdb.h>

#include "parameters.h"
#include "media_types.h"

// Global variable with the parameters
options parameters;

void print_help();
void print_version();

/* TODO: help */
/**
 * Prints the help
 */
void print_help(){
    printf("HELP");
}

/**
 * Prints the proxy version
 */
void print_version() {
    printf("POP3 Proxy 1.0");
}

void resolv_addr();

options parse_options(int argc, char **argv) {

    /* Initialize default values */
    parameters = malloc(sizeof(*parameters));
    parameters->port = 1110;
    parameters->error_file = "/dev/null";
    parameters->management_address = "127.0.0.1";
    parameters->management_port = 9090;
    parameters->listen_address = "0.0.0.0";
    parameters->replacement_msg = "Parte reemplazada.";
    parameters->origin_port = 110;
    parameters->filter_command = "echo hola"; //TODO: pasarlo a Null
    parameters->version = "0.0";

    char * type = malloc(10*sizeof(char));
    char * subtype = malloc(10*sizeof(char));
    struct media_types * mt = new_media_types();
    strcpy(type, "text");
    strcpy(subtype, "plain");
    add_media_type(mt, type, subtype);
    type = malloc(10*sizeof(char));
    subtype = malloc(10*sizeof(char));
    strcpy(type, "image");
    strcpy(subtype, "*");
    add_media_type(mt, type, subtype);
    parameters->filtered_media_types = mt;

    int index = 0;
    int c;

    opterr = 0;
    int messages = 0;

    /* e: option e requires argument e:: optional argument */
    while ((c = getopt (argc, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1){
        switch (c) {
            /* Error file */
            case 'e':
                parameters->error_file = optarg;
                break;
                /* Print help and quit */
            case 'h':
                print_help();
                exit(0);
                break;
                /* Listen address */
            case 'l':
                parameters->listen_address = optarg;
                break;
                /* Management listen address */
            case 'L':
                parameters->management_address = optarg;
                break;
                /* Replacement message */
            case 'm':
                messages++;
                if (messages == 1){
                    parameters->replacement_msg = optarg;
                }else{
                    strcat(parameters->replacement_msg, "\n");
                    strcat(parameters->replacement_msg, optarg);
                }

                break;
            case 'M':
                parameters->filtered_media_types = optarg;
                break;
                /* Management SCTP port */
            case 'o':
                parameters->management_port = (uint16_t) atoi(optarg);
                break;
                /* proxy port */
            case 'p':
                parameters->port = (uint16_t) atoi(optarg);
                break;
                /* pop3 server port*/
            case 'P':
                parameters->origin_port = (uint16_t) atoi(optarg);
                break;
                /* filter command */
            case 't': {
                parameters->filter_command = optarg;
            }
                break;
            case 'v':
                print_version();
                exit(0);
                break;
            case '?':
                if (optopt == 'e' || optopt == 'l' || optopt == 'L'
                    || optopt == 'm' || optopt == 'M' || optopt == 'o'
                    || optopt == 'p' || optopt == 'P' || optopt == 'v')
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

    index = optind;

    if (argc-index == 1){
        parameters->origin_server = argv[index];
    }else {
        fprintf(stderr, "Usage: %s [ POSIX style options ] <origin-server>\n",
                argv[0]);
        exit(1);
    }

    resolv_addr();

    return parameters;
}

void resolv_addr() {

    parameters->managementaddrinfo = 0;
    parameters->listenadddrinfo = 0;


    struct addrinfo hints = {
            .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%hu",
             parameters->port);
    if (0 != getaddrinfo(parameters->listen_address, buff, &hints,
                         &parameters->listenadddrinfo)){
        sprintf(stderr,"Domain name resolution error\n");
    }

    char mgmt_buff[7];
    snprintf(buff, sizeof(mgmt_buff), "%hu",
             parameters->management_port);


    struct addrinfo hints2 = {
            .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
            .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
            .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
            .ai_protocol  = 0,            /* Any protocol */
            .ai_canonname = NULL,
            .ai_addr      = NULL,
            .ai_next      = NULL,
    };

    if (0 != getaddrinfo(parameters->management_address, mgmt_buff, &hints2,
                         &parameters->managementaddrinfo)){
        sprintf(stderr,"Domain name resolution error\n");
    }
}

options get_parameters(){
    return parameters;
}