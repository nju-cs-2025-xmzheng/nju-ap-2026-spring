#pragma once

#include "common/__cpo.hpp"
#include "engine/Coord.hpp"
#include <string>
#include <utility>
#include <variant>

namespace Synera::engine {
class Board;
} // namespace Synera::engine

namespace Synera::unit {

enum class Owner { PlayerCtrl, EnemyCtrl };
enum class State { Idle, Moving, Attacking, Casting, Dead };
enum class Element { Anemo, Geo, Electro, Cryo, Pyro, Hydro };

struct UnitStats {
    Owner owner = Owner::PlayerCtrl;
    State state = State::Idle;
    int hp = 0;
    int atk = 0;
    int range = 0;
    int max_mana = 0;
    int mana = 0;
    int level = 1;
    int shield = 0;
};

struct PyroSlime;
struct HydroSlime;
struct AnemoSlime;
struct GeoSlime;
struct ElectroSlime;
struct CryoSlime;

using Unit = std::variant<PyroSlime, HydroSlime, AnemoSlime, GeoSlime,
                          ElectroSlime, CryoSlime>;

namespace __tag {
struct stats_t {};
struct take_damage_t {};
struct normal_attack_t {};
struct cast_skill_t {};
struct name_t {};
struct element_t {};
} // namespace __tag

namespace __fn {
struct stats_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::stats_t{}, std::forward<T>(a))))
            -> decltype(auto) {
        return tag_invoke(__tag::stats_t{}, std::forward<T>(a));
    }
};

struct name_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::name_t{}, std::forward<T>(a))))
            -> decltype(auto) {
        return tag_invoke(__tag::name_t{}, std::forward<T>(a));
    }
};

struct element_fn {
    template <typename T>
    constexpr auto operator()(T &&a) const
        noexcept(noexcept(tag_invoke(__tag::element_t{}, std::forward<T>(a))))
            -> decltype(auto) {
        return tag_invoke(__tag::element_t{}, std::forward<T>(a));
    }
};

struct cast_skill_fn {
    template <typename T, typename B, typename C>
    constexpr auto operator()(T &&a, B &&board, C &&target) const
        noexcept(noexcept(tag_invoke(__tag::cast_skill_t{}, std::forward<T>(a),
                                     std::forward<B>(board),
                                     std::forward<C>(target))))
            -> decltype(auto) {
        return tag_invoke(__tag::cast_skill_t{}, std::forward<T>(a),
                          std::forward<B>(board), std::forward<C>(target));
    }
};
} // namespace __fn

inline constexpr __fn::stats_fn stats{};
inline constexpr __fn::name_fn name{};
inline constexpr __fn::element_fn element{};
inline constexpr __fn::cast_skill_fn cast_skill{};

namespace __fn {
struct take_damage_fn {
    template <typename T>
    constexpr void operator()(T &&a, int amount) const noexcept {
        if constexpr (requires {
                          tag_invoke(__tag::take_damage_t{}, std::forward<T>(a),
                                     amount);
                      }) {
            tag_invoke(__tag::take_damage_t{}, std::forward<T>(a), amount);
        } else {
            auto &s = stats(a);
            if (s.shield > 0) {
                if (amount <= s.shield) {
                    s.shield -= amount;
                    return;
                } else {
                    amount -= s.shield;
                    s.shield = 0;
                }
            }
            s.hp -= amount;
            if (s.hp <= 0) {
                s.hp = 0;
                s.state = State::Dead;
            }
        }
    }
};

struct normal_attack_fn {
    template <typename T, typename B, typename C>
    constexpr void operator()(T &&a, B &&board, C &&target) const;
};
} // namespace __fn

inline constexpr __fn::take_damage_fn take_damage{};
inline constexpr __fn::normal_attack_fn normal_attack{};

struct PyroSlime {
    UnitStats stats_;
    constexpr PyroSlime(Owner owner, int level = 1) noexcept
        : stats_{owner, State::Idle, 350 * level, 35 * level, 1,
                 60,    0,           level,       0} {}

    friend constexpr const UnitStats &tag_invoke(__tag::stats_t,
                                                 const PyroSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr UnitStats &tag_invoke(__tag::stats_t,
                                           PyroSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr std::string tag_invoke(__tag::name_t,
                                            const PyroSlime &) noexcept {
        return "Pyro Slime";
    }
    friend constexpr Element tag_invoke(__tag::element_t,
                                        const PyroSlime &) noexcept {
        return Element::Pyro;
    }
    friend void tag_invoke(__tag::cast_skill_t, PyroSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

struct HydroSlime {
    UnitStats stats_;
    constexpr HydroSlime(Owner owner, int level = 1) noexcept
        : stats_{owner, State::Idle, 400 * level, 30 * level, 2,
                 60,    0,           level,       0} {}

    friend constexpr const UnitStats &tag_invoke(__tag::stats_t,
                                                 const HydroSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr UnitStats &tag_invoke(__tag::stats_t,
                                           HydroSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr std::string tag_invoke(__tag::name_t,
                                            const HydroSlime &) noexcept {
        return "Hydro Slime";
    }
    friend constexpr Element tag_invoke(__tag::element_t,
                                        const HydroSlime &) noexcept {
        return Element::Hydro;
    }
    friend void tag_invoke(__tag::cast_skill_t, HydroSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

struct AnemoSlime {
    UnitStats stats_;
    constexpr AnemoSlime(Owner owner, int level = 1) noexcept
        : stats_{owner, State::Idle, 300 * level, 40 * level, 3,
                 50,    0,           level,       0} {}

    friend constexpr const UnitStats &tag_invoke(__tag::stats_t,
                                                 const AnemoSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr UnitStats &tag_invoke(__tag::stats_t,
                                           AnemoSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr std::string tag_invoke(__tag::name_t,
                                            const AnemoSlime &) noexcept {
        return "Anemo Slime";
    }
    friend constexpr Element tag_invoke(__tag::element_t,
                                        const AnemoSlime &) noexcept {
        return Element::Anemo;
    }
    friend void tag_invoke(__tag::cast_skill_t, AnemoSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

struct GeoSlime {
    UnitStats stats_;
    constexpr GeoSlime(Owner owner, int level = 1) noexcept
        : stats_{owner, State::Idle, 450 * level, 25 * level, 1,
                 70,    0,           level,       200 * level} {}

    friend constexpr const UnitStats &tag_invoke(__tag::stats_t,
                                                 const GeoSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr UnitStats &tag_invoke(__tag::stats_t,
                                           GeoSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr std::string tag_invoke(__tag::name_t,
                                            const GeoSlime &) noexcept {
        return "Geo Slime";
    }
    friend constexpr Element tag_invoke(__tag::element_t,
                                        const GeoSlime &) noexcept {
        return Element::Geo;
    }
    friend void tag_invoke(__tag::cast_skill_t, GeoSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

struct ElectroSlime {
    UnitStats stats_;
    constexpr ElectroSlime(Owner owner, int level = 1) noexcept
        : stats_{owner, State::Idle, 320 * level, 38 * level, 2,
                 60,    0,           level,       0} {}

    friend constexpr const UnitStats &
    tag_invoke(__tag::stats_t, const ElectroSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr UnitStats &tag_invoke(__tag::stats_t,
                                           ElectroSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr std::string tag_invoke(__tag::name_t,
                                            const ElectroSlime &) noexcept {
        return "Electro Slime";
    }
    friend constexpr Element tag_invoke(__tag::element_t,
                                        const ElectroSlime &) noexcept {
        return Element::Electro;
    }
    friend void tag_invoke(__tag::cast_skill_t, ElectroSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

struct CryoSlime {
    UnitStats stats_;
    constexpr CryoSlime(Owner owner, int level = 1) noexcept
        : stats_{owner, State::Idle, 380 * level, 32 * level, 2,
                 60,    0,           level,       150 * level} {}

    friend constexpr const UnitStats &tag_invoke(__tag::stats_t,
                                                 const CryoSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr UnitStats &tag_invoke(__tag::stats_t,
                                           CryoSlime &u) noexcept {
        return u.stats_;
    }
    friend constexpr std::string tag_invoke(__tag::name_t,
                                            const CryoSlime &) noexcept {
        return "Cryo Slime";
    }
    friend constexpr Element tag_invoke(__tag::element_t,
                                        const CryoSlime &) noexcept {
        return Element::Cryo;
    }
    friend void tag_invoke(__tag::cast_skill_t, CryoSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

template <typename... Ts>
constexpr const UnitStats &tag_invoke(__tag::stats_t,
                                      const std::variant<Ts...> &v) noexcept {
    return std::visit(
        [](const auto &u) -> const UnitStats & { return stats(u); }, v);
}

template <typename... Ts>
constexpr UnitStats &tag_invoke(__tag::stats_t,
                                std::variant<Ts...> &v) noexcept {
    return std::visit([](auto &u) -> UnitStats & { return stats(u); }, v);
}

template <typename... Ts>
std::string tag_invoke(__tag::name_t, const std::variant<Ts...> &v) noexcept {
    return std::visit([](const auto &u) -> std::string { return name(u); }, v);
}

template <typename... Ts>
constexpr Element tag_invoke(__tag::element_t,
                             const std::variant<Ts...> &v) noexcept {
    return std::visit([](const auto &u) -> Element { return element(u); }, v);
}

template <typename... Ts, typename B, typename C>
void tag_invoke(__tag::cast_skill_t, std::variant<Ts...> &v, B &&board,
                C &&target) {
    std::visit([&board, &target](auto &u) { cast_skill(u, board, target); }, v);
}

} // namespace Synera::unit
