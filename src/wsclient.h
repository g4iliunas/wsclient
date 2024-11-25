#ifndef WSCLIENT_H
#define WSCLIENT_H

#include <event2/bufferevent.h>
#include <event2/event.h>
#include <stdint.h>

typedef void (*wsclient_readcb)(
    struct bufferevent *bev, const uint8_t *data, const size_t len);

typedef void (*wsclient_writecb)(
    struct bufferevent *bev, const uint8_t *data, const size_t len);

struct wsclient_ctx {
    wsclient_readcb readcb;
    bufferevent_data_cb writecb;
    bufferevent_event_cb eventcb;
    void *ctx;
};

int wsclient_init(
    struct event_base *base, const char *address, const uint16_t port,
    struct wsclient_ctx *cbarg);

#endif