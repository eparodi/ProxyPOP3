#ifndef TPE_PROTOS_STACK_H
#define TPE_PROTOS_STACK_H

#include <stdint.h>

#define FRONTIER_MAX 72 //70 plus '--'


struct Frontier{
    char frontier[FRONTIER_MAX];
    uint8_t frontier_size;
    struct parser* frontier_parser;
    struct parser_definition* frontier_parser_def;
};



struct Frontier*
frontier_init();

void
add_character(struct Frontier* frontier, const uint8_t c);

void
end_frontier(struct Frontier* frontier);


#endif //TPE_PROTOS_STACK_H
