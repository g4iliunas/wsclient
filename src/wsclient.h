#ifndef WSCLIENT_H
#define WSCLIENT_H

#include <event2/event.h>
#include <stdint.h>

int wsclient_init(
    struct event_base *base, const char *address, const uint16_t port);

#endif