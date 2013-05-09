#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "zmalloc.h"
#include "http_parser.h"
#include "wrk_config.h"

static char *extract_url_part(const char *url, struct http_parser_url *parser_url, enum http_parser_url_fields field);

struct wrk_destination
cfg_resolve_destination(struct wrk_config *cfg, const char *url) {
    struct wrk_destination dst;
    struct addrinfo *addrs, *addr;
    struct http_parser_url parser_url;
    int rc;

    memset(&dst, 0, sizeof(dst));
    char *service = NULL;

    if (cfg->use_sock) {
        /* Unix socket */
        struct sockaddr_un un;
#ifdef BSD
        un.sun_len = sizeof(un);
#endif
        un.sun_family = AF_LOCAL;
        strncpy(un.sun_path, cfg->sock_path, LOCAL_ADDRSTRLEN);

        dst.host = cfg->sock_path;
        dst.port = "0";
        service = "http";
        dst.path = zcalloc(strlen(url) + 1);
        strcpy(dst.path, url);

        int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (fd == -1) {
            fprintf(stderr, "unable to create socket: %s\n", strerror(errno));
            exit(1);
        }

        if (connect(fd, (struct sockaddr *)&un, sizeof(un)) == -1) {
            fprintf(stderr, "unable to connect socket: %s\n", strerror(errno));
            close(fd);
            exit(1);
        }

        close(fd);

        dst.un = un;
        dst.addr.ai_addr = (struct sockaddr *)&dst.un;
        dst.addr.ai_addrlen = sizeof(dst.un);
        dst.addr.ai_family = AF_LOCAL;
        dst.addr.ai_socktype = SOCK_STREAM;
        dst.addr.ai_protocol = 0;
        dst.addr.ai_next = NULL;
    } else {
        /* TCP socket */
        if (http_parser_parse_url(url, strlen(url), 0, &parser_url)) {
            fprintf(stderr, "invalid URL: %s\n", url);
            exit(1);
        }

        dst.host = extract_url_part(url, &parser_url, UF_HOST);
        dst.port = extract_url_part(url, &parser_url, UF_PORT);
        service = dst.port ? dst.port : extract_url_part(url, &parser_url, UF_SCHEMA);
        dst.path = zcalloc(strlen(url) + 1);
        strcpy(dst.path, &url[parser_url.field_data[UF_PATH].off]);

        struct addrinfo hints = {
            .ai_family   = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
        };

        if ((rc = getaddrinfo(dst.host, service, &hints, &addrs)) != 0) {
            const char *msg = gai_strerror(rc);
            fprintf(stderr, "unable to resolve %s:%s %s\n", dst.host, service, msg);
            exit(1);
        }

        for (addr = addrs; addr != NULL; addr = addr->ai_next) {
            int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (fd == -1) continue;
            if (connect(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
                if (errno == EHOSTUNREACH || errno == ECONNREFUSED) {
                    close(fd);
                    continue;
                }
            }
            close(fd);
            break;
        }

        if (addr == NULL) {
            char *msg = strerror(errno);
            fprintf(stderr, "unable to connect to %s:%s %s\n", dst.host, service, msg);
            exit(1);
        }

        dst.addr = *addr;
        dst.addr.ai_next = 0;
        addr->ai_addr = 0;
        freeaddrinfo(addrs);
    }

    return dst;
}

static char *extract_url_part(const char *url, struct http_parser_url *parser_url, enum http_parser_url_fields field) {
    char *part = NULL;

    if (parser_url->field_set & (1 << field)) {
        uint16_t off = parser_url->field_data[field].off;
        uint16_t len = parser_url->field_data[field].len;
        part = zcalloc(len + 1);
        memcpy(part, &url[off], len);
    }

    return part;
}
