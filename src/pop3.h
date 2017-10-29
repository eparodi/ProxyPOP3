#ifndef TPE_PROTOS_POP3_H
#define TPE_PROTOS_POP3_H

#include <netdb.h>
#include "selector.h"

/** handler del socket pasivo que atiende conexiones pop3 */
void
pop3_passive_accept(struct selector_key *key);


/** libera pools internos */
void
pop3_pool_destroy(void);

#endif //TPE_PROTOS_POP3_H
