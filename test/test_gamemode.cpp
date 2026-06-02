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
    mode.remote_hp_before_ = 100;
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
    mode.remote_hp_before_ = 10;
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
    mode.remote_hp_before_ = 25;
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
    mode.remote_hp_before_ = 10;
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
    std::cout << "test_gamemode passed!" << std::endl;
    return 0;
}
