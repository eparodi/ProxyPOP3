#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"
#include "request.h"

enum request_state {
    request_cmd,
    request_param,
    request_newline,

    // apartir de aca están done
    request_done,

    // y apartir de aca son considerado con error
    request_error,
    request_error_cmd_too_long,
    request_error_param_too_long,
};

#define CMD_SIZE    4
#define PARAM_SIZE  (40 * 2)       // cada argumento puede tener 40 bytes y la maxima cantidad de argumentos de un comando es 2

struct request_parser {
    struct pop3_request *request;
    enum request_state  state;

    uint8_t             i, j;

    char                cmd_buffer[CMD_SIZE];
    char                param_buffer[PARAM_SIZE];
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
request_is_done(enum request_state st, bool *errored);

void
request_parser_close(struct request_parser *p);

/**
 * serializa la request en buff
 *
 * Retorna la cantidad de bytes ocupados del buffer o -1 si no había
 * espacio suficiente.
 */
extern int
request_marshall(struct pop3_request *r, buffer *b);


#endif
