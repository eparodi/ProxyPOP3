#include <stdio.h>
#include <netdb.h>
#include "utils.h"

void print_connection_status(const char * msg, struct sockaddr_storage addr){
    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];

    getnameinfo((struct sockaddr *)&addr,
                sizeof(addr), hoststr, sizeof(hoststr), portstr, sizeof(portstr),
                NI_NUMERICHOST | NI_NUMERICSERV);

    printf("%s: ip %s , port %s \n",
           msg,
           hoststr,
           portstr);
}