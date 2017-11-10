#ifndef TPE_PROTOS_UTILS_H
#define TPE_PROTOS_UTILS_H

#define SOCKADDR_TO_HUMAN_MIN (INET6_ADDRSTRLEN + 5 + 1)

/**
 * Describe de forma humana un sockaddr:
 *
 * @param buff     el buffer de escritura
 * @param buffsize el tamaño del buffer  de escritura
 *
 * @param af    address family
 * @param addr  la dirección en si
 * @param nport puerto en network byte order
 *
 */
const char *
sockaddr_to_human(char *buff, const size_t buffsize,
                  const struct sockaddr *addr);

void print_connection_status(const char * msg, struct sockaddr_storage addr);

#endif //TPE_PROTOS_UTILS_H
