#pragma once
#include "engine/Board.hpp"
#include "unit/Unit.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace Synera::engine {

enum class CombatResult { Ongoing, PlayerWin, EnemyWin, Draw };

class BattleEngine {
  public:
    BattleEngine() = default;

    // Run 1 tick of the FSM loop
    void tick(Board &board) {
        struct ActiveUnit {
            std::shared_ptr<unit::Unit> unit;
            HexCoord coord;
            unit::State state;
            int attack_cooldown = 0;
            int move_cooldown = 0;
            int stun_ticks = 0;
        };
        std::vector<ActiveUnit> units;

        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                HexCoord cell{r, c};
                if (auto u_ptr = get_unit(board, cell)) {
                    auto &stats = unit::stats(*u_ptr);
                    if (stats.hp > 0) {
                        units.push_back({u_ptr, cell, stats.state,
                                         stats.attack_cooldown,
                                         stats.move_cooldown,
                                         stats.stun_ticks});
                    }
                }
            }
        }

        for (auto &au : units) {
            auto &u = *au.unit;
            auto &stats = unit::stats(u);
            bool defeated_during_tick = stats.hp <= 0;
            if (defeated_during_tick) {
                stats.state = au.state;
                stats.attack_cooldown = au.attack_cooldown;
                stats.move_cooldown = au.move_cooldown;
                stats.stun_ticks = au.stun_ticks;
            }

            // Stun logic: decrement stun ticks and skip action
            if (stats.stun_ticks > 0) {
                stats.stun_ticks--;
                continue;
            }

            // Find current coordinates of this unit on the board
            HexCoord my_coord = au.coord;
            auto actual_ptr = get_unit(board, my_coord);
            if (actual_ptr.get() != au.unit.get()) {
                bool found = false;
                for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
                    for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                        HexCoord cell{r, c};
                        if (get_unit(board, cell).get() == au.unit.get()) {
                            my_coord = cell;
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                }
                if (!found)
                    continue; // Unit died or was removed
            }

            // Unit FSM
            switch (stats.state) {
            case unit::State::Idle: {
                if (stats.mana >= stats.max_mana) {
                    if (auto target_opt = select_target(board, u, my_coord)) {
                        stats.mana = 0;
                        unit::cast_skill(u, board, *target_opt);
                    }
                    break;
                }

                if (auto target_opt = select_target(board, u, my_coord)) {
                    int dist = distance(my_coord, *target_opt);
                    if (dist <= stats.range) {
                        stats.state = unit::State::Attacking;
                        stats.attack_cooldown = 0;
                    } else {
                        stats.state = unit::State::Moving;
                        stats.move_cooldown = 0;
                    }
                }
                break;
            }
            case unit::State::Moving: {
                auto target_opt = select_target(board, u, my_coord);
                if (!target_opt) {
                    stats.state = unit::State::Idle;
                    break;
                }

                int dist = distance(my_coord, *target_opt);
                if (dist <= stats.range) {
                    stats.state = unit::State::Attacking;
                    stats.attack_cooldown = 0;
                    break;
                }

                if (stats.move_cooldown <= 0) {
                    auto path = find_path(board, my_coord, *target_opt);
                    if (path.size() > 1) {
                        HexCoord next_step = path[1];
                        if (!is_occupied(board, next_step)) {
                            set_unit(board, next_step, au.unit);
                            remove_unit(board, my_coord);
                            my_coord = next_step;
                            stats.move_cooldown = stats.move_interval;

                            // Recheck range after moving
                            dist = distance(my_coord, *target_opt);
                            if (dist <= stats.range) {
                                stats.state = unit::State::Attacking;
                                stats.attack_cooldown = 0;
                            }
                        } else {
                            stats.move_cooldown = 5; // Path blocked, retry soon
                        }
                    } else {
                        stats.move_cooldown = 5; // No path, retry soon
                    }
                } else {
                    stats.move_cooldown--;
                }
                break;
            }
            case unit::State::Attacking: {
                auto target_opt = select_target(board, u, my_coord);
                if (!target_opt) {
                    stats.state = unit::State::Idle;
                    break;
                }

                int dist = distance(my_coord, *target_opt);
                if (dist > stats.range) {
                    stats.state = unit::State::Moving;
                    stats.move_cooldown = 0;
                    break;
                }

                if (stats.attack_cooldown <= 0) {
                    unit::normal_attack(u, board, *target_opt);
                    stats.attack_cooldown = stats.attack_interval;
                } else {
                    stats.attack_cooldown--;
                }
                break;
            }
            case unit::State::Dead:
            default:
                break;
            }

            if (stats.hp <= 0) {
                stats.hp = 0;
                stats.state = unit::State::Dead;
            }
        }
    }

    CombatResult combat_result(const Board &board) const {
        int player_units = 0;
        int enemy_units = 0;

        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                HexCoord cell{r, c};
                if (auto u_ptr = get_unit(board, cell)) {
                    auto &stats = unit::stats(*u_ptr);
                    if (stats.hp > 0) {
                        if (stats.owner == unit::Owner::PlayerCtrl) {
                            player_units++;
                        } else {
                            enemy_units++;
                        }
                    }
                }
            }
        }

        if (player_units == 0 && enemy_units == 0) {
            return CombatResult::Draw;
        }
        if (player_units == 0) {
            return CombatResult::EnemyWin;
        }
        if (enemy_units == 0) {
            return CombatResult::PlayerWin;
        }
        return CombatResult::Ongoing;
    }

    bool is_combat_over(const Board &board, bool &player_won) const {
        CombatResult result = combat_result(board);
        player_won = result == CombatResult::PlayerWin;
        return result != CombatResult::Ongoing;
    }
};

} // namespace Synera::engine
