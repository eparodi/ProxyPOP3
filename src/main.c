#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory.h>
#include <signal.h>
#include <errno.h>
#include "main.h"
#include "selector.h"
#include "pop3.h"

static bool done = false;

/*TODO: help*/
void print_help(){
    printf("HELP");
}

void print_version() {
    printf("POP3 Proxy 1.0");
}

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int main(int argc, char ** argv){

    int port = 1110;

    char *error_file = "/dev/null";
    char *listen_address = INADDR_ANY;
    char *management_address = "127.0.0.1";
    int management_port = 9090;
    char *replacement_msg = "Parte reemplazada.";
    char *filtered_media_types = "text/plain,image/*";
    char *origin_server;
    int origin_port = 110;
    char *filter_command;
    int index = 0;
    int c;

    opterr = 0;

    while ((c = getopt (argc, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1) /* e: option e requires argument e:: optional argument */
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
                if (optopt == 'e' || optopt == 'l' || optopt == 'L') /*TODO: add every option */
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
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

    index = optind;

    if (argc-index == 1){
        origin_server = argv[index];
    }else {
        fprintf(stderr, "Usage: %s [ POSIX style options ] <origin-server>\n", argv[0]);
        return 1;
    }

    close(0);

    const char       *err_msg;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }
    const struct fd_handler pop3 = {
            .handle_read       = pop3_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };
    ss = selector_register(selector, server, &pop3,
                           OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;

    finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                ss == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();

    pop3_pool_destroy();

    if(server >= 0) {
        close(server);
    }
    return ret;
}




