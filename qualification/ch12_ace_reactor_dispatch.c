/*
 * ACE Reactor-style application dispatch with real CNet TCP readiness,
 * Acceptor-Connector, and one exact CMeta typed Observer.
 *
 * One caller drives cnet_client_poll() and dispatches completed I/O events
 * inline. CNet itself observes NativeIO completions internally: this is NOT
 * a second, independently implemented kernel Reactor.
 *
 * All Interface, observer and connection lifetimes are explicit. No Actor,
 * additional I/O engine, background worker, retry, or source-tree headers.
 */
#include <cnet/cnet.h>
#include <cmeta/interface.h>
#include <salts/clock.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum { BOOK_WAIT_MS = 7000 };

#define BOOK_REACTOR_STATE_METHODS(X, I) \
    X(I, FV1, void, observe, stateful, \
      &cmeta_type_void, CMETA_ABI_VOID, \
      (int, state, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR))

CMETA_INTERFACE(book_reactor_state_handler, BOOK_REACTOR_STATE_METHODS);

typedef struct book_reactor_probe {
    const void *owner;
    unsigned connected;
    unsigned terminal;
    unsigned failed;
    unsigned callbacks;
    unsigned callback_wrong_owner;
    unsigned callback_after_expiry;
    bool live;
} book_reactor_probe;

static void book_reactor_observe(void *self, int state)
{
    book_reactor_probe *probe = (book_reactor_probe *)self;
    ++probe->callbacks;
    if (!probe->live)
        ++probe->callback_after_expiry;
    if (probe->owner != cmeta_thread_current_token())
        ++probe->callback_wrong_owner;
    if (state == CNET_CONNECTION_CONNECTED)
        ++probe->connected;
    if (state == CNET_CONNECTION_CLOSED || state == CNET_CONNECTION_FAILED)
        ++probe->terminal;
    if (state == CNET_CONNECTION_FAILED)
        ++probe->failed;
}

CMETA_IMPLEMENTS(book_reactor_state_handler, book_reactor_counter_impl, 0u,
                 .observe = book_reactor_observe);

typedef struct book_reactor_binding {
    book_reactor_state_handler handler; /* borrowed while CNet is live */
    unsigned errors;
} book_reactor_binding;

/* The only CNet -> CMeta bridge. The exact native observer signature is
 * supplied by CNet, then dispatched through the reflected Interface.
 */
static void book_reactor_on_state(
    void *user, cnet_connection connection, cnet_connection_state state,
    const cnet_error *error)
{
    book_reactor_binding *binding = (book_reactor_binding *)user;
    (void)connection;
    if (error != NULL)
        ++binding->errors;
    book_reactor_state_handler_observe(&binding->handler, (int)state);
}

static native_io_backend_kind book_backend(void)
{
#if defined(_WIN32)
    return NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#else
    return NATIVE_IO_BACKEND_EPOLL;
#endif
}

static cnet_client_config book_client_config(void)
{
    cnet_client_config config = {0};
    config.backend = book_backend();
    config.connection_capacity = 2u;
    config.command_capacity = 8u;
    config.request_capacity = 8u;
    config.completion_batch_capacity = 8u;
    config.event_capacity = 8u;
    config.max_send_bytes = 1024u;
    config.receive_buffer_bytes = 1024u;
    config.connect_timeout_ms = BOOK_WAIT_MS;
    return config;
}

static int book_progress(cnet_client *client)
{
    size_t events = 0u;
    return cnet_client_poll(client, 1u, &events);
}

#define BOOK_REQUIRE(expr, code) do {                                \
    if (!(expr)) {                                                   \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);   \
        result = (code);                                             \
        goto finish;                                                 \
    }                                                               \
} while (0)

int main(void)
{
    const cnet_client_config config = book_client_config();
    const cnet_listener_config listen_config = {
        book_backend(), "127.0.0.1", 0u, 8u
    };
    cnet_client connector = {0};
    cnet_client acceptor = {0};
    cnet_listener listener = {0};
    cnet_connection outbound = {0}, inbound = {0}, rejected = {0};
    cnet_connect_options connect = {0}, invalid = {0};
    book_reactor_probe outgoing = {0}, incoming = {0};
    book_reactor_binding outgoing_binding = {0}, incoming_binding = {0};
    cnet_observer outgoing_observer = {0}, incoming_observer = {0};
    uint64_t deadline;
    uint16_t port = 0u;
    char uri[64];
    int ready = 0;
    int result = 0;
    bool accepted = false;

    outgoing.owner = cmeta_thread_current_token();
    incoming.owner = outgoing.owner;
    outgoing.live = incoming.live = true;

    outgoing_binding.handler =
        book_reactor_counter_impl_as_book_reactor_state_handler(&outgoing);
    incoming_binding.handler =
        book_reactor_counter_impl_as_book_reactor_state_handler(&incoming);

    BOOK_REQUIRE(cmeta_interface_desc_valid(
                     book_reactor_state_handler_interface()), 1);
    BOOK_REQUIRE(book_reactor_state_handler_interface()->methods[0].abi != NULL,
                 2);
    BOOK_REQUIRE(book_reactor_state_handler_valid(&outgoing_binding.handler) &&
                 book_reactor_state_handler_valid(&incoming_binding.handler), 3);

    outgoing_observer.on_state = book_reactor_on_state;
    outgoing_observer.user = &outgoing_binding;
    incoming_observer.on_state = book_reactor_on_state;
    incoming_observer.user = &incoming_binding;

    BOOK_REQUIRE(cnet_client_init(&connector, &config) == SALTS_OK, 4);
    BOOK_REQUIRE(cnet_client_init(&acceptor, &config) == SALTS_OK, 5);
    BOOK_REQUIRE(cnet_listener_init(&listener, &listen_config) == SALTS_OK, 6);
    BOOK_REQUIRE(cnet_listener_port(&listener, &port) == SALTS_OK && port != 0u,
                 7);
    BOOK_REQUIRE(snprintf(uri, sizeof(uri), "tcp://127.0.0.1:%u",
                          (unsigned)port) > 0, 8);

    /* Admission rejects invalid observer before any handle or callback. */
    invalid.uri = uri;
    BOOK_REQUIRE(cnet_connect(&connector, &invalid, &rejected) == SALTS_EINVAL,
                 9);
    BOOK_REQUIRE(rejected.slot == 0u && outgoing.callbacks == 0u, 10);

    connect.uri = uri;
    connect.observer = outgoing_observer;
    BOOK_REQUIRE(cnet_connect(&connector, &connect, &outbound) == SALTS_OK &&
                 outbound.slot != 0u, 11);

    deadline = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    while (outgoing.connected == 0u && cmeta_monotonic_ms() < deadline)
        BOOK_REQUIRE(book_progress(&connector) == SALTS_OK, 12);
    BOOK_REQUIRE(outgoing.connected == 1u, 13);

    /* Acceptor readiness is explicit and distinct from client I/O progress. */
    BOOK_REQUIRE(cnet_listener_wait(&listener, BOOK_WAIT_MS, &ready) ==
                 SALTS_OK && ready == 1, 14);

    deadline = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    while (!accepted && cmeta_monotonic_ms() < deadline) {
        int rc = cnet_listener_accept(
            &listener, &acceptor, &incoming_observer, &inbound);
        if (rc == SALTS_OK)
            accepted = true;
        else
            BOOK_REQUIRE(rc == SALTS_ETIMEDOUT, 15);
        BOOK_REQUIRE(book_progress(&connector) == SALTS_OK, 16);
        BOOK_REQUIRE(book_progress(&acceptor) == SALTS_OK, 17);
    }
    BOOK_REQUIRE(accepted && inbound.slot != 0u, 18);

    deadline = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    while (incoming.connected == 0u && cmeta_monotonic_ms() < deadline) {
        BOOK_REQUIRE(book_progress(&connector) == SALTS_OK, 19);
        BOOK_REQUIRE(book_progress(&acceptor) == SALTS_OK, 20);
    }
    BOOK_REQUIRE(incoming.connected == 1u &&
                 outgoing.failed == 0u && incoming.failed == 0u, 21);
    BOOK_REQUIRE(outgoing_binding.errors == 0u &&
                 incoming_binding.errors == 0u, 22);

    /* Closing only the listener must not revoke two admitted connections. */
    BOOK_REQUIRE(cnet_listener_close(&listener) == SALTS_OK, 23);
    BOOK_REQUIRE(cnet_listener_destroy(&listener) == SALTS_OK, 24);
    BOOK_REQUIRE(outgoing.terminal == 0u && incoming.terminal == 0u, 25);

    BOOK_REQUIRE(cnet_close(&connector, outbound) == SALTS_OK, 26);
    BOOK_REQUIRE(cnet_close(&acceptor, inbound) == SALTS_OK, 27);
    deadline = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    while ((outgoing.terminal == 0u || incoming.terminal == 0u) &&
           cmeta_monotonic_ms() < deadline) {
        BOOK_REQUIRE(book_progress(&connector) == SALTS_OK, 28);
        BOOK_REQUIRE(book_progress(&acceptor) == SALTS_OK, 29);
    }
    BOOK_REQUIRE(outgoing.terminal == 1u && incoming.terminal == 1u, 30);
    BOOK_REQUIRE(outgoing.callback_wrong_owner == 0u &&
                 incoming.callback_wrong_owner == 0u, 31);
    BOOK_REQUIRE(outgoing.callback_after_expiry == 0u &&
                 incoming.callback_after_expiry == 0u, 32);

finish:
    /* Handler/self/context outlive every CNet owner and its stop callbacks. */
    if (connector.impl != NULL) {
        if (cnet_client_stop(&connector, BOOK_WAIT_MS) != SALTS_OK && !result)
            result = 33;
        if (cnet_client_destroy(&connector) != SALTS_OK && !result)
            result = 34;
    }
    if (acceptor.impl != NULL) {
        if (cnet_client_stop(&acceptor, BOOK_WAIT_MS) != SALTS_OK && !result)
            result = 35;
        if (cnet_client_destroy(&acceptor) != SALTS_OK && !result)
            result = 36;
    }
    if (listener.impl != NULL) {
        if (cnet_listener_close(&listener) != SALTS_OK && !result)
            result = 37;
        if (cnet_listener_destroy(&listener) != SALTS_OK && !result)
            result = 38;
    }
    outgoing.live = incoming.live = false;
    if (!result &&
        (outgoing.callback_after_expiry || incoming.callback_after_expiry ||
         outgoing.callback_wrong_owner || incoming.callback_wrong_owner))
        result = 39;
    if (result)
        fprintf(stderr, "ACE Reactor-style application gate failed: %d\n",
                result);
    return result;
}
