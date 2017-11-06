#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <zconf.h>

#include "request.h"
#include "parameters.h"

#define CMD_SIZE	(capa + 1)

/** Ejecuta una transformacion externa luego de un retrieve */
void retr_fn(struct pop3_request *r, int client_fd, int origin_fd) {

    char * args[4];
    args[0] = "bash";
    args[1] = "-c";
    args[2] = parameters->filter_command;
    args[3] = NULL;

    int fd[2];
    size_t  nbytes;
    char readbuffer[2048];
    pipe(fd);

    int pid = fork();

    if (pid == -1){
        perror("fork error");
    }else if(pid == 0){
        dup2(fd[0], 0);
        dup2(fd[1], 1);
        int value = execve("/bin/bash", args, NULL);
        perror("execve");
        if (value == -1){
            fprintf(stderr, "Error\n");
        }
    } else {
        // dup2(fd[0], client_fd)
        // fd[0] leer la respuesta
        // fd[1] mandar el mail del server
        // nbytes = (size_t) read(fd[0], readbuffer, sizeof(readbuffer));
        // readbuffer[nbytes-1] = '\0';
        // write(client_fd, readbuffer, nbytes);
        // printf("%s\n",readbuffer);
    }
}

/** Placeholder para comandos que no necesitan ejecutar nada */
void common_fn(struct pop3_request *r, int client_fd, int origin_fd) {
    //printf("Nada por hacer\n");
}

const struct pop3_request_cmd commands[CMD_SIZE] = {
        {
                .id 	= user,
                .name 	= "user",
                .fn     = common_fn,
        },
        {
                .id 	= pass,
                .name 	= "pass",
                .fn     = common_fn,
        },
        {
                .id 	= apop,
                .name 	= "apop",
                .fn     = common_fn,
        },
        {
                .id 	= stat,
                .name 	= "stat",
                .fn     = common_fn,
        },
        {
                .id 	= list,
                .name 	= "list",
                .fn     = common_fn,
        },
        {
                .id 	= retr,
                .name 	= "retr",
                .fn     = retr_fn,
        },
        {
                .id 	= dele,
                .name 	= "dele",
                .fn     = common_fn,
        },
        {
                .id 	= noop,
                .name 	= "noop",
                .fn     = common_fn,
        },
        {
                .id 	= rset,
                .name 	= "rset",
                .fn     = common_fn,
        },
        {
                .id 	= top,
                .name 	= "top",
                .fn     = common_fn,
        },
        {
                .id 	= uidl,
                .name 	= "uidl",
                .fn     = common_fn,
        },
        {
                .id 	= quit,
                .name 	= "quit",
                .fn     = common_fn,
        },
        {
                .id 	= capa,
                .name 	= "capa",
                .fn     = common_fn,
        },
};

const struct pop3_request_cmd invalid_cmd = {
        .id     = error,
        .name   = NULL,
        .fn     = NULL,
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

const struct pop3_request_cmd * get_cmd(const char *cmd) {

    for (int i = 0; i < N(commands); i++) {
        if (strcmpi(cmd, commands[i].name)) {
            return &commands[i];
        }
    }

    return &invalid_cmd;
}