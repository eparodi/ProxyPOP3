/**
 * response.c -- parser del response de POP3
 */

#include <string.h> // memset
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "response_parser.h"
#include "pop3_multi.h"

enum response_state
status(const uint8_t c, struct response_parser* p) {
    enum response_state ret = response_status_indicator;
    if (c == ' ' || c == '\r') {
        p->request->response = get_response(p->status_buffer);
        if (c == ' ') {
            ret = response_description;
        } else {
            ret = response_newline;
        }
    } else if (p->i >= STATUS_SIZE) {
        ret = response_error;
    } else {
        p->status_buffer[p->i++] = c;
    }

    return ret;
}

//ignoramos la descripcion
enum response_state
description(const uint8_t c, struct response_parser* p) {
    enum response_state ret = response_description;

    if (c == '\r') {
        ret = response_newline;
    }

    return ret;
}

enum response_state
newline(const uint8_t c, struct response_parser *p) {
    enum response_state ret = response_done;

    if (p->request->response->status != response_status_err) {
        switch (p->request->cmd->id) {
            case retr:
                ret = response_mail;
                break;
            case list:
                if (p->request->args == NULL) {
                    ret = response_list;
                }
                break;
            case capa:
                ret = response_capa;
                break;
            case uidl:
            case top:
                ret = response_multiline;
                break;
            default:
                break;
        }
    }

    if (c == '\n') {
        p->first_line_done = true;
    }

    return c != '\n' ? response_error : ret;
}

enum response_state
mail(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_mail;

    switch (e->type) {
        case POP3_MULTI_FIN:
            ret = response_done;
            break;
    }

    return ret;
}

enum response_state
plist(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_list;

    switch (e->type) {
        case POP3_MULTI_FIN:
            ret = response_done;
            break;
    }

    return ret;
}

#define BLOCK_SIZE  20

enum response_state
pcapa(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_capa;

    // save capabilities to struct
    p->capa_response[p->j++] = c;
    if (p->j % BLOCK_SIZE == 0) {
        p->capa_response = realloc(p->capa_response, strlen(p->capa_response) + BLOCK_SIZE);
        memset(p->capa_response + p->j, 0, BLOCK_SIZE);
    }

    switch (e->type) {
        case POP3_MULTI_FIN:

            if (p->j % BLOCK_SIZE == 0) {
                p->capa_response = realloc(p->capa_response, strlen(p->capa_response) + 1);
                memset(p->capa_response + p->j, 0, 1);
            }

            ret = response_done;
            break;
    }

    return ret;
}

enum response_state
multiline(const uint8_t c, struct response_parser* p) {
    const struct parser_event * e = parser_feed(p->pop3_multi_parser, c);
    enum response_state ret = response_multiline;

    switch (e->type) {
        case POP3_MULTI_FIN:
            ret = response_done;
            break;
    }

    return ret;
}

extern void
response_parser_init (struct response_parser* p) {
    memset(p->status_buffer, 0, STATUS_SIZE);
    p->state = response_status_indicator;
    p->first_line_done = false;
    p->i = 0;

    if (p->pop3_multi_parser == NULL) {
        p->pop3_multi_parser = parser_init(parser_no_classes(), pop3_multi_parser());
    }

    parser_reset(p->pop3_multi_parser);

    if (p->capa_response == NULL) {
        p->capa_response = calloc(BLOCK_SIZE, sizeof(char));
        p->j = 0;
    }

}

extern enum response_state
response_parser_feed (struct response_parser* p, const uint8_t c) {
    enum response_state next;

    switch(p->state) {
        case response_status_indicator:
            next = status(c, p);
            break;
        case response_description:
            next = description(c, p);
            break;
        case response_newline:
            next = newline(c, p);
            break;
        case response_mail:
            next = mail(c, p);
            break;
        case response_list:
            next = plist(c, p);
            break;
        case response_capa:
            next = pcapa(c, p);
            break;
        case response_multiline:
            next = multiline(c, p);
            break;
        case response_done:
        case response_error:
            next = p->state;
            break;
        default:
            next = response_error;
            break;
    }

    return p->state = next;
}

extern bool
response_is_done(const enum response_state st, bool *errored) {
    if(st >= response_error && errored != 0) {
        *errored = true;
    }
    return st >= response_done;
}

extern enum response_state
response_consume(buffer *b, buffer *wb, struct response_parser *p, bool *errored) {
    enum response_state st = p->state;

    while(buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);
        st = response_parser_feed(p, c);
        buffer_write(wb, c);
        if(response_is_done(st, errored) || p->first_line_done) {   // si se termino la respuesta o se termino de leer la primera linea
            break;
        }
    }
    return st;
}

extern void
response_close(struct response_parser *p) {
    // nada que hacer
}