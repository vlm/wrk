#ifndef WRK_CFG_H
#define WRK_CFG_H

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>

#define LOCAL_ADDRSTRLEN sizeof(((struct sockaddr_un *)0)->sun_path)

/*
 * A program-wide configuration.
 */
struct wrk_config {
    char sock_path[LOCAL_ADDRSTRLEN];
    char *paths;
    uint64_t threads;
    uint64_t connections;
    uint64_t requests;
    uint64_t timeout;
    uint8_t use_keepalive;
    uint8_t use_sock;
};


struct wrk_destination {
    struct addrinfo addr;
    struct sockaddr_un un;  // addr->ai_addr _may_ point to this field.
    char *host;
    char *port;
    char *path;
};

/*
 * Using (cfg->sock_path) or (url) (depending on cfg->use_sock), resolve
 * and fill in the destination address structure.
 * On success, the (*path) will be set to the appropriate pathname as given
 * on the command line or extracted from the command line URL.
 */
struct wrk_destination cfg_resolve_destination(
            struct wrk_config *, const char *url);

#endif  /* WRK_CFG_H */
