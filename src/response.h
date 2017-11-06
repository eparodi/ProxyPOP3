#ifndef POP3_RESPONSE_H_
#define POP3_RESPONSE_H_

enum pop3_response_status {
    response_status_invalid = -1,

    response_status_ok,
    response_status_err,
};

struct pop3_response {
    const enum pop3_response_status status;
    const char 						*name;
};

const struct pop3_response *
get_response(const char *response);

#endif