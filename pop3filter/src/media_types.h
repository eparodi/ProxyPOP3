#ifndef TPE_PROTOS_MEDIA_TYPES_H
#define TPE_PROTOS_MEDIA_TYPES_H

#include <unistd.h>
#include <stdbool.h>

typedef struct subtype_node{
    struct subtype_node  * next;
    char              * subtype;
}subtype_node;

typedef struct media_node{
    char              * type;
    struct media_node * next;
    subtype_node      * first;
    subtype_node      * end;
    bool                wildcard;
}media_node;

struct media_types{
    size_t       size;
    media_node * first;
    media_node * end;
};

struct media_types * new_media_types();
int check_media_type(struct media_types * mt, char * type, char * subtype);
int add_media_type(struct media_types * mt, char * type, char * subtype);
char * get_types_list(struct media_types * mt, char separator);
int is_mime(char * str, char ** type, char ** subtype);
int delete_media_type(struct media_types * mt, char * type, char * subtype);

#endif //TPE_PROTOS_MEDIA_TYPES_H
