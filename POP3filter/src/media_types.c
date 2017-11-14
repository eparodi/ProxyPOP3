#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include "media_types.h"

#define STR_GROWTH 40

struct media_types * new_media_types(){
    struct media_types * ret = malloc(sizeof(struct media_types));
    if (ret == NULL)
        return NULL;
    ret->first = NULL;
    ret->end   = NULL;
    ret->size  = 0;
    return ret;
}

void empty_type(media_node * n);

int delete_type(struct media_types *mt, char *type);

int check_media_type(struct media_types * mt, char * type, char * subtype){
    media_node * node = mt->first;
    while(node != NULL){
        if (strcasecmp(node->type, type) == 0)
            break;
        node = node->next;
    }
    if (node != NULL){
        subtype_node * n = node->first;
        while (n != NULL){
            if (strcasecmp(n->subtype, subtype) == 0)
                return 1;
            n = n->next;
        }
    }
    return 0;
}

int add_media_type(struct media_types * mt, char * type, char * subtype){
    media_node * node = mt->first;
    while(node != NULL){
        if (strcasecmp(node->type, type) == 0)
            break;
        node = node->next;
    }
    if (node == NULL){
        node = malloc(sizeof(struct media_node));
        if (node == NULL){
            return -1;
        }
        node->type = type;
        node->next = NULL;
        node->first = NULL;
        node->end = NULL;
        node->wildcard = false;
        if (mt->first == NULL){
            mt->first = node;
            mt->end = node;
        }else{
            mt->end->next = node;
            mt->end = node;
        }
    }else if(node->wildcard){
        return -1;
    }

    if (strcasecmp(subtype, "*") == 0){
        empty_type(node);
        node->wildcard = true;
    }

    subtype_node * n = node->first;

    while(n != NULL){
        if (strcasecmp(n->subtype, subtype) == 0)
            return -1;
        n = n->next;
    }

    n = malloc(sizeof(subtype_node));
    if (n == NULL){
        if (node->first == NULL)
            delete_type(mt, type);
        return -1;
    }
    n->subtype = subtype;
    n->next = NULL;
    if (node->first == NULL){
        node->first = n;
        node->end = n;
    }else{
        node->end->next = n;
        node->end = n;
    }
    return 1;
}

int delete_type(struct media_types *mt, char *type){
    media_node * node = mt->first;
    media_node * aux  = NULL;
    media_node * prev = NULL;
    while(node != NULL){
        aux = node->next;
        if (strcasecmp(node->type, type) == 0){
            if (aux == NULL){
                mt->end = NULL;
            }
            if (prev == NULL){
                mt->first = aux;
            }
            free(node->type);
            free(node);
        }
        prev = node;
        node = aux;
    }
    return 1;
}

int delete_media_type(struct media_types * mt, char * type, char * subtype){
    media_node * node = mt->first;
    while(node != NULL){
        if (strcasecmp(node->type, type) == 0){
            break;
        }
        node = node->next;
    }
    if (node == NULL)
        return -1;
    subtype_node * n = node->first;
    subtype_node * a = NULL;
    subtype_node * p = NULL;
    while(n != NULL){
        a = n->next;
        if (strcasecmp(n->subtype, subtype) == 0) {
            if (a == NULL){
                node->end = NULL;
            }
            if (p == NULL){
                node->first = a;
            }
            if (strcmp("*", subtype) == 0)
                node->wildcard = false;
            free(n->subtype);
            free(n);
            return 1;
        }
        n = a;
        p = n;
    }
    return -1;
}

void empty_type(media_node * n){
    subtype_node * node = n->first;
    subtype_node * aux = NULL;
    while(node != NULL){
        aux = node->next;
        free(node->subtype);
        free(node);
        node = aux;
    }
    n->first = NULL;
    n->end   = NULL;
}

void delete_media_types(struct media_types * mt){
    media_node * node = mt->first;
    media_node * aux  = NULL;
    while(node != NULL){
        empty_type(node);
        aux = node->next;
        free(node->type);
        free(node);
        node = aux;
    }
    free(mt);
}

char * get_types_list(struct media_types * mt, char separator){
    char * str = malloc(STR_GROWTH * sizeof(char));
    size_t size = STR_GROWTH;
    size_t index = 0;
    media_node * node = mt->first;
    while(node != NULL){
        subtype_node * n = node->first;
        size_t type_length = strlen(node->type);
        while (n != NULL){
            size_t subtype_length = strlen(n->subtype);
            if (size <= index + type_length + subtype_length + 2){
                size_t growth = ((size - index) / STR_GROWTH + 2) * STR_GROWTH;
                void * aux = realloc(str, size + growth);
                if (aux == NULL)
                    return NULL;
                str = aux;
                size += growth;
            }
            strcpy(str + index, node->type);
            index += type_length;
            str[index++] = '/';
            strcpy(str + index, n->subtype);
            index += subtype_length;
            str[index++] = separator;
            n = n->next;
        }
        node = node->next;
    }
    if (index != 0)
        str[index-1] = '\0';
    else
        str[index] = '\0';
    return str;
}

int is_mime(char * str, char ** type, char ** subtype){
    bool slash = false;
    *type = str;
    for (int i = 0; str[i] != 0; i++){
        if (str[i] == '/' && !slash){
            str[i] = '\0';
            slash = true;
            if (str[i+1] != 0)
                *subtype = str+i+1;
            else
                return -1;
        }else if(str[i] == '/' && slash){
            return -1;
        }
    }
    if (slash)
        return 1;
    else
        return -1;
}