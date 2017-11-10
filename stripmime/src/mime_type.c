#include "parser.h"
#include "mime_chars.h"
#include "mime_type.h"

/**
 *	Syntax
 *
 *	General structure:
 *
 *	type/subtype
 *
 *	The structure of a MIME type is very simple; it consists of a type and a 
 *	subtype, two strings, separated by a '/'. No space is allowed. The type 
 *	represents the category and can be a discrete or a multipart type. 
 *	The subtype is specific to each type.
 *
 *	A MIME type is insensitive to the case, but traditionally is written all in lower case.
 *
 */
enum state {
	TYPE0,
    TYPE,
    SUBTYPE,
    ERROR,
};

///////////////////////////////////////////////////////////////////////////////
// Acciones

static void
type(struct parser_event *ret, const uint8_t c) {
    ret->type    = MIME_TYPE_TYPE;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
type_end(struct parser_event *ret, const uint8_t c) {
    ret->type    = MIME_TYPE_TYPE_END;
    ret->n       = 1;
	ret->data[0] = '/';
}

static void
subtype(struct parser_event *ret, const uint8_t c) {
    ret->type    = MIME_TYPE_SUBTYPE;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
unexpected(struct parser_event *ret, const uint8_t c) {
	ret->type    = MIME_TYPE_UNEXPECTED;
    ret->n       = 1;
    ret->data[0] = c;	
}

///////////////////////////////////////////////////////////////////////////////
// Transiciones

static const struct parser_state_transition ST_TYPE0[] =  {
    {.when = '/',        				.dest = ERROR,         		.act1 = unexpected,	},
    {.when = TOKEN_LWSP,   				.dest = ERROR,         		.act1 = unexpected,	},
    {.when = TOKEN_REST_NAME_FIRST,		.dest = TYPE,				.act1 = type,		},
    {.when = ANY, 						.dest = ERROR, 				.act1 = unexpected,	}
};

static const struct parser_state_transition ST_TYPE[] =  {
    {.when = '/',        				.dest = SUBTYPE,         	.act1 = type_end,	},
    {.when = TOKEN_LWSP,   				.dest = ERROR,          	.act1 = unexpected,	},
    {.when = TOKEN_REST_NAME_CHARS,		.dest = TYPE,				.act1 = type,		},
    {.when = ANY, 						.dest = ERROR, 				.act1 = unexpected,	}
};

static const struct parser_state_transition ST_SUBTYPE[] =  {
	{.when = TOKEN_LWSP,   				.dest = ERROR,          	.act1 = unexpected,	},
    {.when = TOKEN_REST_NAME_CHARS,		.dest = SUBTYPE,			.act1 = subtype,	},
    {.when = ANY, 						.dest = ERROR, 				.act1 = unexpected,	}
};

static const struct parser_state_transition ST_ERROR[] =  {
    {.when = ANY,        				.dest = ERROR,         		.act1 = unexpected,	},
};


///////////////////////////////////////////////////////////////////////////////
// Declaración formal

static const struct parser_state_transition *states [] = {
    ST_TYPE0,
    ST_TYPE,
    ST_SUBTYPE,
    ST_ERROR,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t states_n [] = {
    N(ST_TYPE0),
    N(ST_TYPE),
    N(ST_SUBTYPE),
    N(ST_ERROR),
};

static struct parser_definition definition = {
    .states_count = N(states),
    .states       = states,
    .states_n     = states_n,
    .start_state  = TYPE0,
};

const struct parser_definition * 
mime_type_parser(void) {
    return &definition;
}


const char *
mime_type_event(enum mime_type_event_type type) {
    const char *ret;

    switch(type) {
        case MIME_TYPE_TYPE:
            ret = "type(c)";
            break;
        case MIME_TYPE_TYPE_END:
            ret = "type_end('/')";
            break;
        case MIME_TYPE_SUBTYPE:
            ret = "subtype(c)";
            break;
        case MIME_TYPE_UNEXPECTED:
            ret = "unexepected(c)";
            break;
    }
    return ret;
}
