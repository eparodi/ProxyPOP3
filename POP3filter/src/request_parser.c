/**
 * request.c -- parser del request de POP3
 */
#include <string.h> // memset
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include "request_parser.h"

static enum request_state
cmd(const uint8_t c, struct request_parser* p) {
    enum request_state ret = request_cmd;
    struct pop3_request *r = p->request;

    if (c == ' ' || c == '\r' || c == '\n') {   // aceptamos comandos terminados en '\r\n' y '\n'
        r->cmd = get_cmd(p->cmd_buffer);
        if (r->cmd->id == error) {
            ret = request_error;
        } else if (c == ' ') {
            ret = request_param;
        } else if (c == '\r'){
            ret = request_newline;
        } else {
            ret = request_done;
        }
    } else if (p->i >= CMD_SIZE) {
        r->cmd = get_cmd(""); // seteo cmd en error
        ret = request_error_cmd_too_long;
    } else {
        p->cmd_buffer[p->i++] = c;
    }

    return ret;
}

static enum request_state
param(const uint8_t c, struct request_parser* p) {
    enum request_state ret = request_param;
    struct pop3_request *r = p->request;

    if (c == '\r' || c == '\n') {
        char * aux = p->param_buffer;
        int count = 0;
        while (*aux  != 0) {
            if (*aux == ' ' || *aux == '\t')
                count++;
            aux++;
        }

        if (count != p->j) {
            r->args = malloc(strlen(p->param_buffer) + 1);
            strcpy(r->args, p->param_buffer);
        }

        if (c == '\r') {
            ret = request_newline;
        } else {
            ret = request_done;
        }

    } else {
        p->param_buffer[p->j++] = c;
        if (p->j >= PARAM_SIZE) {
            ret = request_error_param_too_long;
        }
    }

    return ret;
}

static enum request_state
newline(const uint8_t c, struct request_parser* p) {

    if (c != '\n') {
        return request_error;
    }

    return request_done;
}

extern void
request_parser_init (struct request_parser* p) {
    memset(p->request, 0, sizeof(*(p->request)));
    memset(p->cmd_buffer, 0, CMD_SIZE);
    memset(p->param_buffer, 0, PARAM_SIZE);
    p->state = request_cmd;
    p->i = p->j = 0;
}

extern enum request_state 
request_parser_feed (struct request_parser* p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_cmd:
            next = cmd(c, p);
            break;
        case request_param:
            next = param(c, p);
            break;
        case request_newline:
            next = newline(c, p);
            break;
        case request_done:
        case request_error:
        case request_error_cmd_too_long:
        case request_error_param_too_long:
            next = p->state;
            break;
        default:
            next = request_error;
            break;
    }

    return p->state = next;
}

extern bool 
request_is_done(const enum request_state st, bool *errored) {
    if(st >= request_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done;
}

extern enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;
    uint8_t c = 0;

    while(buffer_can_read(b)) {
        c = buffer_read(b);
        st = request_parser_feed(p, c);
        if(request_is_done(st, errored)) {
            break;
        }
    }

    //limpio el buffer luego de un comando invalido
    if (st >= request_error && c != '\n') {
        while(buffer_can_read(b) && c != '\n') {
            c = buffer_read(b);
        }

        if (c != '\n') {
            st = request_cmd;
        }
    }

    return st;
}

extern void
request_parser_close(struct request_parser *p) {
    // nada que hacer
}

extern int
request_marshall(struct pop3_request *r, buffer *b) {
    size_t  n;
    uint8_t *buff;

    const char * cmd = r->cmd->name;
    char * args = r->args;

    size_t i = strlen(cmd);
    size_t j = args == NULL ? 0 : strlen(args);
    size_t count = i + j + (j == 0 ? 2 : 3);

    buff = buffer_write_ptr(b, &n);

    if(n < count) {
        return -1;
    }

    memcpy(buff, cmd, i);
    buffer_write_adv(b, i);


    if (args != NULL) {
        buffer_write(b, ' ');
        buff = buffer_write_ptr(b, &n);
        memcpy(buff, args, j);
        buffer_write_adv(b, j);
    }

    buffer_write(b, '\r');
    buffer_write(b, '\n');

    return (int)count;
}
