#include "engine/BattleEngine.hpp"
#include "engine/Board.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Testing Battle Engine, FSM, and CPO Pathfinding/Targeting..."
              << std::endl;

    Synera::engine::Board board;
    Synera::engine::init_board(board);

    auto player_electro = std::make_shared<Synera::unit::Unit>(
        Synera::unit::ElectroSlime(Synera::unit::Owner::PlayerCtrl, 1));
    auto enemy_pyro = std::make_shared<Synera::unit::Unit>(
        Synera::unit::PyroSlime(Synera::unit::Owner::EnemyCtrl, 1));

    // Place them on the board
    // Player territory is row >= 4
    Synera::engine::set_unit(board, Synera::engine::HexCoord{5, 2},
                             player_electro);
    Synera::engine::set_unit(board, Synera::engine::HexCoord{2, 2}, enemy_pyro);

    Synera::engine::BattleEngine engine;

    {
        Synera::engine::Board result_board;
        Synera::engine::init_board(result_board);
        assert(engine.combat_result(result_board) ==
               Synera::engine::CombatResult::Draw);

        auto alive_player = std::make_shared<Synera::unit::Unit>(
            Synera::unit::PyroSlime(Synera::unit::Owner::PlayerCtrl, 1));
        Synera::engine::set_unit(result_board, Synera::engine::HexCoord{5, 1},
                                 alive_player);
        assert(engine.combat_result(result_board) ==
               Synera::engine::CombatResult::PlayerWin);

        Synera::engine::remove_unit(result_board,
                                    Synera::engine::HexCoord{5, 1});
        auto alive_enemy = std::make_shared<Synera::unit::Unit>(
            Synera::unit::PyroSlime(Synera::unit::Owner::EnemyCtrl, 1));
        Synera::engine::set_unit(result_board, Synera::engine::HexCoord{2, 1},
                                 alive_enemy);
        assert(engine.combat_result(result_board) ==
               Synera::engine::CombatResult::EnemyWin);
    }

    {
        Synera::engine::Board draw_board;
        Synera::engine::init_board(draw_board);
        auto player = std::make_shared<Synera::unit::Unit>(
            Synera::unit::PyroSlime(Synera::unit::Owner::PlayerCtrl, 1));
        auto enemy = std::make_shared<Synera::unit::Unit>(
            Synera::unit::PyroSlime(Synera::unit::Owner::EnemyCtrl, 1));
        auto &player_stats = Synera::unit::stats(*player);
        auto &enemy_stats = Synera::unit::stats(*enemy);
        player_stats.hp = 1;
        player_stats.atk = 5;
        player_stats.state = Synera::unit::State::Attacking;
        player_stats.attack_cooldown = 0;
        enemy_stats.hp = 1;
        enemy_stats.atk = 5;
        enemy_stats.state = Synera::unit::State::Attacking;
        enemy_stats.attack_cooldown = 0;
        Synera::engine::set_unit(draw_board, Synera::engine::HexCoord{4, 0},
                                 player);
        Synera::engine::set_unit(draw_board, Synera::engine::HexCoord{3, 0},
                                 enemy);

        engine.tick(draw_board);
        assert(engine.combat_result(draw_board) ==
               Synera::engine::CombatResult::Draw);
    }

    // 1. Verify Target Selection CPO
    auto target_opt = Synera::engine::select_target(
        board, *player_electro, Synera::engine::HexCoord{5, 2});
    assert(target_opt.has_value());
    assert(target_opt->r == 2 && target_opt->c == 2);
    std::cout << "CPO Target Selection: OK (Locked Enemy Pyro Slime at [2, 2])"
              << std::endl;

    // 2. Verify Pathfinding CPO
    auto path = Synera::engine::find_path(board, Synera::engine::HexCoord{5, 2},
                                          Synera::engine::HexCoord{2, 2});
    assert(path.size() > 1);
    assert((path.front() == Synera::engine::HexCoord{5, 2}));
    assert((path.back() == Synera::engine::HexCoord{2, 2}));
    std::cout << "CPO Hex Grid BFS Pathfinding: OK (Path size = " << path.size()
              << ")" << std::endl;

    // 3. Run Battle Simulation
    bool player_won = false;
    int ticks = 0;

    std::cout << "Starting combat loop..." << std::endl;
    while (!engine.is_combat_over(board, player_won) && ticks < 1000) {
        engine.tick(board);
        ticks++;

        // Print position and HP of units every 10 ticks for logging
        if (ticks % 10 == 0 || engine.is_combat_over(board, player_won)) {
            // Find current coordinates
            Synera::engine::HexCoord elec_pos{-1, -1};
            Synera::engine::HexCoord pyro_pos{-1, -1};
            for (int r = 0; r < Synera::config::engine::BOARD_ROWS; ++r) {
                for (int c = 0; c < Synera::config::engine::BOARD_COLS; ++c) {
                    Synera::engine::HexCoord cell{r, c};
                    if (auto u = Synera::engine::get_unit(board, cell)) {
                        if (Synera::unit::stats(*u).owner ==
                            Synera::unit::Owner::PlayerCtrl)
                            elec_pos = cell;
                        else
                            pyro_pos = cell;
                    }
                }
            }

            auto elec_stats = Synera::unit::stats(*player_electro);
            auto pyro_stats = Synera::unit::stats(*enemy_pyro);

            std::cout << "Tick " << ticks << ":" << std::endl;
            std::cout << "  Electro Slime: pos=[" << elec_pos.r << ","
                      << elec_pos.c << "] hp=" << elec_stats.hp
                      << " state=" << int(elec_stats.state)
                      << " mana=" << elec_stats.mana << std::endl;
            std::cout << "  Pyro Slime:    pos=[" << pyro_pos.r << ","
                      << pyro_pos.c << "] hp=" << pyro_stats.hp
                      << " state=" << int(pyro_stats.state)
                      << " mana=" << pyro_stats.mana << std::endl;
        }
    }

    assert(engine.is_combat_over(board, player_won));
    std::cout << "Combat ended after " << ticks << " ticks. Winner: "
              << (player_won ? "Player (Electro Slime)" : "Enemy (Pyro Slime)")
              << std::endl;

    std::cout << "All Battle Engine, FSM, and CPO Pathfinding/Targeting tests "
                 "passed successfully!"
              << std::endl;
    return 0;
}
