/**
 * request.c -- parser del request de SOCKS5
 */
#include <string.h> // memset
#include <arpa/inet.h>
#include <ctype.h>

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

static enum request_state
parse_cmd(struct request * r, uint8_t c) {
    if (strcmp("quit", r->cmd_buffer) == 0) {
        r->cmd = pop3_req_quit;
        return request_done;
    } else {
        r->cmd = pop3_req_other;
    }

    return request_space;
}

static enum request_state
cmd(const uint8_t c, struct request_parser* p) {
    if (isspace(c) || c == '\n') {
        return parse_cmd(p->request, c);
    }

    if (!isalpha(c)) {
        return request_error;
    }

    struct request * r = p->request;
    r->cmd_buffer[r->i++] = c;

    return request_cmd;
}

static enum request_state
space(const uint8_t c, struct request_parser* p) {
    if (isspace(c)) {
        return request_space;
    }

    p->request->i = 0;
    return request_param;
}

static enum request_state
param(const uint8_t c, struct request_parser* p) {
    if (c == '\n') {
        return request_done;
    }

    struct request * r = p->request;
    r->pop3_req_param[r->i++] = c;

    if (r->i >= 40) {
        return request_error;
    }

    return request_param;
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
        case request_space:
            next = space(c, p);
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

extern int
request_marshall(buffer *b,
                 const enum pop3_response_status status) {
    return 10;
}
