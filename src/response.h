#ifndef POP3_RESPONSE_H_
#define POP3_RESPONSE_H_

enum pop3_response_status {
    OK,
    ERR,
};

enum pop3_response_status
parse_response(const char *response);

#endif