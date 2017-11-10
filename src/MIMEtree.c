#include <stdlib.h>
#define WILDCARD '*'


struct Tree{
	struct TreeNode* first;
}

struct TreeNode{
	parser* parser;
	struct TreeNode *next;
	struct TreeNode *children;
	const char* name;
	bool match;
	bool wildcard;
}


struct Tree*
tree_init(){
	struct Tree* tree = malloc(sizeof(*tree));
	if(tree != NULL){
		tree->first = malloc(sizeof(*first));
		if(first != NULL){
			return tree;
		}
	}
	return NULL;
}

struct TreeNode*
findTypeMatch(struct Tree* tree, char* type, bool* found){
	struct TreeNode* node = tree->first;
	while(node != NULL){
		if(node->name == type){
			*found = true;
			return node;
		}
		node = node->next;
	}
	return node;
}

struct TreeNode*
findSubTypeMatch(struct TreeNode* node, char* subtype, bool* found){
	while(node != NULL){
		if(node->wildcard){
			*found = true;
			return node;
		}
		if(node->name == subtype){
			*found = true;
			return node;
		}
		node = node->next;
	}
	return node;
}

void addWildcard(struct Tree* tree, char* type){	
	if(found){
		removeChildren(node);
		node->children = newNodeWildcard();
	}else{
		node->next = newNode(type);
		node->next->children = newNodeWildcard();
	}
	return;
}

void
add(struct Tree* tree, char* type, char*subtype) {
	if(tree!=NULL){
	if(subtype == WILDCARD){
		addWildcard(tree,type);
	}else{
		bool found = false;
		struct TreeNode* node = findTypeMatch(tree,type, &found);
		if(found){
			found = false;
			node = findSubTypeMatch(node, subtype, &found);
			if(found){
				return;
			}else{
				node->next = newNode(subtype);
			}
		}else{
			node->next = newNode(type);
			node->next->children = newNode(subtype);	
		}
	}
	return;	
	}
}

struct TreeNode*
newNode(char* name){
	struct TreeNode* node = malloc(sizeof(*node));
	if(node != NULL){
		node->parser = NULL//parser_init(init_char_class(),parser_utils_strcmpi(name));
		node->next = NULL;
		node->children = NULL;
		node->name = name;
		node->match = true;
		node->wildcard = false;
	}
	return node;
}

struct TreeNode*
newNodeWildcard(){
	struct TreeNode* node = malloc(sizeof(*node));
	if(node != NULL){
		node->parser = NULL
		node->next = NULL;
		node->children = NULL;
		node->name = "*";
		node->match = true;
		node->wildcard = true;
	}
	return node;
}

struct TreeNode*
removeChildren(struct TreeNode* node){
	node->children = NULL;
	return node;
}