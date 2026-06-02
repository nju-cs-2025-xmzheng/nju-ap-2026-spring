#include "engine/LanMultiplayerMode.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace Synera;
using namespace Synera::engine;
using namespace std::chrono_literals;

// Pumps both modes' event loops until the condition holds or the timeout
// elapses, so the asynchronous socket handshake/streaming can complete.
template <typename Cond>
static bool pump_until(LanMultiplayerMode &host, LanMultiplayerMode &client,
                       Cond cond,
                       std::chrono::milliseconds timeout = 4000ms) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        poll_mode(host);
        poll_mode(client);
        if (cond()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return cond();
}

// End-to-end: a client preparation action travels over a real socket to the
// host, mutates the host-owned remote_session_, and the resulting authoritative
// state is streamed back and rendered by the client.
static void test_client_command_roundtrip_over_sockets() {
    LanMultiplayerMode host;
    LanMultiplayerMode client;

    host_multiplayer(host, ConnectionConfig{"127.0.0.1", 39501});
    join_multiplayer(client, ConnectionConfig{"127.0.0.1", 39501});

    assert(pump_until(host, client, [&] {
        return host.connected_ && client.connected_;
    }));

    // The host owns the client's authoritative session and its shop.
    assert(host.remote_session_.shop_[0].has_value());
    int cost = host.remote_session_.shop_[0]->second;
    int gold_before = host.remote_session_.player_.gold;

    // Client buys slot 0. It must NOT change locally (server-authoritative);
    // only the host's command application + STATE stream updates it.
    act_buy(client, 0);

    assert(pump_until(host, client, [&] {
        return host.remote_session_.player_.gold == gold_before - cost;
    }));
    // The authoritative gold and the purchased bench unit reach the client.
    assert(pump_until(host, client, [&] {
        return client.session_.player_.gold == gold_before - cost &&
               get_unit(client.session_.board_, LinearCoord{0}) != nullptr;
    }));

    leave_game(host);
    leave_game(client);
}

int main() {
    std::cout << "Running test_mp_integration..." << std::endl;
    test_client_command_roundtrip_over_sockets();
    std::cout << "test_mp_integration passed!" << std::endl;
    return 0;
}
