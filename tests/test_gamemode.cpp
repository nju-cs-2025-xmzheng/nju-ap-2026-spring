#include "engine/LanMultiplayerMode.hpp"
#include "engine/SinglePlayerMode.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <iostream>
#include <memory>

using namespace Synera;
using namespace Synera::engine;
using namespace Synera::unit;

static void test_round_20_victory_settlement() {
    SinglePlayerMode mode;
    init_board(mode.session_.board_);
    mode.session_.round_ = 20;
    mode.session_.player_.hp = 100;
    mode.session_.player_.gold = 0;
    mode.session_.player_.level = 1;

    auto player = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(mode.session_.board_, HexCoord{4, 0}, player);

    auto start = start_combat(mode);
    assert(mode.is_combat_);
    assert(start.clear_visuals);

    // The combat board has no enemies, so the public tick CPO resolves a win.
    // Round 20 must enter settlement instead of advancing to round 21.
    auto update = process_combat_tick(mode);
    assert(update.enter_settlement);
    assert(update.player_won_game);
    assert(mode.session_.round_ == 20);
}

static void test_acknowledge_advances_score_once() {
    SinglePlayerMode mode;
    init_board(mode.session_.board_);
    mode.session_.player_.level = 2;
    mode.session_.player_.gold = 20;
    mode.session_.round_ = 3;

    auto player = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(mode.session_.board_, HexCoord{4, 0}, player);

    start_combat(mode);
    auto result = process_combat_tick(mode);
    assert(!result.enter_settlement);
    assert(mode.result_announced_);
    int gold_after_reward = mode.session_.player_.gold;

    auto update = acknowledge_result(mode);
    assert(!update.enter_settlement);
    assert(mode.session_.round_ == 4);

    int base_gold = 2 + mode.session_.player_.level;
    int interest = std::min(5, gold_after_reward / 10);
    assert(mode.session_.player_.gold ==
           gold_after_reward + base_gold + interest);
    assert(update.status == "Round 4 Started! Gained 4 Gold + 2 interest.");
}

static void test_common_score_settlement() {
    GameSession session;
    Board combat_board;
    init_board(combat_board);
    session.player_.gold = 10;
    session.player_.level = 3;
    auto victory = settle_combat_score(session, combat_board,
                                       CombatResult::PlayerWin, false);
    assert(victory.status == "VICTORY! Gained 8 Gold.");
    assert(!victory.player_defeated);
    assert(session.player_.gold == 18);

    session.player_.hp = 100;
    auto draw =
        settle_combat_score(session, combat_board, CombatResult::Draw, false);
    assert(draw.status == "DRAW! Took 10 damage.");
    assert(!draw.player_defeated);
    assert(session.player_.hp == 90);

    auto survivor = std::make_shared<Unit>(PyroSlime(Owner::EnemyCtrl, 1));
    set_unit(combat_board, HexCoord{1, 0}, survivor);
    session.player_.hp = 100;
    auto inconsistent_draw =
        settle_combat_score(session, combat_board, CombatResult::Draw, false);
    assert(inconsistent_draw.status == "DRAW! Took 10 damage.");
    assert(session.player_.hp == 90);

    GameSession left;
    GameSession right;
    left.player_.gold = 0;
    left.player_.level = 1;
    right.player_.gold = 0;
    right.player_.level = 5;
    settle_combat_score(left, combat_board, CombatResult::Draw, false,
                        Owner::EnemyCtrl, 2);
    settle_combat_score(right, combat_board, CombatResult::Draw, false,
                        Owner::EnemyCtrl, 2);
    assert(left.player_.gold == right.player_.gold);
    assert(left.player_.hp == right.player_.hp);

    GameSession reported;
    reported.player_.hp = 25;
    reported.player_.gold = 0;
    auto reported_draw = settle_combat_score_with_damage(
        reported, CombatResult::Draw, false, 10, 2);
    assert(reported_draw.status == "DRAW! Gained 2 Gold. Took 10 damage.");
    assert(reported.player_.hp == 15);
    assert(reported.player_.gold == 2);

    reported.player_.hp = 10;
    auto reported_ko = settle_combat_score_with_damage(
        reported, CombatResult::Draw, false, 10, 2);
    assert(reported_ko.player_defeated);
    assert(reported.player_.hp == 0);
}

static void test_multiplayer_acknowledge_keeps_income_status() {
    LanMultiplayerMode mode;
    mode.connected_ = true;
    mode.prep_board_copy_ = clone_board(mode.session_.board_);
    mode.session_.player_.gold = 30;
    mode.session_.player_.level = 1;
    mode.session_.round_ = 1;
    mode.result_announced_ = true;

    auto update = acknowledge_result(mode);
    assert(update.status == "Round 2 Started! Gained 3 Gold + 3 interest.");
}

static void test_multiplayer_has_no_round_20_limit() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.session_.round_ = 20;
    mode.session_.player_.hp = 100;
    mode.remote_session_.player_.hp = 100;
    mode.prep_board_copy_ = clone_board(mode.session_.board_);
    init_board(mode.combat_board_);
    auto player = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(mode.combat_board_, HexCoord{4, 0}, player);
    mode.is_combat_ = true;

    auto result = process_combat_tick(mode);
    assert(!result.enter_settlement);
    auto update = acknowledge_result(mode);
    assert(!update.enter_settlement);
    assert(mode.session_.round_ == 21);
}

static void test_multiplayer_winner_enters_settlement() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.session_.player_.hp = 100;
    mode.remote_session_.player_.hp = 10;
    init_board(mode.combat_board_);
    auto player = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(mode.combat_board_, HexCoord{4, 0}, player);
    mode.is_combat_ = true;

    auto result = process_combat_tick(mode);
    assert(result.enter_settlement);
    assert(result.player_won_game);
    assert(mode.player_won_game_);
}

static void test_multiplayer_draw_scores_and_damages_evenly() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.session_.player_.hp = 25;
    mode.session_.player_.gold = 0;
    mode.remote_session_.player_.hp = 25;
    init_board(mode.combat_board_);
    mode.is_combat_ = true;

    auto result = process_combat_tick(mode);
    assert(!result.enter_settlement);
    assert(mode.combat_result_ == CombatResult::Draw);
    assert(mode.session_.player_.hp == 15);
    assert(mode.remote_hp_after_ == 15);
    assert(mode.session_.player_.gold == 2);
    assert(result.status == "DRAW! Gained 2 Gold. Took 10 damage.");
}

static void test_multiplayer_draw_double_ko_enters_draw_settlement() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.session_.player_.hp = 10;
    mode.remote_session_.player_.hp = 10;
    init_board(mode.combat_board_);
    mode.is_combat_ = true;

    auto result = process_combat_tick(mode);
    assert(result.enter_settlement);
    assert(!result.player_won_game);
    assert(!mode.player_won_game_);
    assert(mode.session_.player_.hp == 0);
    assert(mode.remote_hp_after_ == 0);
}

static void test_multiplayer_keeps_early_remote_ready_after_ack() {
    LanMultiplayerMode mode;
    mode.connected_ = true;
    mode.kind_ = ModeKind::LanHost;
    mode.session_.player_.hp = 100;
    init_board(mode.remote_ready_board_);
    auto remote = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(mode.remote_ready_board_, HexCoord{4, 1}, remote);
    mode.remote_ready_ = true;
    mode.result_announced_ = true;
    mode.is_combat_ = true;

    auto player = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(mode.session_.board_, HexCoord{4, 0}, player);
    mode.prep_board_copy_ = clone_board(mode.session_.board_);

    auto ack = acknowledge_result(mode);
    assert(!ack.enter_settlement);
    assert(!mode.result_announced_);
    assert(!mode.is_combat_);
    assert(mode.remote_ready_);

    auto start = start_combat(mode);
    assert(start.clear_visuals);
    assert(mode.is_combat_);
    assert(!mode.local_ready_);
    assert(!mode.remote_ready_);
}

static void test_multiplayer_initial_connect_enters_gameplay() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanClient;

    auto update = mode.handle_event({network::EventType::Connected, ""});
    assert(mode.connected_);
    assert(!mode.reconnecting_);
    assert(update.enter_gameplay);
    assert(update.clear_visuals);
}

static void test_multiplayer_disconnect_without_link_closes() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanClient;
    mode.connected_ = true;
    mode.local_ready_ = true;

    auto update = mode.handle_event({network::EventType::Disconnected, "drop"});
    assert(!mode.connected_);
    assert(!mode.reconnecting_);
    assert(!mode.local_ready_);
    assert(update.status == "Multiplayer connection closed.");
}

static void test_multiplayer_disconnect_begins_reconnect_and_resumes() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    // A live host listener is what makes reconnection possible.
    assert(network::start_host(mode.connection_, 39401));
    mode.connected_ = true;
    mode.is_combat_ = true;

    auto lost = mode.handle_event({network::EventType::Disconnected, "drop"});
    assert(!mode.connected_);
    assert(mode.reconnecting_);
    assert(mode.is_combat_); // in-progress combat is preserved
    assert(lost.status == "Connection lost. Reconnecting...");

    auto resumed = mode.handle_event({network::EventType::Connected, ""});
    assert(mode.connected_);
    assert(!mode.reconnecting_);
    assert(mode.is_combat_);
    assert(!resumed.enter_gameplay); // resume, not a brand-new game
    assert(resumed.status == "Reconnected. Resuming game.");

    network::shutdown(mode.connection_);
}

// ---- Host-authoritative model ------------------------------------------

static void test_mp_host_applies_buy_command() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.remote_session_.player_.gold = 10;
    mode.remote_session_.shop_[0] =
        std::pair<Unit, int>{PyroSlime(Owner::PlayerCtrl, 1), 2};
    mode.session_.player_.gold = 15; // host's own economy must stay untouched

    mode.handle_event({network::EventType::Message, "BUY 0"});

    assert(mode.remote_session_.player_.gold == 8);
    assert(get_unit(mode.remote_session_.board_, LinearCoord{0}) != nullptr);
    assert(mode.session_.player_.gold == 15);
}

static void test_mp_host_move_merges_authoritatively() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.remote_session_.player_.level = 5; // room to deploy
    set_unit(mode.remote_session_.board_, HexCoord{4, 0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
    set_unit(mode.remote_session_.board_, LinearCoord{0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
    set_unit(mode.remote_session_.board_, LinearCoord{1},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

    mode.handle_event({network::EventType::Message, "MOVE B 0 H 4 1"});

    assert(mode.remote_session_.count_player_board_units() == 1);
    auto merged = get_unit(mode.remote_session_.board_, HexCoord{4, 0});
    assert(merged && stats(*merged).level == 2);
}

static void test_mp_state_roundtrip_overwrites_client() {
    LanMultiplayerMode host;
    host.kind_ = ModeKind::LanHost;
    host.remote_session_.player_.gold = 42;
    host.remote_session_.player_.level = 4;
    host.remote_session_.round_ = 7;
    set_unit(host.remote_session_.board_, HexCoord{4, 0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

    LanMultiplayerMode client;
    client.kind_ = ModeKind::LanClient;
    client.handle_event(
        {network::EventType::Message,
         "STATE\n" + serialize_session_to_string(host.remote_session_)});

    assert(client.session_.player_.gold == 42);
    assert(client.session_.player_.level == 4);
    assert(client.session_.round_ == 7);
    assert(get_unit(client.session_.board_, HexCoord{4, 0}) != nullptr);

    // A second STATE fully replaces the first (wholesale overwrite).
    GameSession next;
    clear_enemy_half(next.board_);
    next.player_.gold = 3;
    client.handle_event({network::EventType::Message,
                         "STATE\n" + serialize_session_to_string(next)});
    assert(client.session_.player_.gold == 3);
    assert(get_unit(client.session_.board_, HexCoord{4, 0}) == nullptr);
}

static void test_mp_client_move_predicts_then_reconciles() {
    LanMultiplayerMode client;
    client.kind_ = ModeKind::LanClient;
    set_unit(client.session_.board_, LinearCoord{0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

    // Prediction: the local board changes immediately (no host round-trip).
    act_move(client, Coord{LinearCoord{0}}, Coord{HexCoord{4, 0}});
    assert(get_unit(client.session_.board_, HexCoord{4, 0}) != nullptr);
    assert(get_unit(client.session_.board_, LinearCoord{0}) == nullptr);

    // Reconciliation: an authoritative STATE wins over the prediction.
    GameSession authoritative;
    clear_enemy_half(authoritative.board_);
    set_unit(authoritative.board_, HexCoord{5, 5},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
    client.handle_event(
        {network::EventType::Message,
         "STATE\n" + serialize_session_to_string(authoritative)});
    assert(get_unit(client.session_.board_, HexCoord{4, 0}) == nullptr);
    assert(get_unit(client.session_.board_, HexCoord{5, 5}) != nullptr);
}

static void test_mp_host_ready_derived_from_remote_session() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    // Host has not readied yet, so the client's READY just records remote
    // readiness (no combat start), letting us inspect the derived board.
    set_unit(mode.remote_session_.board_, HexCoord{4, 1},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

    mode.handle_event({network::EventType::Message, "READY"});

    assert(mode.remote_ready_);
    assert(!mode.is_combat_);
    // Ready board derived from remote_session_, not received over the wire.
    assert(get_unit(mode.remote_ready_board_, HexCoord{4, 1}) != nullptr);
    assert(get_unit(mode.remote_prep_board_copy_, HexCoord{4, 1}) != nullptr);
}

static void test_mp_host_settles_remote_session_once() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.session_.player_.hp = 25;
    mode.remote_session_.player_.hp = 25;
    init_board(mode.combat_board_); // empty board -> draw
    mode.is_combat_ = true;

    auto result = process_combat_tick(mode);
    assert(mode.combat_result_ == CombatResult::Draw);
    // Damage applied exactly once on each real session.
    assert(mode.session_.player_.hp == 15);
    assert(mode.remote_session_.player_.hp == 15);
    assert(mode.remote_hp_after_ == 15);
    assert(!result.enter_settlement);
}

static void test_mp_host_advances_remote_on_ack() {
    LanMultiplayerMode mode;
    mode.kind_ = ModeKind::LanHost;
    mode.remote_session_.round_ = 3;
    mode.remote_session_.player_.hp = 50;
    mode.remote_session_.player_.gold = 30;
    mode.remote_prep_board_copy_ = clone_board(mode.remote_session_.board_);

    mode.handle_event({network::EventType::Message, "ACK"});

    assert(mode.remote_session_.round_ == 4);
    assert(mode.remote_session_.player_.gold > 30); // round income
    assert(mode.session_.round_ == 1); // host's own round not advanced by ACK
}

int main() {
    std::cout << "Running test_gamemode..." << std::endl;
    test_round_20_victory_settlement();
    test_acknowledge_advances_score_once();
    test_common_score_settlement();
    test_multiplayer_acknowledge_keeps_income_status();
    test_multiplayer_has_no_round_20_limit();
    test_multiplayer_winner_enters_settlement();
    test_multiplayer_draw_scores_and_damages_evenly();
    test_multiplayer_draw_double_ko_enters_draw_settlement();
    test_multiplayer_keeps_early_remote_ready_after_ack();
    test_multiplayer_initial_connect_enters_gameplay();
    test_multiplayer_disconnect_without_link_closes();
    test_multiplayer_disconnect_begins_reconnect_and_resumes();
    test_mp_host_applies_buy_command();
    test_mp_host_move_merges_authoritatively();
    test_mp_state_roundtrip_overwrites_client();
    test_mp_client_move_predicts_then_reconciles();
    test_mp_host_ready_derived_from_remote_session();
    test_mp_host_settles_remote_session_once();
    test_mp_host_advances_remote_on_ack();
    std::cout << "test_gamemode passed!" << std::endl;
    return 0;
}
