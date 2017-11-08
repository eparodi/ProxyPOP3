#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "external_transformation.h"
#include "parameters.h"
#include "pop3_session.h"
#include "selector.h"
#define ATTACHMENT(key) ( (struct pop3 *)(key)->data)


