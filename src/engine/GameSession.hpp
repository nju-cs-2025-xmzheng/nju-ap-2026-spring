#pragma once

#include "common/Config.hpp"
#include "common/Serialization.hpp"
#include "engine/Board.hpp"
#include "unit/Unit.hpp"
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <variant>
#include <vector>

namespace Synera::engine {

using Coord = std::variant<HexCoord, LinearCoord>;

struct Player {
    int hp = 100;
    int gold = 15;
    int level = 1;
};

class GameSession {
  public:
    Player player_;
    Board board_;
    std::array<std::optional<std::pair<unit::Unit, int>>, 5> shop_{};
    std::vector<unit::Equipment> equip_pool_;
    std::mt19937 rng_;
    int round_ = 1;
    bool shop_frozen_ = false;

    GameSession() {
        std::random_device rd;
        rng_.seed(rd());
        init_board(board_);
        refresh_shop(true);
        spawn_enemies();
    }

    bool refresh_shop(bool free = false) {
        if (!free) {
            if (player_.gold < 1) {
                return false;
            }
            player_.gold -= 1;
        }

        for (int i = 0; i < 5; ++i) {
            std::uniform_int_distribution<int> elem_dist(0, 5);
            unit::Element elem = static_cast<unit::Element>(elem_dist(rng_));

            int star_level = 1;
            std::uniform_int_distribution<int> pct_dist(0, 99);
            int roll = pct_dist(rng_);

            if (player_.level <= 2) {
                star_level = 1;
            } else if (player_.level <= 4) {
                if (roll < 80) {
                    star_level = 1;
                } else {
                    star_level = 2;
                }
            } else { // level >= 5
                if (roll < 60) {
                    star_level = 1;
                } else if (roll < 90) {
                    star_level = 2;
                } else {
                    star_level = 3;
                }
            }

            int cost = 0;
            if (star_level == 1)
                cost = 2;
            else if (star_level == 2)
                cost = 5;
            else if (star_level == 3)
                cost = 14;

            unit::Unit u = make_slime(elem, star_level);
            shop_[i] = std::make_pair(u, cost);
        }
        return true;
    }

    bool buy_unit(int slot_index) {
        if (slot_index < 0 || slot_index >= 5 ||
            !shop_[slot_index].has_value()) {
            return false;
        }

        auto &[unit, cost] = *shop_[slot_index];
        if (player_.gold < cost) {
            return false;
        }

        int empty_bench_idx = -1;
        for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
            LinearCoord c{i};
            if (!is_occupied(board_, c)) {
                empty_bench_idx = i;
                break;
            }
        }

        if (empty_bench_idx == -1) {
            return false;
        }

        player_.gold -= cost;
        LinearCoord target_coord{empty_bench_idx};
        set_unit(board_, target_coord, std::make_shared<unit::Unit>(unit));
        shop_[slot_index] = std::nullopt;
        check_and_merge(target_coord);

        return true;
    }

    bool buy_level() {
        int cost = player_.level * 5 + 5;
        if (player_.gold < cost) {
            return false;
        }
        player_.gold -= cost;
        player_.level++;
        return true;
    }

    bool equip_unit(Coord coord, size_t pool_index) {
        if (pool_index >= equip_pool_.size()) {
            return false;
        }
        auto unit_ptr = get_unit(board_, coord);
        if (!unit_ptr) {
            return false;
        }
        auto &stats = unit::stats(*unit_ptr);
        if (!std::holds_alternative<std::monostate>(stats.equipped)) {
            return false; // already has equipment
        }
        stats.equipped = equip_pool_[pool_index];
        equip_pool_.erase(equip_pool_.begin() + pool_index);
        return true;
    }

    void check_and_merge(
        std::optional<Coord> most_recently_acquired = std::nullopt) {
        std::map<std::pair<unit::Element, int>, std::vector<Coord>> groups;
        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                HexCoord coord{r, c};
                if (auto u_ptr = get_unit(board_, coord)) {
                    auto &stats = unit::stats(*u_ptr);
                    if (stats.owner == unit::Owner::PlayerCtrl &&
                        stats.level < 4) {
                        groups[{unit::element(*u_ptr), stats.level}].push_back(
                            coord);
                    }
                }
            }
        }

        for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
            LinearCoord coord{i};
            if (auto u_ptr = get_unit(board_, coord)) {
                auto &stats = unit::stats(*u_ptr);
                if (stats.owner == unit::Owner::PlayerCtrl && stats.level < 4) {
                    groups[{unit::element(*u_ptr), stats.level}].push_back(
                        coord);
                }
            }
        }

        for (auto &[key, coords] : groups) {
            if (coords.size() >= 3) {
                Coord c1 = coords[0];
                Coord c2 = coords[1];
                Coord c3 = coords[2];

                Coord target_coord = c3;
                bool has_board = false;
                for (Coord c : {c1, c2, c3}) {
                    if (std::holds_alternative<HexCoord>(c)) {
                        target_coord = c;
                        has_board = true;
                        break;
                    }
                }

                if (!has_board && most_recently_acquired.has_value()) {
                    for (Coord c : {c1, c2, c3}) {
                        if (c == *most_recently_acquired) {
                            target_coord = c;
                            break;
                        }
                    }
                }

                std::vector<Coord> others;
                for (Coord c : {c1, c2, c3}) {
                    if (!(c == target_coord)) {
                        others.push_back(c);
                    }
                }
                while (others.size() < 2) {
                    others.push_back(target_coord);
                }
                for (Coord c : {c1, c2, c3}) {
                    auto u_ptr = get_unit(board_, c);
                    if (u_ptr) {
                        auto &stats = unit::stats(*u_ptr);
                        if (!std::holds_alternative<std::monostate>(
                                stats.equipped)) {
                            equip_pool_.push_back(stats.equipped);
                            stats.equipped = std::monostate{};
                        }
                    }
                }
                auto target_ptr = get_unit(board_, target_coord);
                if (target_ptr) {
                    upgrade_unit(*target_ptr);
                }
                remove_unit(board_, others[0]);
                remove_unit(board_, others[1]);
                check_and_merge(target_coord);
                return;
            }
        }
    }

  public:
    void spawn_enemies() {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                remove_unit(board_, HexCoord{r, c});
            }
        }

        int count = std::min(6, round_);
        int star_level = std::min(4, (round_ - 1) / 3 + 1);

        for (int i = 0; i < count; ++i) {
            unit::Element elem = static_cast<unit::Element>(i % 6);
            HexCoord pos{1, 1 + i};
            set_unit(board_, pos,
                     std::make_shared<unit::Unit>(
                         make_slime(elem, star_level, unit::Owner::EnemyCtrl)));
        }
    }

  private:
    static unit::Unit make_slime(unit::Element elem, int star_level,
                                 unit::Owner owner = unit::Owner::PlayerCtrl) {
        switch (elem) {
        case unit::Element::Pyro:
            return unit::PyroSlime(owner, star_level);
        case unit::Element::Hydro:
            return unit::HydroSlime(owner, star_level);
        case unit::Element::Anemo:
            return unit::AnemoSlime(owner, star_level);
        case unit::Element::Geo:
            return unit::GeoSlime(owner, star_level);
        case unit::Element::Electro:
            return unit::ElectroSlime(owner, star_level);
        case unit::Element::Cryo:
            return unit::CryoSlime(owner, star_level);
        }
        return unit::PyroSlime(owner, star_level);
    }

    static void upgrade_unit(unit::Unit &u) {
        std::visit(
            [&u](auto &concrete_unit) {
                using T = std::decay_t<decltype(concrete_unit)>;
                auto stats = unit::stats(concrete_unit);
                u = T(stats.owner, stats.level + 1);
            },
            u);
    }
};

} // namespace Synera::engine

namespace Synera::serialization {

inline void tag_invoke(serialize_t, std::ostream &out,
                       const engine::Player &player) {
    out << "hp " << player.hp << "\n";
    out << "gold " << player.gold << "\n";
    out << "level " << player.level << "\n";
}

inline void tag_invoke(deserialize_t, std::istream &in,
                       engine::Player &player) {
    std::string line;
    while (true) {
        auto pos = in.tellg();
        if (!std::getline(in, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line == "[player]") {
            continue;
        }
        if (line.front() == '[') {
            in.seekg(pos);
            break;
        }
        std::istringstream iss(line);
        std::string key;
        int val;
        if (iss >> key >> val) {
            if (key == "hp") {
                player.hp = val;
            } else if (key == "gold") {
                player.gold = val;
            } else if (key == "level") {
                player.level = val;
            } else {
                in.seekg(pos);
                break;
            }
        }
    }
}

inline void tag_invoke(serialize_t, std::ostream &out,
                       const engine::GameSession &session) {
    // 1. Save Player
    out << "[player]\n";
    serialize(out, session.player_);
    out << "round " << session.round_ << "\n";
    out << "shop_frozen " << (session.shop_frozen_ ? 1 : 0) << "\n\n";

    // 2. Save Shop
    out << "[shop]\n";
    for (int i = 0; i < 5; ++i) {
        if (session.shop_[i].has_value()) {
            auto &[u, cost] = *session.shop_[i];
            out << "slot " << i << " "
                << unit::element_to_string(unit::element(u)) << " "
                << unit::stats(u).level << " " << cost << "\n";
        } else {
            out << "slot " << i << " empty\n";
        }
    }
    out << "\n";

    // 3. Save Equip Pool
    out << "[equip_pool]\n";
    out << "count " << session.equip_pool_.size() << "\n";
    for (const auto &eq : session.equip_pool_) {
        out << "item ";
        serialize(out, eq);
        out << "\n";
    }
    out << "\n";

    // 4. Save Board and Bench
    serialize(out, session.board_);
}

inline void tag_invoke(deserialize_t, std::istream &in,
                       engine::GameSession &session) {
    // Reset session state
    session.player_ = engine::Player{};
    engine::init_board(session.board_);
    session.shop_.fill(std::nullopt);
    session.equip_pool_.clear();
    session.shop_frozen_ = false;

    std::string line;
    std::string section = "";

    while (true) {
        auto pos = in.tellg();
        if (!std::getline(in, line)) {
            break;
        }
        if (line.empty())
            continue;

        if (line == "[player]") {
            deserialize(in, session.player_);
            section = "player";
            continue;
        } else if (line == "[shop]") {
            section = "shop";
            continue;
        } else if (line == "[equip_pool]") {
            section = "equip_pool";
            continue;
        } else if (line == "[board]" || line == "[bench]") {
            in.seekg(pos);
            deserialize(in, session.board_);
            continue;
        }

        std::istringstream iss(line);
        if (section == "player") {
            std::string key;
            int val;
            if (iss >> key >> val) {
                if (key == "round") {
                    session.round_ = val;
                } else if (key == "shop_frozen") {
                    session.shop_frozen_ = (val != 0);
                }
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
                                unit::string_to_element(status_or_element);
                            session.shop_[slot] = std::make_pair(
                                unit::make_slime_by_element(
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
                    unit::Equipment eq;
                    deserialize(iss, eq);
                    session.equip_pool_.push_back(eq);
                }
            }
        }
    }
}

} // namespace Synera::serialization
