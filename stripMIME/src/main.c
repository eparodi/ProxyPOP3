#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stripmime.h"

#define FILTER_MEDIAS 	"FILTER_MEDIAS"
#define FILTER_MSG 		"FILTER_MSG"

int main(int argc, char const *argv[]) {

    char *filter_medias = getenv(FILTER_MEDIAS);

    struct Tree *tree = tree_init();

    if (tree == NULL)
        return -1;

    if (filter_medias == NULL) {
        fprintf(stderr, "-ERR No filter medias specified.\r\n");
        free(tree);
        return -1;
    }

    char *filter_msg = getenv(FILTER_MSG);

    if (filter_msg == NULL) {
        filter_msg = "Parte reemplazada.";
    }

//    FILE *f = freopen("../mails/64kb:2,S", "r", stdin);
//    if (f == NULL) {
//        free(tree);
//        return -1;
//    }

    char *fm = malloc(strlen(filter_medias) + 1);
    char *str = fm;
    if (str == NULL) {
        free(tree);
//        free(f);
        return -1;
    }
    strcpy(str, filter_medias);

    const char *comma = ",", *slash = "/";
    char *ctx1 = "comma", *ctx2 = "slash";

    char *token;
    char *mime;

    /* get the first token */
    token = strtok_r(str, comma, &ctx1);

    bool error = false;

    /* walk through other tokens */
    while (token != NULL) {
        char *aux = malloc(strlen(token) + 1);
        if (aux == NULL) {
            error = true;
            break;
        }
        strcpy(aux, token);

        /* get mime type */
        mime = strtok_r(aux, slash, &ctx2);
        if (mime == NULL) {
            free(aux);
            error = true;
            break;
        }

        char *type = malloc(strlen(mime) + 1);
        if (type == NULL) {
            free(aux);
            error = true;
            break;
        }
        strcpy(type, mime);

        /* get mime subtype */
        mime = strtok_r(NULL, slash, &ctx2);
        if (mime == NULL) {
            free(aux);
            free(type);
            error = true;
            break;
        }

        char *subtype = malloc(strlen(mime) + 1);
        if (subtype == NULL) {
            free(aux);
            free(type);
            error = true;
            break;
        }
        strcpy(subtype, mime);

        enum add_status status = addNode(tree, type, subtype);

        switch (status) {
            case ok:
                break;
            case err:
            case both:
                free(type);
                free(subtype);
                break;
            case typ:
                free(type);
                break;
            case sub:
                free(subtype);
                break;
        }

        free(aux);
        token = strtok_r(NULL, comma, &ctx1);
    }

    free(fm);
    //free(f);

    if (!error) {
        return stripmime(tree, filter_msg);
    }
    else {
        mime_parser_destroy(tree);
    }

	return 0;
}