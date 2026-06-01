#pragma once

#include "engine/GameMode.hpp"
#include "unit/Synergy.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace Synera::engine {

inline Board clone_board(const Board &src) {
    Board dst;
    init_board(dst);
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(src, coord)) {
                set_unit(dst, coord, std::make_shared<unit::Unit>(*u_ptr));
            }
        }
    }
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        LinearCoord coord{i};
        if (auto u_ptr = get_unit(src, coord)) {
            set_unit(dst, coord, std::make_shared<unit::Unit>(*u_ptr));
        }
    }
    return dst;
}

inline void reset_unit_for_preparation(unit::Unit &u) {
    auto &s = unit::stats(u);
    s.hp = s.max_hp;
    s.mana = 0;
    s.state = unit::State::Idle;
    s.stun_ticks = 0;
    s.attack_cooldown = 0;
    s.move_cooldown = 0;
}

inline void reset_board_for_preparation(Board &board) {
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            if (auto u_ptr = get_unit(board, HexCoord{r, c})) {
                reset_unit_for_preparation(*u_ptr);
            }
        }
    }
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        if (auto u_ptr = get_unit(board, LinearCoord{i})) {
            reset_unit_for_preparation(*u_ptr);
        }
    }
}

inline void clear_enemy_half(Board &board) {
    for (int r = 0; r < config::engine::BOARD_ROWS / 2; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            remove_unit(board, HexCoord{r, c});
        }
    }
}

inline Board player_ready_board(const Board &src) {
    Board dst;
    init_board(dst);
    for (int r = config::engine::BOARD_ROWS / 2; r < config::engine::BOARD_ROWS;
         ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(src, coord)) {
                if (unit::stats(*u_ptr).owner == unit::Owner::PlayerCtrl) {
                    auto copy = std::make_shared<unit::Unit>(*u_ptr);
                    unit::stats(*copy).owner = unit::Owner::PlayerCtrl;
                    set_unit(dst, coord, copy);
                }
            }
        }
    }
    reset_board_for_preparation(dst);
    return dst;
}

inline HexCoord mirror_coord(HexCoord coord) {
    return HexCoord{config::engine::BOARD_ROWS - 1 - coord.r, coord.c};
}

inline Board build_host_multiplayer_board(const Board &host_board,
                                          const Board &client_board) {
    Board combat;
    init_board(combat);
    for (int r = config::engine::BOARD_ROWS / 2; r < config::engine::BOARD_ROWS;
         ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(host_board, coord)) {
                if (unit::stats(*u_ptr).owner == unit::Owner::PlayerCtrl) {
                    auto copy = std::make_shared<unit::Unit>(*u_ptr);
                    unit::stats(*copy).owner = unit::Owner::PlayerCtrl;
                    set_unit(combat, coord, copy);
                }
            }
            if (auto u_ptr = get_unit(client_board, coord)) {
                if (unit::stats(*u_ptr).owner == unit::Owner::PlayerCtrl) {
                    auto copy = std::make_shared<unit::Unit>(*u_ptr);
                    unit::stats(*copy).owner = unit::Owner::EnemyCtrl;
                    set_unit(combat, mirror_coord(coord), copy);
                }
            }
        }
    }
    reset_board_for_preparation(combat);
    return combat;
}

inline Board board_to_client_view(const Board &host_board) {
    Board client_view;
    init_board(client_view);
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(host_board, coord)) {
                auto copy = std::make_shared<unit::Unit>(*u_ptr);
                auto &s = unit::stats(*copy);
                s.owner = s.owner == unit::Owner::PlayerCtrl
                              ? unit::Owner::EnemyCtrl
                              : unit::Owner::PlayerCtrl;
                set_unit(client_view, mirror_coord(coord), copy);
            }
        }
    }
    return client_view;
}

inline bool same_unit_identity(const unit::Unit &a, const unit::Unit &b) {
    return unit::element(a) == unit::element(b) &&
           unit::stats(a).owner == unit::stats(b).owner &&
           unit::stats(a).level == unit::stats(b).level;
}

inline void apply_board_snapshot_preserving_units(Board &dst,
                                                  const Board &snapshot) {
    struct ExistingUnit {
        std::shared_ptr<unit::Unit> ptr;
        HexCoord coord;
        bool used = false;
    };

    std::vector<ExistingUnit> existing;
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(dst, coord)) {
                existing.push_back({u_ptr, coord, false});
            }
        }
    }

    Board merged;
    init_board(merged);
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            auto incoming = get_unit(snapshot, coord);
            if (!incoming) {
                continue;
            }

            int best_index = -1;
            int best_dist = std::numeric_limits<int>::max();
            for (int i = 0; i < (int)existing.size(); ++i) {
                if (existing[i].used) {
                    continue;
                }
                if (!same_unit_identity(*existing[i].ptr, *incoming)) {
                    continue;
                }
                int d = distance(existing[i].coord, coord);
                if (d < best_dist) {
                    best_dist = d;
                    best_index = i;
                }
            }

            if (best_index >= 0) {
                *existing[best_index].ptr = *incoming;
                existing[best_index].used = true;
                set_unit(merged, coord, existing[best_index].ptr);
            } else {
                set_unit(merged, coord,
                         std::make_shared<unit::Unit>(*incoming));
            }
        }
    }

    dst = std::move(merged);
}

inline std::string serialize_board_to_string(const Board &board) {
    std::ostringstream out;
    serialization::serialize(out, board);
    return out.str();
}

inline std::optional<Board> deserialize_board_from_string(
    const std::string &text) {
    Board board;
    init_board(board);
    std::istringstream in(text);
    serialization::deserialize(in, board);
    return board;
}

inline int count_deployed_player_units(const Board &board) {
    int count = 0;
    for (int r = config::engine::BOARD_ROWS / 2; r < config::engine::BOARD_ROWS;
         ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            if (auto u_ptr = get_unit(board, HexCoord{r, c})) {
                if (unit::stats(*u_ptr).owner == unit::Owner::PlayerCtrl) {
                    count++;
                }
            }
        }
    }
    return count;
}

inline void apply_owner_synergies(Board &board, unit::Owner owner) {
    auto synergies = unit::compute_synergies(board, owner);
    unit::apply_combat_synergies(board, synergies, owner);
}

inline int count_living_units(const Board &board, unit::Owner owner) {
    int count = 0;
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            if (auto u_ptr = get_unit(board, HexCoord{r, c})) {
                auto &s = unit::stats(*u_ptr);
                if (s.owner == owner && s.hp > 0) {
                    count++;
                }
            }
        }
    }
    return count;
}

inline std::string result_title(CombatResult result) {
    switch (result) {
    case CombatResult::PlayerWin:
        return "VICTORY!";
    case CombatResult::EnemyWin:
        return "DEFEAT!";
    case CombatResult::Draw:
        return "DRAW!";
    case CombatResult::Ongoing:
    default:
        return "COMBAT";
    }
}

inline bool is_player_win(CombatResult result) {
    return result == CombatResult::PlayerWin;
}

inline CombatResult invert_result(CombatResult result) {
    if (result == CombatResult::PlayerWin) {
        return CombatResult::EnemyWin;
    }
    if (result == CombatResult::EnemyWin) {
        return CombatResult::PlayerWin;
    }
    return result;
}

inline std::string combat_result_to_string(CombatResult result) {
    switch (result) {
    case CombatResult::PlayerWin:
        return "PlayerWin";
    case CombatResult::EnemyWin:
        return "EnemyWin";
    case CombatResult::Draw:
        return "Draw";
    case CombatResult::Ongoing:
    default:
        return "Ongoing";
    }
}

inline CombatResult combat_result_from_string(const std::string &text) {
    if (text == "PlayerWin") {
        return CombatResult::PlayerWin;
    }
    if (text == "EnemyWin") {
        return CombatResult::EnemyWin;
    }
    if (text == "Draw") {
        return CombatResult::Draw;
    }
    return CombatResult::Ongoing;
}

inline ModeUpdate advance_to_next_preparation(GameSession &session,
                                              Board prep_board_copy) {
    session.board_ = std::move(prep_board_copy);
    reset_board_for_preparation(session.board_);
    session.round_++;
    session.spawn_enemies();
    if (!session.shop_frozen_) {
        session.refresh_shop(true);
    } else {
        session.shop_frozen_ = false;
    }

    int base_gold = 2 + session.player_.level;
    int interest = std::min(5, session.player_.gold / 10);
    session.player_.gold += base_gold + interest;

    return ModeUpdate{
        "Preparation Phase - Drag and drop units to position them.", 3.0f,
        true, false, false, false};
}

} // namespace Synera::engine
