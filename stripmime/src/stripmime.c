#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>

#include "mime_type.h"
#include "parser_utils.h"
#include "pop3_multi.h"
#include "mime_chars.h"
#include "mime_msg.h"
#include "stripmime.h"
#include "frontier.h"

/*
 * imprime información de debuging sobre un evento.
 *
 * @param p        prefijo (8 caracteres)
 * @param namefnc  obtiene el nombre de un tipo de evento
 * @param e        evento que se quiere imprimir
 */
static void
debug(const char *p,
      const char * (*namefnc)(unsigned),
      const struct parser_event* e) {
    // DEBUG: imprime
    if (e->n == 0) {
        fprintf(stderr, "%-8s: %-14s\n", p, namefnc(e->type));
    } else {
        for (int j = 0; j < e->n; j++) {
            const char* name = (j == 0) ? namefnc(e->type)
                                        : "";
            if (e->data[j] <= ' ') {
                fprintf(stderr, "%-8s: %-14s 0x%02X\n", p, name, e->data[j]);
            } else {
                fprintf(stderr, "%-8s: %-14s %c\n", p, name, e->data[j]);
            }
        }
    }
}

/* mantiene el estado durante el parseo */
struct ctx {
    /* delimitador respuesta multi-línea POP3 */
    struct parser* multi;
    /* delimitador mensaje "tipo-rfc 822" */
    struct parser* msg;
    /* detector de field-name "Content-Type" */
    struct parser* ctype_header;

    struct parser* filtered_msg;

    struct Tree* mime_tree;

    struct TreeNode* subtype;

    struct parser* boundary;

    struct Frontier* boundary_frontier;

    /* ¿hemos detectado si el field-name que estamos procesando refiere
     * a Content-Type?. Utilizando dentro msg para los field-name.
     */
    bool           *frontier_end_detected;
    bool           *frontier_detected;
    bool           *msg_content_type_field_detected;
    bool           *filtered_msg_detected;
    bool           *boundary_detected;
    bool           *multipart_detected;
};


void mime_parser_destroy(struct Tree *mime_tree){
    struct TreeNode* node = mime_tree->first;
    struct TreeNode* children;
    struct TreeNode* tmp;
    while(node != NULL){
        children = node->children;
        while(children != NULL){
            tmp = children;
            if(children->parser != NULL){
                parser_destroy(children->parser);
              parser_utils_strcmpi_destroy(children->def);
            }
            children = children->next;
            free(tmp);
        }
        tmp = node;
        parser_destroy(node->parser);
        parser_utils_strcmpi_destroy(node->def);
        node = node->next;
        free(tmp);
    }
    free(mime_tree);
}

static bool T = true;
static bool F = false;


void
mime_parser_reset(struct Tree* mime_tree){
    struct TreeNode* node = mime_tree->first;
    struct TreeNode* children;
    while(node != NULL){
        children = node->children;
        while(children != NULL){
            parser_reset(children->parser);
            children = children->next;
        }
        parser_reset(node->parser);
        node = node->next;
    }
}

void
setContextType(struct ctx* ctx){
    struct TreeNode* node = ctx->mime_tree->first;
    if(node->event->type == STRING_CMP_EQ){
        ctx->subtype = node->children;
        return;
    }

    while(node->next != NULL){
        node = node->next;
        if(node->event->type == STRING_CMP_EQ){
            ctx->subtype = node->children;
            return;
        }
    }
}

static void
frontier_reset(struct Frontier* frontier){
    parser_reset(frontier->frontier_end_parser);
    parser_reset(frontier->frontier_parser);
}

const struct parser_event *
parser_feed_type (struct Tree* mime_tree, const uint8_t c){
    struct TreeNode* node = mime_tree->first;
    const struct parser_event* global_event;
    node->event = parser_feed(node->parser,c);
    global_event = node->event;
    while(node->next != NULL){
        node = node->next;
        node->event = parser_feed(node->parser,c);
        if(node->event->type == STRING_CMP_EQ){
            global_event = node->event;
        }
    }
    return global_event;
}

const struct parser_event *
parser_feed_subtype (struct TreeNode* node, const uint8_t c){
    struct parser_event* global_event;

    if(node->wildcard){
        global_event = malloc(sizeof(*global_event));
        memset(global_event,0,sizeof(*global_event));
        global_event->type = STRING_CMP_EQ;
        global_event->next = NULL;
        global_event->n = 1;
        global_event->data[0] = c;
        return global_event;
    }
    node->event = parser_feed(node->parser,c);
    global_event = (struct parser_event *)node->event;

    while(node->next != NULL){
        node = node->next;
        node->event = parser_feed(node->parser,c);
        if(node->event->type == STRING_CMP_EQ) {
            global_event = (struct parser_event *)node->event;
        }
    }
    return global_event;
}

static void
check_end_of_frontier(struct ctx*ctx, const uint8_t c){
    const struct parser_event* e = parser_feed(ctx->boundary_frontier->frontier_end_parser, c);
    do{
        debug("7.Body", parser_utils_strcmpi_event, e);
        switch(e->type){
            case STRING_CMP_EQ:
                ctx->frontier_end_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->frontier_end_detected = &F;
                break;
        }
        e = e->next;
    }while(e != NULL);
}


static void boundary_frontier_check(struct ctx*ctx, const uint8_t c){
    const struct parser_event* e = parser_feed(ctx->boundary_frontier->frontier_parser,c);
    do{
        debug("6.Body", parser_utils_strcmpi_event,e);
        switch(e->type){
            case STRING_CMP_EQ:
                ctx->frontier_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->frontier_detected = &F;
        }
        e = e->next;
    }while(e != NULL);
}

static void store_boundary_parameter(struct ctx*ctx, const uint8_t c){
    add_character(ctx->boundary_frontier,c);
}


static void parameter_boundary(struct ctx *ctx, const uint8_t c){
    const struct parser_event* e = parser_feed(ctx->boundary,c);
    do{
        debug("5.Boundary", parser_utils_strcmpi_event, e);
        switch(e->type){
            case STRING_CMP_EQ:
                ctx->boundary_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->boundary_detected = &F;
                break;
        }
        e = e->next;
    }while(e != NULL);
}

static void
content_type_subtype(struct ctx* ctx, const uint8_t c){
    const struct parser_event* e = parser_feed_subtype(ctx->subtype,c);
    do{
        debug("4.subtype", parser_utils_strcmpi_event, e);
        switch(e->type){
            case STRING_CMP_EQ:
                ctx->filtered_msg_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->filtered_msg_detected = &F;
                break;
        }
        e = e->next;
    } while(e != NULL);
}


static void
content_type_type(struct ctx*ctx, const uint8_t c){
    //struct TreeNode* node;
    const struct parser_event* e = parser_feed_type(ctx->mime_tree,c);
    do{
        debug("4.type", parser_utils_strcmpi_event, e);
        switch(e->type){
            case STRING_CMP_EQ:
                ctx->filtered_msg_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->filtered_msg_detected = &F;
                break;
        }
        e = e->next;
    } while (e != NULL);
}

static void
content_type_value(struct ctx*ctx, const uint8_t c){
    const struct parser_event* e = parser_feed(ctx->filtered_msg, c);
    do{
        debug("3.typeval", mime_type_event, e);
        switch(e->type){
            case MIME_TYPE_TYPE:
                if(ctx->filtered_msg_detected != 0
                   || *ctx->filtered_msg_detected)
                    for(int i=0;i<e->n;i++){
                        content_type_type(ctx,e->data[i]);
                    }

                break;
            case MIME_TYPE_SUBTYPE:
                if(ctx->filtered_msg_detected != 0
                   && *ctx->filtered_msg_detected)
                content_type_subtype(ctx,c);
                break;
            case MIME_TYPE_TYPE_END:
                if(ctx->filtered_msg_detected != 0
                   || *ctx->filtered_msg_detected){
                    setContextType(ctx);
                    break;
                }
            case MIME_PARAMETER:
                parameter_boundary(ctx,c);
                break;
            case MIME_FRONTIER:
                if(ctx->boundary_detected != 0
                        && *ctx->boundary_detected)
                    for(int i=0;i<e->n;i++){
                        store_boundary_parameter(ctx,e->data[i]);
                    }
                break;
        }
        e = e->next;
    } while (e != NULL);
}


/* Detecta si un header-field-name equivale a Content-Type.
 * Deja el valor en `ctx->msg_content_type_field_detected'. Tres valores
 * posibles: NULL (no tenemos información suficiente todavia, por ejemplo
 * viene diciendo Conten), true si matchea, false si no matchea.
 */
static void
content_type_header(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->ctype_header, c);
    do {
        debug("2.typehr", parser_utils_strcmpi_event, e);
        switch(e->type) {
            case STRING_CMP_EQ:
                ctx->msg_content_type_field_detected = &T;
                break;
            case STRING_CMP_NEQ:
                ctx->msg_content_type_field_detected = &F;
                break;
        }
        e = e->next;
    } while (e != NULL);
}

/**
 * Procesa un mensaje `tipo-rfc822'.
 * Si reconoce un al field-header-name Content-Type lo interpreta.
 *
 */
static void
mime_msg(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->msg, c);

    do {
        debug("1.   msg", mime_msg_event, e);
        switch(e->type) {
            case MIME_MSG_NAME:
                if( ctx->msg_content_type_field_detected == 0
                || *ctx->msg_content_type_field_detected) {
                    for(int i = 0; i < e->n; i++) {
                        content_type_header(ctx, e->data[i]);
                    }
                }
                break;
            case MIME_MSG_NAME_END:
                // lo dejamos listo para el próximo header
                parser_reset(ctx->ctype_header);
                break;
            case MIME_MSG_VALUE:
                if(ctx->msg_content_type_field_detected != 0
                    && *ctx->msg_content_type_field_detected) {
                    for(int i = 0; i < e->n; i++) {
                        content_type_value(ctx, e->data[i]);
                    }
                }
                break;
            case MIME_MSG_VALUE_END:
                // si parseabamos Content-Type ya terminamos
                end_frontier(ctx->boundary_frontier);
                parser_reset(ctx->filtered_msg);
                mime_parser_reset(ctx->mime_tree);
                parser_reset(ctx->boundary);
                ctx->msg_content_type_field_detected = 0;
                ctx->filtered_msg_detected = &T;
                break;
            case MIME_MSG_BODY:
                if((*ctx->boundary_detected != 0
                        && *ctx->boundary_detected) || *ctx->multipart_detected){
                    for(int i = 0; i < e->n; i++){
                        boundary_frontier_check(ctx,e->data[i]);
                        check_end_of_frontier(ctx,e->data[i]);
                    }
                }
                break;
            case MIME_MSG_BODY_NEWLINE:
                if(*ctx->frontier_end_detected && !*ctx->frontier_detected){
                    ctx->multipart_detected = &F;
                }
                   if (ctx->frontier_detected != 0 && *ctx->frontier_detected) {
                       ctx->multipart_detected = &T;
                       ctx->filtered_msg_detected = &F;
                       ctx->boundary_detected = &F;
                       ctx->frontier_detected = NULL;
                       ctx->frontier_end_detected = NULL;
                       ctx->subtype = NULL;
                       ctx->msg_content_type_field_detected = NULL;
                       parser_reset(ctx->msg);
                       mime_parser_reset(ctx->mime_tree);
                       parser_reset(ctx->filtered_msg);
                       parser_reset(ctx->boundary);
                       parser_reset(ctx->ctype_header);
                       frontier_reset(ctx->boundary_frontier);
                   }
                    if(ctx->boundary_frontier->frontier_parser != NULL){
                        parser_reset(ctx->boundary_frontier->frontier_parser);
                        parser_reset(ctx->boundary_frontier->frontier_end_parser);
                    }
                break;
        }
        e = e->next;
    } while (e != NULL);
}

/* Delimita una respuesta multi-línea POP3. Se encarga del "byte-stuffing" */
static void
pop3_multi(struct ctx *ctx, const uint8_t c) {
    const struct parser_event* e = parser_feed(ctx->multi, c);
    do {
        debug("0. multi", pop3_multi_event,  e);
        switch (e->type) {
            case POP3_MULTI_BYTE:
                for(int i = 0; i < e->n; i++) {
                    mime_msg(ctx, e->data[i]);
                }
                break;
            case POP3_MULTI_WAIT:
                // nada para hacer mas que esperar
                break;
            case POP3_MULTI_FIN:
                // arrancamos de vuelta
                parser_reset(ctx->msg);
                ctx->msg_content_type_field_detected = NULL;
                break;
        }
        e = e->next;
    } while (e != NULL);
}

int
stripmime(int argc, const char **argv, struct Tree* tree) {

    const unsigned int* no_class = parser_no_classes();
    struct parser_definition media_header_def =
            parser_utils_strcmpi("content-type");

    struct parser_definition boundary_def =
            parser_utils_strcmpi("boundary");

    struct ctx ctx = {
            .multi        = parser_init(no_class, pop3_multi_parser()),
            .msg          = parser_init(init_char_class(), mime_message_parser()),
            .ctype_header = parser_init(no_class, &media_header_def),
            .filtered_msg = parser_init(init_char_class(), mime_type_parser()),
            .boundary     = parser_init(no_class, &boundary_def),
            .mime_tree    = tree,
            .boundary_frontier = frontier_init(),
            .filtered_msg_detected = NULL,
            .boundary_detected     = NULL,
            .frontier_end_detected = NULL,
            .frontier_detected     = NULL,
            .multipart_detected    = &F,
    };

    uint8_t data[4096];
    ssize_t n;
    int fd = STDIN_FILENO;

    //freopen("myfile.txt", "w", stdout);

    do {
        n = read(fd, data, sizeof(data));
        for(ssize_t i = 0; i < n ; i++) {
            pop3_multi(&ctx, data[i]);
            putchar(data[i]);
        }
    } while(n > 0);

    parser_destroy(ctx.multi);
    parser_destroy(ctx.msg);
    parser_destroy(ctx.ctype_header);
    parser_destroy(ctx.filtered_msg);
    parser_destroy(ctx.boundary);
    mime_parser_destroy(ctx.mime_tree);
    parser_utils_strcmpi_destroy(&media_header_def);

    //fclose(stdout);

    return 0;
}
