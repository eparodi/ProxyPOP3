#include <stdlib.h>
#include "MIMEtree.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include "parser_utils.h"

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
		struct parser_definition * def = malloc(sizeof(*def));
		if (def == NULL) {
			free(node);
			return NULL;
		}

		struct parser_definition aux = parser_utils_strcmpi(name);
		memcpy(def, &aux, sizeof(aux));

		//parser_utils_strcmpi_destroy(&aux);


        //const unsigned int* no_class = parser_no_classes();
		node->parser = parser_init(parser_no_classes(), def);
		node->def	= def;
		node->next = NULL;
		node->children = NULL;
		node->name = name;
		node->event = NULL;
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
		node->def = NULL;
		node->next = NULL;
		node->children = NULL;
		node->name = WILDCARD;
		node->event = NULL;
		node->wildcard = true;
	}
	return node;
}

struct TreeNode*
removeChildren(struct TreeNode* node){
	struct TreeNode* aux = node->children;
	//struct TreeNode* tmp;
	while(aux->next!=NULL){
		//tmp = aux;
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
		fprintf(stderr,"Created new Node: %s/*\n", type);
	}else{
		node->next = newNode(type);
		node->next->children = newNodeWildcard();
		fprintf(stderr,"Created new Node: %s/*\n", type);
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
				fprintf(stderr,"Created new Node:%s/%s\n",type,subtype);
			}else{
				tree->first->children = newNode(subtype);
				fprintf(stderr,"Created new Node:%s/%s\n",type,subtype);
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
				fprintf(stderr,"Filter already added\n");
				return;
			}else{
				fprintf(stderr,"Added new subtype to %s/%s\n",type,subtype);
				node->next = newNode(subtype);
			}
		}else{
			node->next = newNode(type);
			node->next->children = newNode(subtype);
			fprintf(stderr,"Created new Node:%s/%s\n",type,subtype);
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

void mime_parser_destroy(struct Tree *mime_tree){
    struct TreeNode* node = mime_tree->first;
    struct TreeNode* children;
    struct TreeNode* tmp;
    while(node != NULL){
        children = node->children;
        while(children != NULL){
            tmp = children;
            if(children->parser != NULL){
                parser_destroy(children->parser);
                parser_utils_strcmpi_destroy(children->def);
            }
            children = children->next;
            free(tmp);
        }
        tmp = node;
        parser_destroy(node->parser);
        parser_utils_strcmpi_destroy(node->def);
        node = node->next;
        free(tmp);
    }
    free(mime_tree);
}

void
mime_parser_reset(struct Tree* mime_tree){
    struct TreeNode* node = mime_tree->first;
    struct TreeNode* children;
    while(node != NULL){
        children = node->children;
        while(children != NULL){
			if (!children->wildcard) {
				parser_reset(children->parser);
			}
            children = children->next;
        }
        parser_reset(node->parser);
        node = node->next;
    }
}

