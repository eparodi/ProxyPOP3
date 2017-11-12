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

/**
 * Prints the help
 */
void print_help(){
    printf("Uso: pop3filter [OPTION] <servidor-origen>\n");
    printf("Proxy POP3 que filtra mensajes de <origin-server>.\n");
    printf("\n");
    printf("Opciones:\n");
    printf("%-30s","\t-e archivo-de-error");
    printf("especifica el archivo de error donde se redirecciona stderr de las "
                   "ejecuciones de los filtros\n");
    printf("%-30s", "\t-h");
    printf("imprime la ayuda y termina\n");
    printf("%-30s", "\t-l direccion_pop3");
    printf("establece la dirección donde servirá el proxy\n");
    printf("%-30s", "\t-L direccion_management");
    printf("establece la dirección donde servirá el servicio de management\n");
    printf("%-30s", "\t-m mensaje_de_reemplazo");
    printf("mensaje testigo dejado cuando una parte es reemplazada por el "
                   "filtro\n");
    printf("%-30s", "\t-M media_types_censurables");
    printf("lista de media types censurados\n");
    printf("%-30s", "\t-o puerto_de_management");
    printf("puerto SCTP donde servirá el servicio de management\n");
    printf("%-30s", "\t-p puerto_local");
    printf("puerto TCP donde escuchará conexiones entrantes POP3\n");
    printf("%-30s", "\t-P puerto_origen");
    printf("puerto TCP donde se encuentra el servidor POP3 origen\n");
    printf("%-30s", "\t-t cmd");
    printf("comando utilizado para las transofmraciones externas\n");
    printf("%-30s", "\t-v");
    printf("imprime la versión y termina\n");
}

/**
 * Prints the proxy version
 */
void print_version() {
    printf("POP3 Proxy v%s", parameters->version);
}

void
resolv_addr(char *address, uint16_t port, struct addrinfo **addrinfo);

int
get_user_pass();

void parse_media_types(struct media_types *mt_struct, char *mt_string);

void parse_options(int argc, char **argv) {

    /* Initialize default values */
    parameters                      = malloc(sizeof(*parameters));
    parameters->port                = 1110;
    parameters->error_file          = "/dev/null";
    parameters->management_address  = "127.0.0.1";
    parameters->management_port     = 9090;
    parameters->listen_address      = "0.0.0.0";
    parameters->replacement_msg     = "Parte reemplazada.";
    parameters->origin_port         = 110;
    parameters->et_activated        = false;
    //grep -i -v ^Subject:
    parameters->filter_command      = NULL;
    parameters->version             = "0.0";
    parameters->listenadddrinfo     = 0;
    parameters->managementaddrinfo  = 0;

    parameters->filtered_media_types = new_media_types();

    int index = 0;
    int c;

    opterr = 0;
    int messages = 0;

    if (get_user_pass() < 0){
        fprintf (stderr, "There is a problem with the auth configuration");
        exit(0);
    }

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
                parse_media_types(parameters->filtered_media_types, optarg);
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
                char * cmd = malloc(sizeof(char)*strlen(optarg));
                strcpy(cmd, optarg);
                parameters->filter_command = cmd;
                parameters->et_activated   = true;
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

    resolv_addr(parameters->listen_address, parameters->port,
                &parameters->listenadddrinfo);
    resolv_addr(parameters->management_address, parameters->management_port,
                &parameters->managementaddrinfo);

}

void parse_media_types(struct media_types *mt_struct, char *mt_string) {

}

void
resolv_addr(char *address, uint16_t port, struct addrinfo ** addrinfo) {

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
             port);
    if (0 != getaddrinfo(address, buff, &hints,
                         addrinfo)){
        fprintf(stderr,"Domain name resolution error\n");
    }
}

#define MAX_USER_CHAR 255
#define MAX_PASS_CHAR 255

int
get_user_pass(){
    FILE * f = fopen("secret.txt", "r");
    if (f == NULL)
        return -1;
    parameters->user = malloc(MAX_USER_CHAR * sizeof(char));
    parameters->pass = malloc(MAX_PASS_CHAR * sizeof(char));
    if (parameters->user == NULL || parameters->pass == NULL){
        return -1;
    }
    int n = fscanf(f, "%s\n%s", parameters->user, parameters->pass);
    if (n != 2)
        return -1;
    fclose(f);
    return 1;
}