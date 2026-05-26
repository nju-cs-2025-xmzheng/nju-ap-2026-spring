#pragma once

#include "common/__cpo.hpp"
#include "engine/GameSession.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace Synera::engine {

namespace __tag {
struct save_t {};
struct load_t {};
} // namespace __tag

namespace __fn {
struct save_fn {
    template <typename S, typename P>
    constexpr auto operator()(S &&session, P &&path) const
        noexcept(noexcept(tag_invoke(__tag::save_t{}, std::forward<S>(session),
                                     std::forward<P>(path))))
            -> decltype(auto) {
        return tag_invoke(__tag::save_t{}, std::forward<S>(session),
                          std::forward<P>(path));
    }
};

struct load_fn {
    template <typename S, typename P>
    constexpr auto operator()(S &&session, P &&path) const
        noexcept(noexcept(tag_invoke(__tag::load_t{}, std::forward<S>(session),
                                     std::forward<P>(path))))
            -> decltype(auto) {
        return tag_invoke(__tag::load_t{}, std::forward<S>(session),
                          std::forward<P>(path));
    }
};
} // namespace __fn

inline constexpr __fn::save_fn save{};
inline constexpr __fn::load_fn load{};

inline std::string element_to_string(unit::Element elem) {
    switch (elem) {
    case unit::Element::Pyro:
        return "Pyro";
    case unit::Element::Hydro:
        return "Hydro";
    case unit::Element::Anemo:
        return "Anemo";
    case unit::Element::Geo:
        return "Geo";
    case unit::Element::Electro:
        return "Electro";
    case unit::Element::Cryo:
        return "Cryo";
    }
    return "Pyro";
}

inline unit::Element string_to_element(const std::string &str) {
    if (str == "Pyro")
        return unit::Element::Pyro;
    if (str == "Hydro")
        return unit::Element::Hydro;
    if (str == "Anemo")
        return unit::Element::Anemo;
    if (str == "Geo")
        return unit::Element::Geo;
    if (str == "Electro")
        return unit::Element::Electro;
    if (str == "Cryo")
        return unit::Element::Cryo;
    return unit::Element::Pyro;
}

inline std::string owner_to_string(unit::Owner owner) {
    return owner == unit::Owner::PlayerCtrl ? "Player" : "Enemy";
}

inline unit::Owner string_to_owner(const std::string &str) {
    return str == "Player" ? unit::Owner::PlayerCtrl : unit::Owner::EnemyCtrl;
}

inline std::string state_to_string(unit::State state) {
    switch (state) {
    case unit::State::Idle:
        return "Idle";
    case unit::State::Moving:
        return "Moving";
    case unit::State::Attacking:
        return "Attacking";
    case unit::State::Casting:
        return "Casting";
    case unit::State::Dead:
        return "Dead";
    }
    return "Idle";
}

inline unit::State string_to_state(const std::string &str) {
    if (str == "Idle")
        return unit::State::Idle;
    if (str == "Moving")
        return unit::State::Moving;
    if (str == "Attacking")
        return unit::State::Attacking;
    if (str == "Casting")
        return unit::State::Casting;
    if (str == "Dead")
        return unit::State::Dead;
    return unit::State::Idle;
}

inline std::string equipment_to_string(const unit::Equipment &eq) {
    if (std::holds_alternative<unit::PyroDrop>(eq))
        return "PyroDrop";
    if (std::holds_alternative<unit::HydroDrop>(eq))
        return "HydroDrop";
    if (std::holds_alternative<unit::AnemoDrop>(eq))
        return "AnemoDrop";
    if (std::holds_alternative<unit::GeoDrop>(eq))
        return "GeoDrop";
    if (std::holds_alternative<unit::ElectroDrop>(eq))
        return "ElectroDrop";
    if (std::holds_alternative<unit::CryoDrop>(eq))
        return "CryoDrop";
    return "None";
}

inline unit::Equipment string_to_equipment(const std::string &str) {
    if (str == "PyroDrop")
        return unit::PyroDrop{};
    if (str == "HydroDrop")
        return unit::HydroDrop{};
    if (str == "AnemoDrop")
        return unit::AnemoDrop{};
    if (str == "GeoDrop")
        return unit::GeoDrop{};
    if (str == "ElectroDrop")
        return unit::ElectroDrop{};
    if (str == "CryoDrop")
        return unit::CryoDrop{};
    return std::monostate{};
}

inline unit::Unit make_slime_by_element(unit::Element elem, unit::Owner owner,
                                        int level) {
    switch (elem) {
    case unit::Element::Pyro:
        return unit::PyroSlime(owner, level);
    case unit::Element::Hydro:
        return unit::HydroSlime(owner, level);
    case unit::Element::Anemo:
        return unit::AnemoSlime(owner, level);
    case unit::Element::Geo:
        return unit::GeoSlime(owner, level);
    case unit::Element::Electro:
        return unit::ElectroSlime(owner, level);
    case unit::Element::Cryo:
        return unit::CryoSlime(owner, level);
    }
    return unit::PyroSlime(owner, level);
}

inline bool tag_invoke(__tag::save_t, const GameSession &session,
                       const std::string &filepath) {
    std::ofstream out(filepath);
    if (!out)
        return false;

    // 1. Save Player
    out << "[player]\n";
    out << "hp " << session.player_.hp << "\n";
    out << "gold " << session.player_.gold << "\n";
    out << "level " << session.player_.level << "\n";
    out << "round " << session.round_ << "\n\n";

    // 2. Save Shop
    out << "[shop]\n";
    for (int i = 0; i < 5; ++i) {
        if (session.shop_[i].has_value()) {
            auto &[u, cost] = *session.shop_[i];
            out << "slot " << i << " " << element_to_string(unit::element(u))
                << " " << unit::stats(u).level << " " << cost << "\n";
        } else {
            out << "slot " << i << " empty\n";
        }
    }
    out << "\n";

    // 3. Save Equip Pool
    out << "[equip_pool]\n";
    out << "count " << session.equip_pool_.size() << "\n";
    for (const auto &eq : session.equip_pool_) {
        out << "item " << equipment_to_string(eq) << "\n";
    }
    out << "\n";

    // 4. Save Board Grid (8x8)
    out << "[board]\n";
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            HexCoord coord{r, c};
            if (auto u_ptr = get_unit(session.board_, coord)) {
                auto &s = unit::stats(*u_ptr);
                out << "grid " << r << " " << c << " "
                    << element_to_string(unit::element(*u_ptr)) << " "
                    << owner_to_string(s.owner) << " "
                    << state_to_string(s.state) << " " << s.hp << " "
                    << s.max_hp << " " << s.atk << " " << s.range << " "
                    << s.max_mana << " " << s.mana << " " << s.level << " "
                    << s.shield << " " << equipment_to_string(s.equipped) << " "
                    << s.attack_cooldown << " " << s.move_cooldown << " "
                    << s.stun_ticks << " " << s.attack_interval << " "
                    << s.move_interval << "\n";
            }
        }
    }
    out << "\n";

    // 5. Save Bench (8 slots)
    out << "[bench]\n";
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        LinearCoord coord{i};
        if (auto u_ptr = get_unit(session.board_, coord)) {
            auto &s = unit::stats(*u_ptr);
            out << "bench " << i << " "
                << element_to_string(unit::element(*u_ptr)) << " "
                << owner_to_string(s.owner) << " " << state_to_string(s.state)
                << " " << s.hp << " " << s.max_hp << " " << s.atk << " "
                << s.range << " " << s.max_mana << " " << s.mana << " "
                << s.level << " " << s.shield << " "
                << equipment_to_string(s.equipped) << " " << s.attack_cooldown
                << " " << s.move_cooldown << " " << s.stun_ticks << " "
                << s.attack_interval << " " << s.move_interval << "\n";
        }
    }

    return true;
}

inline bool tag_invoke(__tag::load_t, GameSession &session,
                       const std::string &filepath) {
    std::ifstream in(filepath);
    if (!in)
        return false;

    // Reset session state
    session.player_ = Player{};
    init_board(session.board_);
    session.shop_.fill(std::nullopt);
    session.equip_pool_.clear();

    std::string line;
    std::string section = "";

    while (std::getline(in, line)) {
        if (line.empty())
            continue;

        if (line == "[player]") {
            section = "player";
            continue;
        } else if (line == "[shop]") {
            section = "shop";
            continue;
        } else if (line == "[equip_pool]") {
            section = "equip_pool";
            continue;
        } else if (line == "[board]") {
            section = "board";
            continue;
        } else if (line == "[bench]") {
            section = "bench";
            continue;
        }

        std::istringstream iss(line);
        if (section == "player") {
            std::string key;
            int val;
            if (iss >> key >> val) {
                if (key == "hp")
                    session.player_.hp = val;
                else if (key == "gold")
                    session.player_.gold = val;
                else if (key == "level")
                    session.player_.level = val;
                else if (key == "round")
                    session.round_ = val;
            }
        } else if (section == "shop") {
            std::string key;
            int slot;
            iss >> key >> slot;
            if (key == "slot") {
                std::string status_or_element;
                if (iss >> status_or_element) {
                    if (status_or_element == "empty") {
                        session.shop_[slot] = std::nullopt;
                    } else {
                        int lvl, cost;
                        if (iss >> lvl >> cost) {
                            unit::Element elem =
                                string_to_element(status_or_element);
                            session.shop_[slot] = std::make_pair(
                                make_slime_by_element(
                                    elem, unit::Owner::PlayerCtrl, lvl),
                                cost);
                        }
                    }
                }
            }
        } else if (section == "equip_pool") {
            std::string key;
            if (iss >> key) {
                if (key == "item") {
                    std::string item_name;
                    if (iss >> item_name) {
                        session.equip_pool_.push_back(
                            string_to_equipment(item_name));
                    }
                }
            }
        } else if (section == "board") {
            std::string key;
            int r, c;
            if (iss >> key >> r >> c) {
                if (key == "grid") {
                    std::string elem_str, owner_str, state_str, eq_str;
                    int hp, max_hp, atk, range, max_mana, mana, lvl, shield;
                    int attack_cooldown, move_cooldown, stun_ticks,
                        attack_interval, move_interval;
                    if (iss >> elem_str >> owner_str >> state_str >> hp >>
                        max_hp >> atk >> range >> max_mana >> mana >> lvl >>
                        shield >> eq_str >> attack_cooldown >> move_cooldown >>
                        stun_ticks >> attack_interval >> move_interval) {
                        unit::Element elem = string_to_element(elem_str);
                        unit::Owner owner = string_to_owner(owner_str);
                        unit::State state = string_to_state(state_str);
                        unit::Equipment eq = string_to_equipment(eq_str);

                        auto u_variant =
                            make_slime_by_element(elem, owner, lvl);
                        auto &s = unit::stats(u_variant);
                        s.state = state;
                        s.hp = hp;
                        s.max_hp = max_hp;
                        s.atk = atk;
                        s.range = range;
                        s.max_mana = max_mana;
                        s.mana = mana;
                        s.shield = shield;
                        s.equipped = eq;
                        s.attack_cooldown = attack_cooldown;
                        s.move_cooldown = move_cooldown;
                        s.stun_ticks = stun_ticks;
                        s.attack_interval = attack_interval;
                        s.move_interval = move_interval;

                        set_unit(session.board_, HexCoord{r, c},
                                 std::make_shared<unit::Unit>(u_variant));
                    }
                }
            }
        } else if (section == "bench") {
            std::string key;
            int idx;
            if (iss >> key >> idx) {
                if (key == "bench") {
                    std::string elem_str, owner_str, state_str, eq_str;
                    int hp, max_hp, atk, range, max_mana, mana, lvl, shield;
                    int attack_cooldown, move_cooldown, stun_ticks,
                        attack_interval, move_interval;
                    if (iss >> elem_str >> owner_str >> state_str >> hp >>
                        max_hp >> atk >> range >> max_mana >> mana >> lvl >>
                        shield >> eq_str >> attack_cooldown >> move_cooldown >>
                        stun_ticks >> attack_interval >> move_interval) {
                        unit::Element elem = string_to_element(elem_str);
                        unit::Owner owner = string_to_owner(owner_str);
                        unit::State state = string_to_state(state_str);
                        unit::Equipment eq = string_to_equipment(eq_str);

                        auto u_variant =
                            make_slime_by_element(elem, owner, lvl);
                        auto &s = unit::stats(u_variant);
                        s.state = state;
                        s.hp = hp;
                        s.max_hp = max_hp;
                        s.atk = atk;
                        s.range = range;
                        s.max_mana = max_mana;
                        s.mana = mana;
                        s.shield = shield;
                        s.equipped = eq;
                        s.attack_cooldown = attack_cooldown;
                        s.move_cooldown = move_cooldown;
                        s.stun_ticks = stun_ticks;
                        s.attack_interval = attack_interval;
                        s.move_interval = move_interval;

                        set_unit(session.board_, LinearCoord{idx},
                                 std::make_shared<unit::Unit>(u_variant));
                    }
                }
            }
        }
    }

    return true;
}

} // namespace Synera::engine
