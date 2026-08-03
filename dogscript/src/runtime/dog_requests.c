#include "dog_runtime.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef DOG_HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

typedef struct {
    char scheme[8];
    char host[256];
    int port;
    char path[2048];
    int https;
} DogUrl;

typedef struct {
    int sock;
#ifdef DOG_HAVE_OPENSSL
    SSL_CTX *ctx;
    SSL *ssl;
#endif
    int https;
} DogConn;

static DogValue *alloc_v(DogValue v) {
    DogValue *p = (DogValue *)malloc(sizeof(DogValue));
    if (!p) return NULL;
    *p = v;
    return p;
}

static void map_set_int(DogMap *m, const char *k, int v) { dog_map_set(m, k, alloc_v(dog_v_int(v))); }
static void map_set_bool(DogMap *m, const char *k, int v) { dog_map_set(m, k, alloc_v(dog_v_bool(v))); }
static void map_set_str(DogMap *m, const char *k, const char *v) { dog_map_set(m, k, alloc_v(dog_v_str(v))); }
static void map_set_map(DogMap *m, const char *k, DogMap *v) { dog_map_set(m, k, alloc_v(dog_v_map(v))); }

static int parse_url(const char *url, DogUrl *out) {
    const char *p, *host_start, *path_start, *port_sep;
    size_t host_len;
    memset(out, 0, sizeof(*out));
    strcpy(out->scheme, "http");
    out->port = 80;
    out->https = 0;

    p = strstr(url, "://");
    host_start = url;
    if (p) {
        size_t n = (size_t)(p - url);
        if (n < sizeof(out->scheme)) memcpy(out->scheme, url, n);
        out->scheme[n] = '\0';
        host_start = p + 3;
        if (strcmp(out->scheme, "https") == 0) {
            out->https = 1;
            out->port = 443;
        }
    }
    path_start = strchr(host_start, '/');
    if (!path_start) path_start = host_start + strlen(host_start);
    port_sep = memchr(host_start, ':', (size_t)(path_start - host_start));
    if (port_sep) {
        host_len = (size_t)(port_sep - host_start);
        out->port = atoi(port_sep + 1);
    } else {
        host_len = (size_t)(path_start - host_start);
    }
    if (host_len == 0 || host_len >= sizeof(out->host)) return 0;
    memcpy(out->host, host_start, host_len);
    out->host[host_len] = '\0';
    if (*path_start) {
        strncpy(out->path, path_start, sizeof(out->path) - 1);
    } else {
        strcpy(out->path, "/");
    }
    return 1;
}

static int conn_open(DogConn *c, const DogUrl *u) {
    struct addrinfo hints, *res, *it;
    char port_buf[16];
    int rc;
    memset(c, 0, sizeof(*c));
    c->sock = -1;
    c->https = u->https;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_buf, sizeof(port_buf), "%d", u->port);
    rc = getaddrinfo(u->host, port_buf, &hints, &res);
    if (rc != 0) return 0;
    for (it = res; it; it = it->ai_next) {
        c->sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (c->sock < 0) continue;
        if (connect(c->sock, it->ai_addr, it->ai_addrlen) == 0) break;
        close(c->sock);
        c->sock = -1;
    }
    freeaddrinfo(res);
    if (c->sock < 0) return 0;
#ifdef DOG_HAVE_OPENSSL
    if (u->https) {
        SSL_library_init();
        SSL_load_error_strings();
        c->ctx = SSL_CTX_new(TLS_client_method());
        if (!c->ctx) return 0;
        c->ssl = SSL_new(c->ctx);
        if (!c->ssl) return 0;
        SSL_set_tlsext_host_name(c->ssl, u->host);
        SSL_set_fd(c->ssl, c->sock);
        if (SSL_connect(c->ssl) <= 0) return 0;
    }
#else
    if (u->https) return 0;
#endif
    return 1;
}

static void conn_close(DogConn *c) {
#ifdef DOG_HAVE_OPENSSL
    if (c->ssl) SSL_free(c->ssl);
    if (c->ctx) SSL_CTX_free(c->ctx);
#endif
    if (c->sock >= 0) close(c->sock);
}

static ssize_t conn_write(DogConn *c, const void *buf, size_t n) {
#ifdef DOG_HAVE_OPENSSL
    if (c->https) return SSL_write(c->ssl, buf, (int)n);
#endif
    return send(c->sock, buf, n, 0);
}

static ssize_t conn_read(DogConn *c, void *buf, size_t n) {
#ifdef DOG_HAVE_OPENSSL
    if (c->https) return SSL_read(c->ssl, buf, (int)n);
#endif
    return recv(c->sock, buf, n, 0);
}

static char *recv_all(DogConn *c, size_t *out_len) {
    size_t cap = 8192, len = 0;
    char *out = (char *)malloc(cap);
    for (;;) {
        char tmp[4096];
        ssize_t n = conn_read(c, tmp, sizeof(tmp));
        if (n <= 0) break;
        if (len + (size_t)n + 1 > cap) {
            while (len + (size_t)n + 1 > cap) cap *= 2;
            out = (char *)realloc(out, cap);
        }
        memcpy(out + len, tmp, (size_t)n);
        len += (size_t)n;
    }
    out[len] = '\0';
    if (out_len) *out_len = len;
    return out;
}

static DogMap *parse_response(const char *raw, size_t n) {
    const char *sep = strstr(raw, "\r\n\r\n");
    DogMap *res = dog_map_new();
    DogMap *hdrs = dog_map_new();
    int status = 0;
    char *head;
    if (!res || !hdrs) return NULL;
    if (!sep) {
        map_set_int(res, "status_code", 0);
        map_set_map(res, "headers", hdrs);
        map_set_str(res, "body", raw);
        map_set_bool(res, "ok", 0);
        return res;
    }
    {
        const char *sp = strchr(raw, ' ');
        if (sp) status = atoi(sp + 1);
    }
    head = dog_strndup(raw, (size_t)(sep - raw));
    {
        char *save = NULL;
        char *line = strtok_r(head, "\r\n", &save);
        line = strtok_r(NULL, "\r\n", &save);
        while (line) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                while (*(colon + 1) == ' ') colon++;
                dog_map_set(hdrs, line, alloc_v(dog_v_str(colon + 1)));
            }
            line = strtok_r(NULL, "\r\n", &save);
        }
    }
    free(head);
    map_set_int(res, "status_code", status);
    map_set_map(res, "headers", hdrs);
    map_set_str(res, "body", sep + 4);
    map_set_bool(res, "ok", status > 0 && status < 400);
    (void)n;
    return res;
}

static void append_headers(char *buf, size_t cap, const DogMap *headers) {
    size_t i;
    if (!headers) return;
    for (i = 0; i < headers->len; i++) {
        DogString *k = dog_map_get_key(headers, i);
        DogValue *v = (DogValue *)dog_map_get_value(headers, i);
        if (!k || !v || v->type != DOG_STR) continue;
        strncat(buf, k->data, cap - strlen(buf) - 1);
        strncat(buf, ": ", cap - strlen(buf) - 1);
        strncat(buf, v->as.s->data, cap - strlen(buf) - 1);
        strncat(buf, "\r\n", cap - strlen(buf) - 1);
    }
}

static DogMap *http_request(const char *method, const char *url, const char *body, const DogMap *headers) {
    DogUrl u;
    DogConn c;
    char req[32768];
    char len_buf[64];
    char *raw;
    size_t raw_n;
    if (!parse_url(url, &u)) {
        DogMap *err = dog_map_new();
        map_set_int(err, "status_code", 0);
        map_set_str(err, "body", "invalid url");
        map_set_map(err, "headers", dog_map_new());
        map_set_bool(err, "ok", 0);
        return err;
    }
    if (!conn_open(&c, &u)) {
        DogMap *err = dog_map_new();
        map_set_int(err, "status_code", 0);
        map_set_str(err, "body", "connection failed");
        map_set_map(err, "headers", dog_map_new());
        map_set_bool(err, "ok", 0);
        return err;
    }
    req[0] = '\0';
    snprintf(req, sizeof(req), "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n",
             method, u.path, u.host);
    append_headers(req, sizeof(req), headers);
    if (body) {
        snprintf(len_buf, sizeof(len_buf), "%zu", strlen(body));
        strncat(req, "Content-Length: ", sizeof(req) - strlen(req) - 1);
        strncat(req, len_buf, sizeof(req) - strlen(req) - 1);
        strncat(req, "\r\n", sizeof(req) - strlen(req) - 1);
    }
    strncat(req, "\r\n", sizeof(req) - strlen(req) - 1);
    if (body) strncat(req, body, sizeof(req) - strlen(req) - 1);
    conn_write(&c, req, strlen(req));
    raw = recv_all(&c, &raw_n);
    conn_close(&c);
    {
        DogMap *resp = parse_response(raw, raw_n);
        free(raw);
        return resp;
    }
}

DogMap *dog_requests_get(const char *url, const DogMap *headers) {
    return http_request("GET", url, NULL, headers);
}
DogMap *dog_requests_post(const char *url, const char *body, const DogMap *headers) {
    return http_request("POST", url, body ? body : "", headers);
}
DogMap *dog_requests_put(const char *url, const char *body, const DogMap *headers) {
    return http_request("PUT", url, body ? body : "", headers);
}
DogMap *dog_requests_patch(const char *url, const char *body, const DogMap *headers) {
    return http_request("PATCH", url, body ? body : "", headers);
}
DogMap *dog_requests_delete(const char *url, const DogMap *headers) {
    return http_request("DELETE", url, NULL, headers);
}

/* Optional aliases if the bridge maps __http_* symbols directly. */
DogMap *dog_http_get(const char *url, const DogMap *headers) { return dog_requests_get(url, headers); }
DogMap *dog_http_post(const char *url, const char *body, const DogMap *headers) { return dog_requests_post(url, body, headers); }
DogMap *dog_http_put(const char *url, const char *body, const DogMap *headers) { return dog_requests_put(url, body, headers); }
DogMap *dog_http_patch(const char *url, const char *body, const DogMap *headers) { return dog_requests_patch(url, body, headers); }
DogMap *dog_http_delete(const char *url, const DogMap *headers) { return dog_requests_delete(url, headers); }
