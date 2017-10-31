#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"


#define CMD_SIZE    6
#define PARAM_SIZE  40

/*
 * POP3 Request:
 *
 *      [CMD] [PARAMETER]
 *
 *      - CMD : USER, PASS, LIST, RETR, QUIT, CAPA, etc...
 */

enum pop3_req_cmd {
    pop3_req_quit,
    pop3_req_retr,
    pop3_req_other, //TODO
};

struct request {
    enum pop3_req_cmd   cmd;
    unsigned            i, j, count;
    // TODO podria usar buffer.c
    char                cmd_buffer[CMD_SIZE];
    char                pop3_req_param[PARAM_SIZE];  //TODO ver RFC pop3 si acepta mas de un parametro
};

enum request_state {
   request_cmd,
   request_param,

    // apartir de aca están done
   request_done,

   // y apartir de aca son considerado con error
   request_error,
   request_error_unsupported_cmd,
};

struct request_parser {
   struct request *request;
   enum request_state state;
   /** cuantos bytes tenemos que leer*/
   uint8_t n;
   /** cuantos bytes ya leimos */
   uint8_t i;
};

/*
 *  POP3 Response
 *
 *      [INDICATOR] [DESCRIPTION]
 *
 *      - INDICATOR: +OK o -ERR
 */
enum pop3_response_status {
    status_ok,
    status_err,
};


/** inicializa el parser */
void 
request_parser_init (struct request_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state 
request_parser_feed (struct request_parser *p, const uint8_t c);

/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored);

/**
 * Permite distinguir a quien usa request_parser_feed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool 
request_is_done(const enum request_state st, bool *errored);

void
request_close(struct request_parser *p);

/**
 * serializa la request en buff
 *
 * Retorna la cantidad de bytes ocupados del buffer o -1 si no había
 * espacio suficiente.
 */
extern int
request_marshall(struct request *r, buffer *b);


/** convierte a errno en pop3_response_status */
enum pop3_response_status
errno_to_pop3(int e);

#endif
