#include <stdint.h>
#include <wchar.h>
#include <memory.h>
#include <stdlib.h>
#include "frontier.h"
#include "parser.h"
#include "parser_utils.h"

struct Frontier*
frontier_init(){
    struct Frontier* node = malloc(sizeof(*node));
    memset(node,0,sizeof(*node));
    if(node != NULL){
        node->frontier_size = 2;
        node->frontier[0] = '-';
        node->frontier[1] = '-';
    }
    return node;
}

void
end_frontier(struct Frontier* frontier){
    if(frontier != NULL){
        struct parser_definition* def = malloc(sizeof(*def));
        struct parser_definition aux = parser_utils_strcmpi(frontier->frontier);
        memcpy(def,&aux,sizeof(aux));
        frontier->frontier_parser = parser_init(parser_no_classes(),def);
        frontier->frontier_parser_def = def;
    }
}


void
add_character(struct Frontier* frontier, const uint8_t c){
    frontier->frontier[frontier->frontier_size] = c;
    frontier->frontier_size++;
}
