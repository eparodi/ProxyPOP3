#include <string.h>

#include "pop3_session.h"

void pop3_session_init(struct pop3_session *s, bool pipelining) {
    memset(s, 0, sizeof(*s));

    s->pipelining = pipelining;
    s->state = POP3_AUTHORIZATION;

    s->request_queue = queue_new();
}

void pop3_session_close(struct pop3_session *s) {
    queue_destroy(s->request_queue);
    s->state = POP3_DONE;
}