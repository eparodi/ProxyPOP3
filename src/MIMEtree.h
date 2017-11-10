struct MIMEtype{
	char* type;
	char* subtype;
}


struct MIMEtree* tree_init();

void add(struct MIMEtree* tree; struct MIMEtype* mime);