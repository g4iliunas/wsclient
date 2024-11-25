#include "src/wsclient.h"
#include "src/yyjson.h"
#include <assert.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <openssl/ssl.h>
#include <stdio.h>

static void handle_json(yyjson_val *root)
{
    printf("got root\n");
    yyjson_val *op_val = yyjson_obj_get(root, "op");
    uint8_t op = yyjson_get_uint(op_val);
    printf("discord opcode: %u\n", op);

    yyjson_val *d_val = yyjson_obj_get(root, "d");
    if (d_val) {
        yyjson_val *hb_int_val = yyjson_obj_get(d_val, "heartbeat_interval");
        uint32_t hb_int = yyjson_get_uint(hb_int_val);
        printf("discord heartbeat interval: %u\n", hb_int);
    }
}

static void
readcb(struct bufferevent *bev, const uint8_t *data, const size_t len)
{
    printf("Got data\n");
    printf("%.*s\n", (int)len, data);

    if (data[0] == 'H' && data[1] == 'T' && data[2] == 'T' && data[3] == 'P') {
        // check where | resides
        size_t i = 0;
        while (i < len) {
            if (data[i] == '|')
                break;
            i++;
        }

        const char *raw_json = (char *)data + i + 1;
        size_t raw_json_len = len - i - 1;

        printf("json: %.*s\n", (int)raw_json_len, raw_json);

        yyjson_doc *doc = yyjson_read(raw_json, raw_json_len, 0);
        assert(doc != NULL);
        yyjson_val *root = yyjson_doc_get_root(doc);
        handle_json(root);
        yyjson_doc_free(doc);
    }
}

static void eventcb(struct bufferevent *bev, short events, void *ctx)
{
    if (events & BEV_EVENT_CONNECTED) {
        printf("connected\n");
    }
    else if (events & BEV_EVENT_ERROR) {
        printf("got error\n");
        event_base_loopexit(bufferevent_get_base(bev), NULL);
    }
    else if (events & BEV_EVENT_ERROR) {
        printf("connection closed\n");
        event_base_loopexit(bufferevent_get_base(bev), NULL);
    }
}

int main(void)
{
    // initialize openssl
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    struct event_base *base = event_base_new();
    assert(base != NULL);

    struct wsclient_ctx ctx;
    ctx.readcb = readcb;
    ctx.writecb = NULL;
    ctx.eventcb = eventcb;
    ctx.ctx = NULL;

    // connecting to gateway.discord.gg
    assert(wsclient_init(base, "wss://162.159.133.234", 443, &ctx) == 0);

    event_base_dispatch(base);
    event_base_free(base);
    EVP_cleanup();
    return 0;
}