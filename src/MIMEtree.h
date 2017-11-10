struct MIMEtype{
	char* type;
	char* subtype;
}


struct Tree* tree_init();

void add(struct Tree* tree; struct MIMEtype* mime);