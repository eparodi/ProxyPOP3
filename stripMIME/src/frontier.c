#include <stdint.h>
#include <wchar.h>
#include <memory.h>
#include <stdlib.h>
#include "frontier.h"
#include "parser.h"
#include "parser_utils.h"
#include "mime_chars.h"

struct Frontier *
frontier_init() {
    struct Frontier *node = malloc(sizeof(*node));
    memset(node, 0, sizeof(*node));
    if (node != NULL) {
        node->frontier_size = 2;
        node->frontier[0] = '-';
        node->frontier[1] = '-';
    }
    return node;
}

void
end_frontier(struct Frontier *frontier) {
    if (frontier != NULL) {
        frontier->frontier[frontier->frontier_size] = 0;
        struct parser_definition *def = malloc(sizeof(*def));
        struct parser_definition aux = parser_utils_strcmpi(frontier->frontier);
        memcpy(def, &aux, sizeof(aux));
        frontier->frontier_parser = parser_init(init_char_class(), def);
        frontier->frontier_parser_def = def;
        frontier->frontier[frontier->frontier_size] = '-';
        frontier->frontier[frontier->frontier_size + 1] = '-';
        frontier->frontier[frontier->frontier_size + 2] = 0;
        struct parser_definition *def_end = malloc(sizeof(*def_end));
        struct parser_definition aux_end = parser_utils_strcmpi(frontier->frontier);
        memcpy(def_end, &aux_end, sizeof(aux_end));
        frontier->frontier_end_parser = parser_init(init_char_class(), def_end);
    }
}


void
add_character(struct Frontier *frontier, const uint8_t c) {
    frontier->frontier[frontier->frontier_size] = c;
    frontier->frontier_size++;
}

void
frontier_reset(struct Frontier *frontier) {
    parser_reset(frontier->frontier_end_parser);
    parser_reset(frontier->frontier_parser);
}
