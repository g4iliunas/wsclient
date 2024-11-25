#include "src/wsclient.h"
#include <assert.h>
#include <event2/event.h>
#include <openssl/ssl.h>

int main(void)
{
    // initialize openssl
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    struct event_base *base = event_base_new();
    assert(base != NULL);

    // connecting to gateway.discord.gg
    assert(wsclient_init(base, "162.159.133.234", 443) == 0);

    event_base_dispatch(base);
    event_base_free(base);
    EVP_cleanup();
    return 0;
}