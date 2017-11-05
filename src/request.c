#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

#include "request.h"
#include "pop3_session.h"

#define CMD_SIZE	(capa + 1)

static struct pop3_request_cmd auth_commands[] = {
        {
                .id 	= user,
                .name 	= "user",
        },
        {
                .id 	= pass,
                .name 	= "pass",
        },
        {
                .id 	= apop,
                .name 	= "apop",
        },
        {
                .id 	= quit,
                .name 	= "quit",
        },
        {
                .id 	= capa,
                .name 	= "capa",
        },
        /* marca de final */
        {
                .id 	= error,
                .name	= NULL,
        }
};

static struct pop3_request_cmd tran_commdands[] = {
        {
                .id 	= stat,
                .name 	= "stat",
        },
        {
                .id 	= list,
                .name 	= "list",
        },
        {
                .id 	= retr,
                .name 	= "retr",
        },
        {
                .id 	= dele,
                .name 	= "dele",
        },
        {
                .id 	= noop,
                .name 	= "noop",
        },
        {
                .id 	= rset,
                .name 	= "rset",
        },
        {
                .id 	= top,
                .name 	= "top",
        },
        {
                .id 	= uidl,
                .name 	= "uidl",
        },
        {
                .id 	= quit,
                .name 	= "quit",
        },
        {
                .id 	= capa,
                .name 	= "capa",
        },
        /* marca de final */
        {
                .id 	= error,
                .name	= NULL,
        }
};

static struct pop3_request_cmd * all_commands[POP3_DONE] = {
        auth_commands, tran_commdands, NULL,
};

///////////////////////////////////////////////////////

const struct pop3_request_cmd commands[CMD_SIZE] = {
        {
                .id 	= user,
                .name 	= "user",
        },
        {
                .id 	= pass,
                .name 	= "pass",
        },
        {
                .id 	= apop,
                .name 	= "apop",
        },
        {
                .id 	= stat,
                .name 	= "stat",
        },
        {
                .id 	= list,
                .name 	= "list",
        },
        {
                .id 	= retr,
                .name 	= "retr",
        },
        {
                .id 	= dele,
                .name 	= "dele",
        },
        {
                .id 	= noop,
                .name 	= "noop",
        },
        {
                .id 	= rset,
                .name 	= "rset",
        },
        {
                .id 	= top,
                .name 	= "top",
        },
        {
                .id 	= uidl,
                .name 	= "uidl",
        },
        {
                .id 	= quit,
                .name 	= "quit",
        },
        {
                .id 	= capa,
                .name 	= "capa",
        },
};

const struct pop3_request_cmd invalid_cmd = {
        .id = error,
        .name = NULL,
};


/**
 * Comparacion case insensitive de dos strings
 */
static bool strcmpi(const char * str1, const char * str2) {
    int c1, c2;
    while (*str1 && *str2) {
        c1 = tolower(*str1++);
        c2 = tolower(*str2++);
        if (c1 != c2) {
            return false;
        }
    }

    return *str1 == 0 && *str2 == 0 ? true : false;
}

#define N(x) (sizeof(x)/sizeof((x)[0]))

const struct pop3_request_cmd * parse_cmd(const char *cmd) {

    for (int i = 0; i < N(commands); i++) {
        if (strcmpi(cmd, commands[i].name)) {
            return &commands[i];
        }
    }

    return &invalid_cmd;
}

enum pop3_cmd_id parse_cmd_2(unsigned state, const char *cmd) {

    struct pop3_request_cmd * commands = all_commands[state];

    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmpi(cmd, commands[i].name)) {
            return commands[i].id;
        }
    }

    return error;
}