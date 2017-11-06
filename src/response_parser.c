/**
 * response.c -- parser del response de POP3
 */

#include <string.h> // memset
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "response_parser.h"

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

    //TODO multilinea
    switch (p->request->cmd->id) {
        case retr:
        case list:
        case capa:
            // ret = multiline_baby!
            break;
    }

    return c != '\n' ? response_error : ret;
}

enum response_state
mail(const uint8_t c, struct response_parser* p) {
    //TODO usar el parser pop3_multi

    return response_mail;
}


extern void
response_parser_init (struct response_parser* p) {
    memset(p->status_buffer, 0, STATUS_SIZE);
    p->state = response_status_indicator;
    p->i = 0;
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
response_consume(buffer *b, struct response_parser *p, bool *errored) {
    enum response_state st = p->state;

    while(buffer_can_read(b)) {
        const uint8_t c = buffer_read(b);
        st = response_parser_feed(p, c);
        buffer_write(b, c);
        if(response_is_done(st, errored)) {
            break;
        }
    }
    return st;
}

extern void
response_close(struct response_parser *p) {
    // nada que hacer
}