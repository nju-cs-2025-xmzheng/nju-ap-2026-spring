#pragma once

#include "engine/Board.hpp"
#include "unit/Unit.hpp"
#include <algorithm>
#include <cstdlib>

namespace Synera::unit {

struct ActiveSynergies {
    // clang-format off
    bool fervent_flames = false;     // Pyro: ATK +25%
    bool soothing_water = false;     // Hydro: HP +25%
    bool impetuous_winds = false;    // Anemo: Max Mana -15
    bool enduring_rock = false;      // Geo: Shield +15%, and +15% damage when shielded
    bool high_voltage = false;       // Electro: Normal attack mana gain +5 (total 15)
    bool shattering_ice = false;     // Cryo: Normal attacks have 20% chance to freeze (stun for 1 tick)
    bool protective_canopy = false;  // 4+ unique elements: -15% damage taken
    // clang-format on

    int pyro_count = 0;
    int hydro_count = 0;
    int anemo_count = 0;
    int geo_count = 0;
    int electro_count = 0;
    int cryo_count = 0;
};

inline ActiveSynergies compute_synergies(const engine::Board &board,
                                         Owner owner) {
    ActiveSynergies active;

    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            if (auto u_ptr = engine::get_unit(board, coord)) {
                if (stats(*u_ptr).owner == owner) {
                    switch (element(*u_ptr)) {
                    case Element::Pyro:
                        active.pyro_count++;
                        break;
                    case Element::Hydro:
                        active.hydro_count++;
                        break;
                    case Element::Anemo:
                        active.anemo_count++;
                        break;
                    case Element::Geo:
                        active.geo_count++;
                        break;
                    case Element::Electro:
                        active.electro_count++;
                        break;
                    case Element::Cryo:
                        active.cryo_count++;
                        break;
                    }
                }
            }
        }
    }

    active.fervent_flames = (active.pyro_count >= 2);
    active.soothing_water = (active.hydro_count >= 2);
    active.impetuous_winds = (active.anemo_count >= 2);
    active.enduring_rock = (active.geo_count >= 2);
    active.high_voltage = (active.electro_count >= 2);
    active.shattering_ice = (active.cryo_count >= 2);

    int unique_elements = 0;
    if (active.pyro_count > 0)
        unique_elements++;
    if (active.hydro_count > 0)
        unique_elements++;
    if (active.anemo_count > 0)
        unique_elements++;
    if (active.geo_count > 0)
        unique_elements++;
    if (active.electro_count > 0)
        unique_elements++;
    if (active.cryo_count > 0)
        unique_elements++;

    active.protective_canopy = (unique_elements >= 4);

    return active;
}

inline ActiveSynergies compute_synergies(const engine::Board &board) {
    return compute_synergies(board, Owner::PlayerCtrl);
}

inline void apply_combat_synergies(engine::Board &board,
                                   const ActiveSynergies &synergies,
                                   Owner owner) {
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            if (auto u_ptr = engine::get_unit(board, coord)) {
                if (stats(*u_ptr).owner == owner) {
                    auto &s = stats(*u_ptr);
                    if (synergies.fervent_flames) {
                        s.atk = int(s.atk * 1.25);
                    }
                    if (synergies.soothing_water) {
                        s.max_hp = int(s.max_hp * 1.25);
                        s.hp = int(s.hp * 1.25);
                    }
                    if (synergies.impetuous_winds) {
                        s.max_mana = std::max(10, s.max_mana - 15);
                    }
                    if (synergies.enduring_rock && s.shield > 0) {
                        s.shield = int(s.shield * 1.15);
                    }
                }
            }
        }
    }
}

inline void apply_combat_synergies(engine::Board &board,
                                   const ActiveSynergies &synergies) {
    apply_combat_synergies(board, synergies, Owner::PlayerCtrl);
}

template <typename Attacker, typename Target>
inline void deal_damage(engine::Board &board, Attacker &attacker,
                        Target &target, int base_damage) {
    auto attacker_synergies = compute_synergies(board, stats(attacker).owner);
    auto target_synergies = compute_synergies(board, stats(target).owner);
    int final_dmg = base_damage;

    // Geo Resonance (Enduring Rock): shielded units deal 15% more damage
    if (attacker_synergies.enduring_rock && stats(attacker).shield > 0) {
        final_dmg = int(final_dmg * 1.15);
    }

    // Protective Canopy: units take 15% less damage
    if (target_synergies.protective_canopy) {
        final_dmg = int(final_dmg * 0.85);
    }

    take_damage(target, final_dmg);
}

} // namespace Synera::unit
