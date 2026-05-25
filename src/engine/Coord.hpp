#pragma once

#include "common/Config.hpp"
#include "common/__cpo.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Synera::engine {

namespace __tag {
struct distance_t {};
struct in_range_t {};
struct neighbor_t {};
} // namespace __tag

namespace __fn {
struct distance_fn {
    template <typename T>
    constexpr auto operator()(T &&a, T &&b) const
        noexcept(noexcept(tag_invoke(__tag::distance_t{}, std::forward<T>(a),
                                     std::forward<T>(b)))) -> decltype(auto) {
        return tag_invoke(__tag::distance_t{}, std::forward<T>(a),
                          std::forward<T>(b));
    }
};
struct in_range_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::in_range_t{}, std::forward<T>(a))))
            -> decltype(auto) {
        return tag_invoke(__tag::in_range_t{}, std::forward<T>(a));
    }
};
struct neighbor_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::neighbor_t{}, std::forward<T>(a))))
            -> decltype(auto) {
        return tag_invoke(__tag::neighbor_t{}, std::forward<T>(a));
    }
};
} // namespace __fn

inline constexpr __fn::distance_fn distance{};
inline constexpr __fn::in_range_fn in_range{};
inline constexpr __fn::neighbor_fn neighbor{};

struct LinearCoord {
    int x = 0;

    constexpr LinearCoord() noexcept = default;
    constexpr LinearCoord(int x) noexcept : x(x) {}

    constexpr auto operator<=>(const LinearCoord &) const noexcept = default;

    friend constexpr int tag_invoke(__tag::distance_t, LinearCoord a,
                                    LinearCoord b) noexcept {
        return std::abs(a.x - b.x);
    }
    friend constexpr bool tag_invoke(__tag::in_range_t,
                                     LinearCoord a) noexcept {
        return 0 <= a.x && a.x < config::engine::BENCH_SIZE;
    }
    friend constexpr auto tag_invoke(__tag::neighbor_t,
                                     LinearCoord a) noexcept {
        std::vector<LinearCoord> neighbors;
        for (int nx : {a.x - 1, a.x + 1}) {
            if (LinearCoord n{nx}; in_range(n)) {
                neighbors.push_back(n);
            }
        }
        return neighbors;
    }
};

struct HexCoord {
    int r = 0;
    int c = 0;

    constexpr HexCoord() noexcept = default;
    constexpr HexCoord(int r, int c) noexcept : r(r), c(c) {}

    constexpr auto operator<=>(const HexCoord &) const noexcept = default;

    friend constexpr int tag_invoke(__tag::distance_t, HexCoord a,
                                    HexCoord b) noexcept {
        int x1 = a.c - (a.r + (a.r & 1)) / 2;
        int z1 = a.r;
        int y1 = -x1 - z1;
        int x2 = b.c - (b.r + (b.r & 1)) / 2;
        int z2 = b.r;
        int y2 = -x2 - z2;
        return std::max(
            {std::abs(x1 - x2), std::abs(y1 - y2), std::abs(z1 - z2)});
    }

    friend constexpr bool tag_invoke(__tag::in_range_t, HexCoord a) noexcept {
        return 0 <= a.r && a.r < config::engine::BOARD_ROWS && 0 <= a.c &&
               a.c < config::engine::BOARD_COLS;
    }

    friend constexpr auto tag_invoke(__tag::neighbor_t, HexCoord a) noexcept {
        std::vector<HexCoord> neighbors;
        int shift = a.r & 1;
        for (HexCoord n : {HexCoord{a.r, a.c - 1}, HexCoord{a.r, a.c + 1},
                           HexCoord{a.r - 1, a.c - shift},
                           HexCoord{a.r - 1, a.c + 1 - shift},
                           HexCoord{a.r + 1, a.c - shift},
                           HexCoord{a.r + 1, a.c + 1 - shift}}) {
            if (in_range(n)) {
                neighbors.push_back(n);
            }
        }
        return neighbors;
    }
};

} // namespace Synera::engine
