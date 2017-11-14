#ifndef MIMETREE_H_
#define MIMETREE_H_

#include <stdbool.h>

struct TreeNode{
	struct parser* parser;
	struct parser_definition *def;
	struct TreeNode *next;
	struct TreeNode *children;
	const char* name;
	const struct parser_event* event;
	bool wildcard;
};

struct Tree{
	struct TreeNode* first;
};

struct MIMEtype{
	char* type;
	char* subtype;
};



struct Tree* tree_init(void);

void addNode(struct Tree* tree, char* type,char* subtype);

void removeNode(struct Tree* tree, char* type, char* subtype);

void mime_parser_destroy(struct Tree *mime_tree);

void mime_parser_reset(struct Tree* mime_tree);

#endif