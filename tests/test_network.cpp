#include "network/LanConnection.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

using namespace Synera::network;
using namespace std::chrono_literals;

// Polls a connection until an event of the requested type arrives or the
// timeout elapses. Events of other types seen along the way are discarded,
// which is fine for these tests because their event streams are ordered.
static std::optional<Event>
wait_event(LanConnection &conn, EventType type,
           std::chrono::milliseconds timeout = 4000ms) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        while (auto ev = poll_event(conn)) {
            if (ev->type == type) {
                return ev;
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    return std::nullopt;
}

// Baseline: two peers connect and exchange a message in both directions.
static void test_basic_exchange() {
    LanConnection host;
    LanConnection client;
    assert(start_host(host, 39301));
    assert(join_host(client, "127.0.0.1", 39301));

    assert(wait_event(host, EventType::Connected));
    assert(wait_event(client, EventType::Connected));
    assert(is_connected(host));
    assert(is_connected(client));

    assert(send_text(host, "hello"));
    auto to_client = wait_event(client, EventType::Message);
    assert(to_client && to_client->text == "hello");

    assert(send_text(client, "world"));
    auto to_host = wait_event(host, EventType::Message);
    assert(to_host && to_host->text == "world");

    Synera::network::shutdown(host);
    Synera::network::shutdown(client);
}

// Host-side reconnection: after a connected client drops, the host re-arms its
// acceptor and a fresh client can take over the slot and resume messaging.
static void test_host_reaccepts_after_drop() {
    LanConnection host;
    assert(start_host(host, 39302));

    {
        LanConnection client_a;
        assert(join_host(client_a, "127.0.0.1", 39302));
        assert(wait_event(host, EventType::Connected));
        assert(wait_event(client_a, EventType::Connected));
        // Dropping the peer's end looks like an unexpected disconnect to the
        // host (the host itself never called shutdown).
        Synera::network::shutdown(client_a);
    }

    assert(wait_event(host, EventType::Disconnected));
    assert(!is_connected(host));

    // The host is still alive and able to reconnect: it re-listens.
    assert(reconnect(host));

    LanConnection client_b;
    assert(join_host(client_b, "127.0.0.1", 39302));
    assert(wait_event(host, EventType::Connected));
    assert(wait_event(client_b, EventType::Connected));
    assert(is_connected(host));

    assert(send_text(host, "resync"));
    auto resync = wait_event(client_b, EventType::Message);
    assert(resync && resync->text == "resync");

    Synera::network::shutdown(host);
    Synera::network::shutdown(client_b);
}

// Client-side reconnection: after the host disappears, the client retries (with
// backoff) and latches onto a freshly started host on the same port.
static void test_client_reconnects_to_new_host() {
    LanConnection client;

    {
        LanConnection host1;
        assert(start_host(host1, 39303));
        assert(join_host(client, "127.0.0.1", 39303));
        assert(wait_event(host1, EventType::Connected));
        assert(wait_event(client, EventType::Connected));
        Synera::network::shutdown(host1); // host disappears
    }

    assert(wait_event(client, EventType::Disconnected));
    assert(!is_connected(client));

    LanConnection host2;
    assert(start_host(host2, 39303));

    // The client retries on a timer until host2 accepts it.
    assert(reconnect(client));
    assert(wait_event(client, EventType::Connected, 6000ms));
    assert(wait_event(host2, EventType::Connected, 6000ms));
    assert(is_connected(client));

    assert(send_text(host2, "back"));
    auto back = wait_event(client, EventType::Message);
    assert(back && back->text == "back");

    Synera::network::shutdown(client);
    Synera::network::shutdown(host2);
}

// Reconnecting a connection that was never established and has no role is a
// no-op that reports failure rather than crashing.
static void test_reconnect_without_role_fails() {
    LanConnection idle;
    assert(!reconnect(idle));
}

int main() {
    std::cout << "Running test_network..." << std::endl;
    test_basic_exchange();
    test_host_reaccepts_after_drop();
    test_client_reconnects_to_new_host();
    test_reconnect_without_role_fails();
    std::cout << "test_network passed!" << std::endl;
    return 0;
}
