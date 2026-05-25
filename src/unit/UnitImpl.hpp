#pragma once

#include "unit/Synergy.hpp"
#include <algorithm>

namespace Synera::unit {

template <typename SlimeType>
inline std::optional<engine::HexCoord>
find_unit_coord(const engine::Board &board, const SlimeType &self) noexcept {
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            if (auto u_ptr = engine::get_unit(board, coord)) {
                if (auto slime_ptr = std::get_if<SlimeType>(u_ptr.get())) {
                    if (slime_ptr == &self) {
                        return coord;
                    }
                }
            }
        }
    }
    return std::nullopt;
}

// Pyro Slime Skill: Pyro Explosion
// Deals 2.0 * ATK Pyro damage to the target and all its adjacent neighbors
inline void tag_invoke(__tag::cast_skill_t, PyroSlime &u, engine::Board &board,
                       engine::HexCoord target) {
    if (auto target_unit = engine::get_unit(board, target)) {
        int dmg = int(u.stats_.atk * 2.0);
        deal_damage(board, u, *target_unit, dmg);

        for (auto n : engine::neighbor(target)) {
            if (auto neighbor_unit = engine::get_unit(board, n)) {
                deal_damage(board, u, *neighbor_unit, dmg);
            }
        }
    }
}

// Hydro Slime Skill: Bubble Trap
// Deals 1.2 * ATK Hydro damage and stuns the target for 1 tick
inline void tag_invoke(__tag::cast_skill_t, HydroSlime &u, engine::Board &board,
                       engine::HexCoord target) {
    if (auto target_unit = engine::get_unit(board, target)) {
        deal_damage(board, u, *target_unit, int(u.stats_.atk * 1.2));
        stats(*target_unit).state = State::Idle;
    }
}

// Anemo Slime Skill: Anemo Burst
// Deals 1.5 * ATK Wind damage to all units in a straight line towards the
// target
inline void tag_invoke(__tag::cast_skill_t, AnemoSlime &u, engine::Board &board,
                       engine::HexCoord target) {
    auto my_coord_opt = find_unit_coord(board, u);
    if (!my_coord_opt)
        return;
    auto from_coord = *my_coord_opt;

    int dr = target.r - from_coord.r;
    int dc = target.c - from_coord.c;
    int step_r = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
    int step_c = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);

    if (step_r == 0 && step_c == 0)
        return;

    int dmg = int(u.stats_.atk * 1.5);
    engine::HexCoord cur = from_coord;
    while (true) {
        cur.r += step_r;
        cur.c += step_c;
        if (!engine::in_range(cur)) {
            break;
        }
        if (auto target_u = engine::get_unit(board, cur)) {
            deal_damage(board, u, *target_u, dmg);
        }
    }
}

// Geo Slime Skill: Geo Shielding
// Regenerates 200 * level shield HP and deals 1.0 * ATK damage to all adjacent
// enemies
inline void tag_invoke(__tag::cast_skill_t, GeoSlime &u, engine::Board &board,
                       engine::HexCoord target) {
    auto my_coord_opt = find_unit_coord(board, u);
    if (!my_coord_opt)
        return;
    auto my_coord = *my_coord_opt;

    // Regenerate shield
    u.stats_.shield += 200 * u.stats_.level;

    // Deal damage to all adjacent enemies
    for (auto n : engine::neighbor(my_coord)) {
        if (auto target_u = engine::get_unit(board, n)) {
            if (stats(*target_u).owner != u.stats_.owner) {
                deal_damage(board, u, *target_u, u.stats_.atk);
            }
        }
    }
}

// Electro Slime Skill: Chain Electro
// Deals 1.2 * ATK Electro damage to the target and chains to up to 2 other
// nearby enemies (distance <= 2)
inline void tag_invoke(__tag::cast_skill_t, ElectroSlime &u,
                       engine::Board &board, engine::HexCoord target) {
    if (auto target_unit = engine::get_unit(board, target)) {
        int dmg = int(u.stats_.atk * 1.2);
        deal_damage(board, u, *target_unit, dmg);

        // Find nearby enemies to chain to
        std::vector<std::shared_ptr<Unit>> chain_targets;
        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                engine::HexCoord cell{r, c};
                if (cell == target)
                    continue;
                if (auto target_u = engine::get_unit(board, cell)) {
                    if (stats(*target_u).owner != u.stats_.owner &&
                        engine::distance(target, cell) <= 2) {
                        chain_targets.push_back(target_u);
                    }
                }
            }
        }

        // Limit chain to 2 targets
        for (size_t i = 0; i < std::min(chain_targets.size(), size_t(2)); ++i) {
            deal_damage(board, u, *chain_targets[i], dmg);
        }
    }
}

// Cryo Slime Skill: Cryo Frost
// Deals 1.2 * ATK Cryo damage and freezes (stuns) target for 1 tick
inline void tag_invoke(__tag::cast_skill_t, CryoSlime &u, engine::Board &board,
                       engine::HexCoord target) {
    if (auto target_unit = engine::get_unit(board, target)) {
        deal_damage(board, u, *target_unit, int(u.stats_.atk * 1.2));
        stats(*target_unit).state = State::Idle;
    }
}

template <typename T, typename B, typename C>
inline constexpr void __fn::normal_attack_fn::operator()(T &&a, B &&board,
                                                         C &&target) const {
    if constexpr (requires {
                      tag_invoke(__tag::normal_attack_t{}, std::forward<T>(a),
                                 std::forward<B>(board),
                                 std::forward<C>(target));
                  }) {
        tag_invoke(__tag::normal_attack_t{}, std::forward<T>(a),
                   std::forward<B>(board), std::forward<C>(target));
    } else {
        if (auto target_unit = engine::get_unit(board, target)) {
            deal_damage(board, a, *target_unit, stats(a).atk);
            auto &s = stats(a);

            // High Voltage (Electro Resonance): normal attack mana gain +5
            // (total 15) for Electro units
            int mana_gain = 10;
            auto synergies = compute_synergies(board);
            if (synergies.high_voltage && element(a) == Element::Electro) {
                mana_gain = 15;
            }
            s.mana += mana_gain;

            // Shattering Ice (Cryo Resonance): normal attacks of Cryo units
            // have 20% chance to freeze (stun for 1 tick)
            if (synergies.shattering_ice && element(a) == Element::Cryo) {
                if (std::rand() % 100 < 20) {
                    stats(*target_unit).state = State::Idle;
                }
            }

            if (s.mana >= s.max_mana) {
                s.mana = 0;
                cast_skill(std::forward<T>(a), std::forward<B>(board),
                           std::forward<C>(target));
            }
        }
    }
}

} // namespace Synera::unit
