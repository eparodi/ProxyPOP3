#include <stdlib.h>
#define WILDCARD '*'


struct MIMEtree{
	struct MIMEtreeNode* first;
}

struct MIMEtreeNode{
	parser* parser;
	struct MIMEtreeNode *next;
	struct MIMEtreeNode *children;
	const char* name;
	bool match;
	bool wildcard;
}


struct MIMEtree*
tree_init(){
	struct MIMEtree* tree = malloc(sizeof(*tree));
	if(tree != NULL){
		tree->first = malloc(sizeof(*first));
		if(first != NULL){
			return tree;
		}
	}
	return NULL;
}

struct MIMEtreeNode*
findTypeMatch(struct MIMEtree* tree, char* type, bool* found){
	struct MIMEtreeNode* node = tree->first;
	while(node != NULL){
		if(node->name == type){
			*found = true;
			return node;
		}
		node = node->next;
	}
	return node;
}

struct MIMEtreeNode*
findSubTypeMatch(struct MIMEtreeNode* node, char* subtype, bool* found){
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

void addWildcard(struct MIMEtree* tree, char* type){	
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
add(struct MIMEtree* tree, char* type, char*subtype) {
	if(tree!=NULL){
	if(subtype == WILDCARD){
		addWildcard(tree,type);
	}else{
		bool found = false;
		struct MIMEtreeNode* node = findTypeMatch(tree,type, &found);
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

struct MIMEtreeNode*
newNode(char* name){
	struct MIMEtreeNode* node = malloc(sizeof(*node));
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

struct MIMEtreeNode*
newNodeWildcard(){
	struct MIMEtreeNode* node = malloc(sizeof(*node));
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

struct MIMEtreeNode*
removeChildren(struct MIMEtreeNode* node){
	node->children = NULL;
	return node;
}