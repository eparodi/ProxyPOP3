/**
 * request.c -- parser del request de POP3
 */
#include <string.h> // memset
#include <arpa/inet.h>
#include <stdio.h>

#include "request.h"

static void
remaining_set(struct request_parser* p, const int n) {
    p->i = 0;
    p->n = n;
}

static int
remaining_is_done(struct request_parser* p) {
    return p->i >= p->n;
}

//////////////////////////////////////////////////////////////////////////////

#include <assert.h>
static void
parse_cmd(struct request *r) {

    //TODO add parser_utils_stcmpi para cada comando

    if (strcmp("quit", r->cmd_buffer) == 0) {
        r->cmd = pop3_req_quit;
    } else {
        r->cmd = pop3_req_other;
    }
}

static enum request_state
cmd(const uint8_t c, struct request_parser* p) {
    enum request_state ret = request_cmd;
    struct request *r = p->request;

    if (c == ' ' || c == '\n') {
        r->count += r->i + 1; // ' '
        parse_cmd(p->request);
        ret = c == ' ' ? request_param : request_done;
    } else {
        r->cmd_buffer[r->i++] = c;
        if (r->i >= CMD_SIZE) {
            r->count += r->i - 1;
            ret = request_error;
        }
    }

    return ret;
}

static enum request_state
param(const uint8_t c, struct request_parser* p) {
    enum request_state ret = request_param;
    struct request *r = p->request;

    if (c == '\n') {
        r->count += r->j + 1;
        ret = request_done;
    } else {
        r->pop3_req_param[r->j++] = c;
        if (r->j >= PARAM_SIZE) {
            r->count += r->j - 1;
            ret = request_error;
        }
    }

    return ret;
}

extern void
request_parser_init (struct request_parser* p) {
    p->state = request_cmd;
    memset(p->request, 0, sizeof(*(p->request)));
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
        case request_done:
        case request_error:
        case request_error_unsupported_cmd:
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

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = request_parser_feed(p, c);
       if(request_is_done(st, errored)) {
          break;
       }
    }
    return st;
}

extern void
request_close(struct request_parser *p) {
    // nada que hacer
}

//VIVA LA PERU
extern int
request_marshall(struct request *r, buffer *b) {
    size_t  n;
    uint8_t *buff = buffer_write_ptr(b, &n);

    printf("request->count: %d\n", r->count);
    printf("request->cmd: %s\n", r->cmd_buffer);
    printf("request->param: %s\n", r->pop3_req_param);

    if(n < r->count) {
        return -1;
    }

    memcpy(buff, r->cmd_buffer, r->i);
    buffer_write_adv(b, r->i);

    buffer_write(b, ' ');
    buff = buffer_write_ptr(b, &n);

    if (n < r->j + 1) {
        return -1;
    }

    memcpy(buff, r->pop3_req_param, r->j);
    buffer_write_adv(b, r->j);

    buffer_write(b, '\n');

    printf("buffer: %s\n", b->data);

    return r->count;
}
