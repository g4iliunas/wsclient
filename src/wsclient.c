#include "wsclient.h"
#include <arpa/inet.h>
#include <bits/types/struct_iovec.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <stdio.h>

#include <fcntl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdbool.h>

static int wsclient_send_handshake(struct bufferevent *bev)
{
    // UXXsy2TW7eYLBnmyDnOCrQ==

    const char buf[] = "GET /?encoding=json&v=9 HTTP/1.1\r\n"
                       "Host: gateway.discord.gg\r\n"
                       //"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:132.0)
                       // Gecko/20100101 " "Firefox/132.0\r\n"
                       //"Accept: */*\r\n"
                       "Accept-Language: en-US,en;q=0.5\r\n"
                       "Accept-Encoding: gzip, deflate, br, zstd\r\n"
                       "Sec-WebSocket-Version: 13\r\n"
                       "Origin: https://discord.com\r\n"
                       //"Sec-WebSocket-Extensions: permessage-deflate\r\n"
                       "Sec-WebSocket-Key: UXXsy2TW7eYLBnmyDnOCrQ==\r\n"
                       "Connection: keep-alive, Upgrade\r\n"
                       //"Sec-Fetch-Dest: empty\r\n"
                       //"Sec-Fetch-Mode: websocket\r\n"
                       //"Sec-Fetch-Site: cross-site\r\n"
                       "Pragma: no-cache\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Upgrade: websocket\r\n\r\n";

    return !(bufferevent_write(bev, buf, sizeof(buf) - 1) == 0);
}

static void readcb(struct bufferevent *bev, void *ctx)
{
    struct wsclient_ctx *cbarg = (struct wsclient_ctx *)ctx;

    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    struct evbuffer_iovec vec[1];
    evbuffer_peek(input, -1, NULL, vec, 1);
    char *data = (char *)vec[0].iov_base;

    if (data[0] == 'H' && data[1] == 'T' && data[2] == 'T' && data[3] == 'P') {
        if (cbarg->readcb)
            cbarg->readcb(bev, (uint8_t *)data, len);
        return;
    }

    fprintf(stdout, "Got %lu bytes of data\n", len);
    fprintf(stdout, "%.*s\n", (int)vec[0].iov_len, data);

    // parse frame
    uint8_t fin = data[0] >> 7;
    uint8_t opcode = data[0] & 0xf;
    evbuffer_drain(input, len);
}

static void writecb(struct bufferevent *bev, void *ctx)
{
    struct wsclient_ctx *cbarg = (struct wsclient_ctx *)ctx;
    if (cbarg->writecb)
        cbarg->writecb(bev, cbarg->ctx);
}

static void eventcb(struct bufferevent *bev, short events, void *ctx)
{
    struct wsclient_ctx *cbarg = (struct wsclient_ctx *)ctx;

    if (events & BEV_EVENT_CONNECTED) {
        fprintf(stdout, "Connected to the server\n");
        if (wsclient_send_handshake(bev) != 0) {
            fprintf(stderr, "Failed to send the handshake\n");
            if (cbarg->eventcb)
                cbarg->eventcb(bev, BEV_EVENT_ERROR, ctx);
        }
        else {
            if (cbarg->eventcb)
                cbarg->eventcb(bev, events, ctx);
        }
    }
    else {
        if (cbarg->eventcb)
            cbarg->eventcb(bev, events, ctx);
    }
}

int wsclient_init(
    struct event_base *base, const char *address, const uint16_t port,
    struct wsclient_ctx *cbarg)
{
    bool is_ssl = address[2] == 's';
    address += is_ssl ? 6 : 5;

    struct sockaddr_in sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &sin.sin_addr) != 1)
        return 1;

    const SSL_METHOD *method;
    SSL_CTX *ctx;
    SSL *ssl;

    if (is_ssl) {
        method = SSLv23_client_method();
        ctx = SSL_CTX_new(method);
        if (!ctx)
            return 1;

        ssl = SSL_new(ctx);
        if (!ssl) {
            SSL_CTX_free(ctx);
            return 1;
        }
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        if (is_ssl) {
            SSL_CTX_free(ctx);
            SSL_free(ssl);
        }
        return 1;
    }

    // first we connect then we set the socket to nonblockable
    if (connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)) != 0 ||
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        if (is_ssl) {
            SSL_CTX_free(ctx);
            SSL_free(ssl);
        }
        close(sockfd);
        return 1;
    }

    struct bufferevent *bev =
        is_ssl ? bufferevent_openssl_socket_new(
                     base, sockfd, ssl, BUFFEREVENT_SSL_CONNECTING,
                     BEV_OPT_CLOSE_ON_FREE)
               : bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);

    if (!bev) {
        if (is_ssl) {
            SSL_CTX_free(ctx);
            SSL_free(ssl);
        }
        close(sockfd);
        return 1;
    }

    // because that doesnt trigger connect event
    if (!is_ssl)
        if (wsclient_send_handshake(bev) != 0) {
            if (is_ssl) {
                SSL_CTX_free(ctx);
                SSL_free(ssl);
            }
            close(sockfd);
            return 1;
        }

    bufferevent_setcb(bev, readcb, NULL, eventcb, cbarg);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    return 0;
}