#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "stripmime.h"
#include "MIMEtree.h"

#define FILTER_MEDIAS   "FILTER_MEDIAS"
#define FILTER_MSG      "FILTER_MSG"

int main(int argc, char const *argv[]) {

    char * filter_medias = getenv(FILTER_MEDIAS);

    struct MIMEtree* tree = treeinit();  

    if (filter_medias == NULL) {
        printf("ERROR\n");
        return -1;
    }

    char * fm = malloc(strlen(filter_medias) + 1);
    char * str = fm;
    if (str == NULL) {
        return -1;
    }
    strcpy(str, filter_medias);

    const char *comma = ",", *slash = "/";   
    char *ctx1 = "comma", *ctx2 = "slash";

    char *token;
    char *mime;
    char *type, *subtype;

    /* get the first token */
    token = strtok_r(str, comma, &ctx1);

    /* walk through other tokens */
    while(token != NULL) {
        char * aux = malloc(strlen(token) + 1);
        if (aux == NULL) {
            return -1;
        }
        strcpy(aux, token);

        /* get mime type */
        mime = strtok_r(aux, slash, &ctx2);
        if (mime == NULL) {
            return -1;
        }

        char * type = malloc(strlen(mime) + 1);
        if (type == NULL) {
            return -1;
        }
        strcpy(type, mime);

        /* get mime subtype */
        mime = strtok_r(NULL, slash, &ctx2);
        if (mime == NULL) {
            return -1;
        }

        char * subtype = malloc(strlen(mime) + 1);  
        if (subtype == NULL) {
            return -1;
        }
        strcpy(subtype, mime);

        add(tree, type, subtyoe); 

        printf("'%s'/'%s'\n", type, subtype);
    
        free(aux);
        token = strtok_r(NULL, comma, &ctx1);
    }

    free(fm);

    return stripmime(argc, argv);
}