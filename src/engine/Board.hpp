#pragma once

#include "common/Config.hpp"
#include "common/Serialization.hpp"
#include "common/__cpo.hpp"
#include "engine/Coord.hpp"
#include "unit/Unit.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace Synera::engine {

namespace __tag {
struct init_board_t {};
struct get_unit_t {};
struct set_unit_t {};
struct is_occupied_t {};
struct remove_unit_t {};
struct count_player_units_on_board_t {};
struct move_unit_t {};
struct find_path_t {};
struct select_target_t {};
} // namespace __tag

namespace __fn {
struct init_board_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::init_board_t{},
                                     std::forward<T>(a)))) -> decltype(auto) {
        return tag_invoke(__tag::init_board_t{}, std::forward<T>(a));
    }
};

struct get_unit_fn {
    template <typename B, typename C>
    constexpr auto operator()(B &&b, C &&c) const
        noexcept(noexcept(tag_invoke(__tag::get_unit_t{}, std::forward<B>(b),
                                     std::forward<C>(c)))) -> decltype(auto) {
        return tag_invoke(__tag::get_unit_t{}, std::forward<B>(b),
                          std::forward<C>(c));
    }
};

struct set_unit_fn {
    template <typename B, typename C, typename U>
    constexpr auto operator()(B &&b, C &&c, U &&u) const
        noexcept(noexcept(tag_invoke(__tag::set_unit_t{}, std::forward<B>(b),
                                     std::forward<C>(c), std::forward<U>(u))))
            -> decltype(auto) {
        return tag_invoke(__tag::set_unit_t{}, std::forward<B>(b),
                          std::forward<C>(c), std::forward<U>(u));
    }
};

struct is_occupied_fn {
    template <typename B, typename C>
    constexpr auto operator()(B &&b, C &&c) const
        noexcept(noexcept(tag_invoke(__tag::is_occupied_t{}, std::forward<B>(b),
                                     std::forward<C>(c)))) -> decltype(auto) {
        return tag_invoke(__tag::is_occupied_t{}, std::forward<B>(b),
                          std::forward<C>(c));
    }
};

struct remove_unit_fn {
    template <typename B, typename C>
    constexpr auto operator()(B &&b, C &&c) const
        noexcept(noexcept(tag_invoke(__tag::remove_unit_t{}, std::forward<B>(b),
                                     std::forward<C>(c)))) -> decltype(auto) {
        return tag_invoke(__tag::remove_unit_t{}, std::forward<B>(b),
                          std::forward<C>(c));
    }
};

struct count_player_units_on_board_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::count_player_units_on_board_t{},
                                     std::forward<T>(a)))) -> decltype(auto) {
        return tag_invoke(__tag::count_player_units_on_board_t{},
                          std::forward<T>(a));
    }
};

struct move_unit_fn {
    template <typename B, typename C1, typename C2, typename L>
    constexpr auto operator()(B &&b, C1 &&from, C2 &&to, L &&limit) const
        noexcept(noexcept(tag_invoke(__tag::move_unit_t{}, std::forward<B>(b),
                                     std::forward<C1>(from),
                                     std::forward<C2>(to),
                                     std::forward<L>(limit))))
            -> decltype(auto) {
        return tag_invoke(__tag::move_unit_t{}, std::forward<B>(b),
                          std::forward<C1>(from), std::forward<C2>(to),
                          std::forward<L>(limit));
    }
};

struct find_path_fn {
    template <typename B, typename C1, typename C2>
    constexpr auto operator()(B &&b, C1 &&start, C2 &&goal) const
        noexcept(noexcept(tag_invoke(__tag::find_path_t{}, std::forward<B>(b),
                                     std::forward<C1>(start),
                                     std::forward<C2>(goal))))
            -> decltype(auto) {
        return tag_invoke(__tag::find_path_t{}, std::forward<B>(b),
                          std::forward<C1>(start), std::forward<C2>(goal));
    }
};

struct select_target_fn {
    template <typename B, typename U, typename C>
    constexpr auto operator()(B &&b, U &&u, C &&my_coord) const
        noexcept(noexcept(tag_invoke(__tag::select_target_t{},
                                     std::forward<B>(b), std::forward<U>(u),
                                     std::forward<C>(my_coord))))
            -> decltype(auto) {
        return tag_invoke(__tag::select_target_t{}, std::forward<B>(b),
                          std::forward<U>(u), std::forward<C>(my_coord));
    }
};
} // namespace __fn

inline constexpr __fn::init_board_fn init_board{};
inline constexpr __fn::get_unit_fn get_unit{};
inline constexpr __fn::set_unit_fn set_unit{};
inline constexpr __fn::is_occupied_fn is_occupied{};
inline constexpr __fn::remove_unit_fn remove_unit{};
inline constexpr __fn::count_player_units_on_board_fn
    count_player_units_on_board{};
inline constexpr __fn::move_unit_fn move_unit{};
inline constexpr __fn::find_path_fn find_path{};
inline constexpr __fn::select_target_fn select_target{};

class Board {
    using Coord = std::variant<HexCoord, LinearCoord>;

  private:
    std::array<
        std::array<std::shared_ptr<unit::Unit>, config::engine::BOARD_COLS>,
        config::engine::BOARD_ROWS>
        grid_{};
    std::array<std::shared_ptr<unit::Unit>, config::engine::BENCH_SIZE>
        bench_{};

  public:
    constexpr Board() noexcept = default;

    friend constexpr void tag_invoke(__tag::init_board_t, Board &b) noexcept {
        for (auto &row : b.grid_) {
            row.fill(nullptr);
        }
        b.bench_.fill(nullptr);
    }

    friend constexpr std::shared_ptr<unit::Unit>
    tag_invoke(__tag::get_unit_t, const Board &b, Coord coord) noexcept {
        return std::visit(
            [&b](auto &&c) -> std::shared_ptr<unit::Unit> {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, HexCoord>) {
                    if (in_range(c)) {
                        return b.grid_[c.r][c.c];
                    }
                } else if constexpr (std::is_same_v<T, LinearCoord>) {
                    if (in_range(c)) {
                        return b.bench_[c.x];
                    }
                }
                return nullptr;
            },
            coord);
    }

    friend constexpr bool
    tag_invoke(__tag::set_unit_t, Board &b, Coord coord,
               std::shared_ptr<unit::Unit> unit) noexcept {
        return std::visit(
            [&b, unit = std::move(unit)](auto &&c) mutable -> bool {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, HexCoord>) {
                    if (in_range(c)) {
                        b.grid_[c.r][c.c] = std::move(unit);
                        return true;
                    }
                } else if constexpr (std::is_same_v<T, LinearCoord>) {
                    if (in_range(c)) {
                        b.bench_[c.x] = std::move(unit);
                        return true;
                    }
                }
                return false;
            },
            coord);
    }

    friend constexpr bool tag_invoke(__tag::is_occupied_t, const Board &b,
                                     Coord coord) noexcept {
        return get_unit(b, coord) != nullptr;
    }

    friend constexpr void tag_invoke(__tag::remove_unit_t, Board &b,
                                     Coord coord) noexcept {
        set_unit(b, coord, nullptr);
    }

    static constexpr bool is_player_territory(HexCoord coord) noexcept {
        return coord.r >= config::engine::BOARD_ROWS / 2 &&
               coord.r < config::engine::BOARD_ROWS;
    }

    static constexpr bool is_enemy_territory(HexCoord coord) noexcept {
        return coord.r >= 0 && coord.r < config::engine::BOARD_ROWS / 2;
    }

    friend constexpr int tag_invoke(__tag::count_player_units_on_board_t,
                                    const Board &b) noexcept {
        int count = 0;
        for (const auto &row : b.grid_) {
            for (const auto &cell : row) {
                if (cell &&
                    unit::stats(*cell).owner == unit::Owner::PlayerCtrl) {
                    count++;
                }
            }
        }
        return count;
    }

    friend constexpr bool tag_invoke(__tag::move_unit_t, Board &b, Coord from,
                                     Coord to, int population_limit) noexcept {
        auto unit_from = get_unit(b, from);
        if (!unit_from) {
            return false;
        }
        auto unit_to = get_unit(b, to);

        // Destination is empty
        if (!unit_to) {
            bool is_from_bench = std::holds_alternative<LinearCoord>(from);
            bool is_to_board = std::holds_alternative<HexCoord>(to);
            if (is_to_board) {
                auto hex_to = std::get<HexCoord>(to);
                if (unit::stats(*unit_from).owner == unit::Owner::PlayerCtrl &&
                    !is_player_territory(hex_to)) {
                    return false;
                }
                if (is_from_bench &&
                    unit::stats(*unit_from).owner == unit::Owner::PlayerCtrl) {
                    if (count_player_units_on_board(b) >= population_limit) {
                        return false;
                    }
                }
            }
            set_unit(b, to, std::move(unit_from));
            remove_unit(b, from);
            return true;
        }
        // Destination is occupied
        else {
            bool is_from_board = std::holds_alternative<HexCoord>(from);
            bool is_to_board = std::holds_alternative<HexCoord>(to);
            if (is_to_board &&
                unit::stats(*unit_from).owner == unit::Owner::PlayerCtrl) {
                if (!is_player_territory(std::get<HexCoord>(to))) {
                    return false;
                }
            }
            if (is_from_board &&
                unit::stats(*unit_to).owner == unit::Owner::PlayerCtrl) {
                if (!is_player_territory(std::get<HexCoord>(from))) {
                    return false;
                }
            }
            if (unit::stats(*unit_from).owner != unit::stats(*unit_to).owner) {
                return false;
            }
            set_unit(b, from, nullptr);
            auto temp_unit = std::move(unit_to);
            set_unit(b, to, std::move(unit_from));
            set_unit(b, from, std::move(temp_unit));
            return true;
        }
    }

    friend std::vector<HexCoord> tag_invoke(__tag::find_path_t, const Board &b,
                                            HexCoord start, HexCoord goal) {
        std::vector<HexCoord> path;
        if (!in_range(start) || !in_range(goal))
            return path;

        std::vector<HexCoord> q;
        size_t head = 0;
        bool visited[config::engine::BOARD_ROWS][config::engine::BOARD_COLS] =
            {};
        HexCoord parent[config::engine::BOARD_ROWS][config::engine::BOARD_COLS];
        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                parent[r][c] = HexCoord{-1, -1};
            }
        }

        q.push_back(start);
        visited[start.r][start.c] = true;

        bool found = false;
        while (head < q.size()) {
            HexCoord curr = q[head++];
            if (curr == goal) {
                found = true;
                break;
            }

            for (HexCoord next : neighbor(curr)) {
                if (!visited[next.r][next.c]) {
                    bool is_obstacle = false;
                    if (!(next == goal) && is_occupied(b, next)) {
                        is_obstacle = true;
                    }
                    if (!is_obstacle) {
                        visited[next.r][next.c] = true;
                        parent[next.r][next.c] = curr;
                        q.push_back(next);
                    }
                }
            }
        }

        HexCoord end_cell = goal;
        if (!found) {
            int min_dist = 999999;
            HexCoord closest = start;
            for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
                for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                    if (visited[r][c]) {
                        HexCoord cell{r, c};
                        int d = distance(cell, goal);
                        if (d < min_dist) {
                            min_dist = d;
                            closest = cell;
                        }
                    }
                }
            }
            end_cell = closest;
        }

        if (!(end_cell == start)) {
            HexCoord curr = end_cell;
            while (!(curr == start)) {
                path.push_back(curr);
                curr = parent[curr.r][curr.c];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
        } else {
            path.push_back(start);
        }

        return path;
    }

    friend std::optional<HexCoord> tag_invoke(__tag::select_target_t,
                                              const Board &b,
                                              const unit::Unit &u,
                                              HexCoord my_coord) {
        std::optional<HexCoord> best_target = std::nullopt;
        int best_dist = 999999;
        int best_hp = -1;
        int best_c = 999999;
        int best_r = -1;

        unit::Owner my_owner = unit::stats(u).owner;

        for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
            for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
                HexCoord cell{r, c};
                if (auto target_ptr = get_unit(b, cell)) {
                    auto &target_stats = unit::stats(*target_ptr);
                    if (target_stats.owner != my_owner && target_stats.hp > 0) {
                        int dist = distance(my_coord, cell);
                        bool is_better = false;

                        if (!best_target.has_value()) {
                            is_better = true;
                        } else {
                            if (dist < best_dist) {
                                is_better = true;
                            } else if (dist == best_dist) {
                                if (target_stats.hp > best_hp) {
                                    is_better = true;
                                } else if (target_stats.hp == best_hp) {
                                    if (c < best_c) {
                                        is_better = true;
                                    } else if (c == best_c) {
                                        if (r > best_r) {
                                            is_better = true;
                                        }
                                    }
                                }
                            }
                        }

                        if (is_better) {
                            best_target = cell;
                            best_dist = dist;
                            best_hp = target_stats.hp;
                            best_c = c;
                            best_r = r;
                        }
                    }
                }
            }
        }
        return best_target;
    }
};

} // namespace Synera::engine

namespace Synera::serialization {

inline void tag_invoke(serialize_t, std::ostream &out,
                       const engine::Board &board) {
    // Save Board Grid (8x8)
    out << "[board]\n";
    for (int r = 0; r < config::engine::BOARD_ROWS; ++r) {
        for (int c = 0; c < config::engine::BOARD_COLS; ++c) {
            engine::HexCoord coord{r, c};
            if (auto u_ptr = engine::get_unit(board, coord)) {
                out << "grid " << r << " " << c << " ";
                serialize(out, *u_ptr);
                out << "\n";
            }
        }
    }
    out << "\n";

    // Save Bench (8 slots)
    out << "[bench]\n";
    for (int i = 0; i < config::engine::BENCH_SIZE; ++i) {
        engine::LinearCoord coord{i};
        if (auto u_ptr = engine::get_unit(board, coord)) {
            out << "bench " << i << " ";
            serialize(out, *u_ptr);
            out << "\n";
        }
    }
}

inline void tag_invoke(deserialize_t, std::istream &in, engine::Board &board) {
    engine::init_board(board);

    std::string line;
    std::string section = "board";

    while (true) {
        auto pos = in.tellg();
        if (!std::getline(in, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line == "[board]") {
            section = "board";
            continue;
        } else if (line == "[bench]") {
            section = "bench";
            continue;
        } else if (line.front() == '[') {
            in.seekg(pos);
            break;
        }

        std::istringstream iss(line);
        if (section == "board") {
            std::string key;
            int r, c;
            if (iss >> key >> r >> c && key == "grid") {
                unit::Unit u_val;
                deserialize(iss, u_val);
                engine::set_unit(board, engine::HexCoord{r, c},
                                 std::make_shared<unit::Unit>(u_val));
            }
        } else if (section == "bench") {
            std::string key;
            int idx;
            if (iss >> key >> idx && key == "bench") {
                unit::Unit u_val;
                deserialize(iss, u_val);
                engine::set_unit(board, engine::LinearCoord{idx},
                                 std::make_shared<unit::Unit>(u_val));
            }
        }
    }
}

} // namespace Synera::serialization
