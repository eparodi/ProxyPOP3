#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ctype.h>

#include "parameters.h"

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

options parse_options(int argc, char **argv) {

    /* Initialize default values */
    parameters = malloc(sizeof(*parameters));
    parameters->port = 1110;
    parameters->error_file = "/dev/null";
    parameters->management_address = "127.0.0.1";
    parameters->management_port = 9090;
    parameters->listen_address = INADDR_ANY;
    parameters->replacement_msg = "Parte reemplazada.";
    parameters->filtered_media_types = "text/plain,image/*";
    parameters->origin_port = 110;

    int index = 0;
    int c;

    opterr = 0;

    /* e: option e requires argument e:: optional argument */
    while ((c = getopt (argc, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1){
        switch (c)
        {
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
                parameters->replacement_msg = optarg;
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
            case 't': {
                parameters->filter_command = optarg;
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

    return parameters;
}

options get_parameters(){
    return parameters;
}