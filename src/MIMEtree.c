#include <stdlib.h>
#include "MIMEtree.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include "parser_utils.h"
#include "mime_chars.h"
#define WILDCARD "*"



struct Tree*
tree_init(void){
	struct Tree* tree = malloc(sizeof(*tree));
	if(tree != NULL){
        memset(tree, 0, sizeof(*tree));
    }
    return tree;

	}

struct TreeNode*
newNode(char* name){
	struct TreeNode* node = malloc(sizeof(*node));
	if(node != NULL){
		memset(node,0,sizeof(*node));
		const struct parser_definition parser = parser_utils_strcmpi(name);
		node->parser = parser_init(init_char_class(),&parser);
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
		memset(node,0,sizeof(*node));
		node->parser = NULL;
		node->next = NULL;
		node->children = NULL;
		node->name = WILDCARD;
		node->match = true;
		node->wildcard = true;
	}
	return node;
}

struct TreeNode*
removeChildren(struct TreeNode* node){
	struct TreeNode* aux = node->children;
	struct TreeNode* tmp;
	while(aux->next!=NULL){
		tmp = aux;
		aux = aux->next;
		free(aux);
	}
	free(node->children);
	node->children = NULL;
	return node;
}

bool
checkTypeMatch(struct TreeNode* node, char* type, bool* found){
	if(strcmp(node->name,type) == 0){
		*found = true;
	}
	return *found;
}

struct TreeNode*
findTypeMatch(struct Tree* tree, char* type, bool* found){
	struct TreeNode* node = tree->first;
	if(checkTypeMatch(node,type,found)){
		return node;
	}
	while(node->next != NULL){
		node = node->next;
		if(checkTypeMatch(node,type,found)){
			return node;
		}
	}
	return node;
}


bool
checkSubTypeMatch(struct TreeNode* node, char* subtype, bool* found){
	if(node->wildcard || strcmp(node->name,subtype) == 0){
		*found = true;
	}
	return *found;
}

struct TreeNode*
findSubTypeMatch(struct TreeNode* node, char* subtype, bool* found){
	if(checkSubTypeMatch(node,subtype,found)){
		return node;
	}
	while(node->next != NULL){
		node = node->next;
		if(checkSubTypeMatch(node,subtype,found)){
			return node;
		}
	}
	return node;
}

void addWildcard(struct TreeNode* node, char* type, bool found){	
	if(found){
		removeChildren(node);
		node->children = newNodeWildcard();
		printf("Created new Node: %s/*\n", type);
	}else{
		node->next = newNode(type);
		node->next->children = newNodeWildcard();
		printf("Created new Node: %s/*\n", type);
	}
	return;
}

void
addNode(struct Tree* tree, char* type, char* subtype) {
	if(tree!=NULL){
		if(tree->first == NULL){
			tree->first = newNode(type);
			if(strcmp(subtype,WILDCARD)==0){
				tree->first->children = newNodeWildcard();
				printf("Created new Node:%s/%s\n",type,subtype);
			}else{
				tree->first->children = newNode(subtype);
				printf("Created new Node:%s/%s\n",type,subtype);
			}
			return;
		}
		bool found = false;
		struct TreeNode* node = findTypeMatch(tree,type,&found);
	if(strcmp(subtype,WILDCARD) == 0){
		addWildcard(node,type,found);
	}else{
		if(found){
			found = false;
			node = findSubTypeMatch(node->children, subtype, &found);
			if(found){
				printf("Filter already added\n");
				return;
			}else{
				printf("Added new subtype to %s/%s\n",type,subtype);
				node->next = newNode(subtype);
			}
		}else{
			node->next = newNode(type);
			node->next->children = newNode(subtype);
			printf("Created new Node:%s/%s\n",type,subtype);	
		}
	}
	return;	
	}
}

void
removeNode(struct Tree* tree, char* type, char*subtype){
	struct TreeNode* nodeType = tree->first;
	struct TreeNode* tmpType;
	struct TreeNode* tmpSubType;
	struct TreeNode* nodeSubType;
	if(nodeType == NULL){
		return;
	}
	if(strcmp(nodeType->name,type) == 0){
			nodeSubType = nodeType->children;
			if(strcmp(nodeSubType->name,subtype) == 0){
				nodeType->children = nodeSubType->next;
				free(nodeSubType);
			}
			while(nodeSubType->next != NULL){
				tmpSubType = nodeSubType;
				nodeSubType = nodeSubType->next;
				if(strcmp(nodeSubType->name,subtype)==0){
					tmpSubType->next = nodeSubType->next;
					free(nodeSubType);
				}
			}
			if(nodeType->children == NULL){
				tree->first = nodeType->next;
				free(nodeType);
				return;
			}
	while(nodeType->next!= NULL){
		tmpType = nodeType;
		nodeType = nodeType->next;
		if(strcmp(nodeType->name,type) == 0){
			nodeSubType = nodeType->children;
			if(strcmp(nodeSubType->name,subtype) == 0){
				nodeType->children = nodeSubType->next;
				free(nodeSubType);
			}
			while(nodeSubType->next != NULL){
				tmpSubType = nodeSubType;
				nodeSubType = nodeSubType->next;
				if(strcmp(nodeSubType->name,subtype)==0){
					tmpSubType->next = nodeSubType->next;
					free(nodeSubType);
				}
			}
			if(nodeType->children == NULL){
				tmpType->next = nodeType->next;
				free(nodeType);
				return;
			}
		}
	}
}
}

