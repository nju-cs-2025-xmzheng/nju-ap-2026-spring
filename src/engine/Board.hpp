#pragma once

#include "common/Config.hpp"
#include "common/__cpo.hpp"
#include "engine/Coord.hpp"
#include "unit/Unit.hpp"
#include <array>
#include <memory>
#include <variant>

namespace Synera::engine {

namespace __tag {
struct init_board_t {};
struct get_unit_t {};
struct set_unit_t {};
struct is_occupied_t {};
struct remove_unit_t {};
struct count_player_units_on_board_t {};
struct move_unit_t {};
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
} // namespace __fn

inline constexpr __fn::init_board_fn init_board{};
inline constexpr __fn::get_unit_fn get_unit{};
inline constexpr __fn::set_unit_fn set_unit{};
inline constexpr __fn::is_occupied_fn is_occupied{};
inline constexpr __fn::remove_unit_fn remove_unit{};
inline constexpr __fn::count_player_units_on_board_fn
    count_player_units_on_board{};
inline constexpr __fn::move_unit_fn move_unit{};

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
};

} // namespace Synera::engine
