#pragma once

#include "common/Serialization.hpp"
#include "common/__cpo.hpp"
#include "engine/Coord.hpp"
#include "unit/Types.hpp"
#include <string>
#include <utility>
#include <variant>

namespace Synera::engine {
class Board;
} // namespace Synera::engine

namespace Synera::unit {

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
    constexpr void operator()(T &&a, int amount) const
        noexcept(noexcept(tag_invoke(__tag::take_damage_t{}, std::forward<T>(a),
                                     amount))) {
        tag_invoke(__tag::take_damage_t{}, std::forward<T>(a), amount);
    }
};

struct normal_attack_fn {
    template <typename T, typename B, typename C>
    constexpr void operator()(T &&a, B &&board, C &&target) const
        noexcept(noexcept(tag_invoke(__tag::normal_attack_t{},
                                     std::forward<T>(a), std::forward<B>(board),
                                     std::forward<C>(target)))) {
        tag_invoke(__tag::normal_attack_t{}, std::forward<T>(a),
                   std::forward<B>(board), std::forward<C>(target));
    }
};
} // namespace __fn

inline constexpr __fn::take_damage_fn take_damage{};
inline constexpr __fn::normal_attack_fn normal_attack{};

template <typename T>
constexpr void tag_invoke(__tag::take_damage_t, T &&a, int amount) {
    auto &s = stats(a);
    if (std::holds_alternative<CryoDrop>(s.equipped)) {
        amount = int(amount * 0.90);
    }
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

constexpr double get_star_scale(int level) noexcept {
    switch (level) {
    case 1:
        return 1.0;
    case 2:
        return 2.0;
    case 3:
        return 3.5;
    case 4:
        return 5.5;
    default:
        return 1.0;
    }
}

struct PyroSlime {
    UnitStats stats_;
    constexpr PyroSlime(Owner owner = Owner::PlayerCtrl, int level = 1) noexcept
        : stats_{owner,
                 State::Idle,
                 int(350 * get_star_scale(level)),
                 int(350 * get_star_scale(level)),
                 int(35 * get_star_scale(level)),
                 1,
                 60,
                 0,
                 level,
                 0} {}

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
    constexpr HydroSlime(Owner owner = Owner::PlayerCtrl,
                         int level = 1) noexcept
        : stats_{owner,
                 State::Idle,
                 int(400 * get_star_scale(level)),
                 int(400 * get_star_scale(level)),
                 int(30 * get_star_scale(level)),
                 2,
                 60,
                 0,
                 level,
                 0} {}

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
    friend void tag_invoke(__tag::normal_attack_t, HydroSlime &u,
                           engine::Board &board, engine::HexCoord target);
};

struct AnemoSlime {
    UnitStats stats_;
    constexpr AnemoSlime(Owner owner = Owner::PlayerCtrl,
                         int level = 1) noexcept
        : stats_{owner,
                 State::Idle,
                 int(300 * get_star_scale(level)),
                 int(300 * get_star_scale(level)),
                 int(40 * get_star_scale(level)),
                 3,
                 50,
                 0,
                 level,
                 0} {}

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
    constexpr GeoSlime(Owner owner = Owner::PlayerCtrl, int level = 1) noexcept
        : stats_{owner,
                 State::Idle,
                 int(450 * get_star_scale(level)),
                 int(450 * get_star_scale(level)),
                 int(25 * get_star_scale(level)),
                 1,
                 70,
                 0,
                 level,
                 int(200 * get_star_scale(level))} {}

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
    constexpr ElectroSlime(Owner owner = Owner::PlayerCtrl,
                           int level = 1) noexcept
        : stats_{owner,
                 State::Idle,
                 int(320 * get_star_scale(level)),
                 int(320 * get_star_scale(level)),
                 int(38 * get_star_scale(level)),
                 2,
                 60,
                 0,
                 level,
                 0} {}

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
    constexpr CryoSlime(Owner owner = Owner::PlayerCtrl, int level = 1) noexcept
        : stats_{owner,
                 State::Idle,
                 int(380 * get_star_scale(level)),
                 int(380 * get_star_scale(level)),
                 int(32 * get_star_scale(level)),
                 2,
                 60,
                 0,
                 level,
                 int(150 * get_star_scale(level))} {}

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

constexpr const UnitStats &tag_invoke(__tag::stats_t, const Unit &v) noexcept {
    return std::visit(
        [](const auto &u) -> const UnitStats & { return stats(u); }, v);
}

constexpr UnitStats &tag_invoke(__tag::stats_t, Unit &v) noexcept {
    return std::visit([](auto &u) -> UnitStats & { return stats(u); }, v);
}

inline std::string tag_invoke(__tag::name_t, const Unit &v) noexcept {
    return std::visit([](const auto &u) -> std::string { return name(u); }, v);
}

constexpr Element tag_invoke(__tag::element_t, const Unit &v) noexcept {
    return std::visit([](const auto &u) -> Element { return element(u); }, v);
}

template <typename B, typename C>
void tag_invoke(__tag::cast_skill_t, Unit &v, B &&board, C &&target) {
    std::visit([&board, &target](auto &u) { cast_skill(u, board, target); }, v);
}

inline void tag_invoke(__tag::take_damage_t, Unit &v, int amount) {
    std::visit([amount](auto &u) { take_damage(u, amount); }, v);
}

template <typename B, typename C>
void tag_invoke(__tag::normal_attack_t, Unit &v, B &&board, C &&target) {
    std::visit([&board, &target](auto &u) { normal_attack(u, board, target); },
               v);
}

inline std::string element_to_string(Element elem) {
    switch (elem) {
    case Element::Pyro:
        return "Pyro";
    case Element::Hydro:
        return "Hydro";
    case Element::Anemo:
        return "Anemo";
    case Element::Geo:
        return "Geo";
    case Element::Electro:
        return "Electro";
    case Element::Cryo:
        return "Cryo";
    }
    return "Pyro";
}

inline Element string_to_element(const std::string &str) {
    if (str == "Pyro")
        return Element::Pyro;
    if (str == "Hydro")
        return Element::Hydro;
    if (str == "Anemo")
        return Element::Anemo;
    if (str == "Geo")
        return Element::Geo;
    if (str == "Electro")
        return Element::Electro;
    if (str == "Cryo")
        return Element::Cryo;
    return Element::Pyro;
}

inline std::string owner_to_string(Owner owner) {
    return owner == Owner::PlayerCtrl ? "Player" : "Enemy";
}

inline Owner string_to_owner(const std::string &str) {
    return str == "Player" ? Owner::PlayerCtrl : Owner::EnemyCtrl;
}

inline std::string state_to_string(State state) {
    switch (state) {
    case State::Idle:
        return "Idle";
    case State::Moving:
        return "Moving";
    case State::Attacking:
        return "Attacking";
    case State::Casting:
        return "Casting";
    case State::Dead:
        return "Dead";
    }
    return "Idle";
}

inline State string_to_state(const std::string &str) {
    if (str == "Idle")
        return State::Idle;
    if (str == "Moving")
        return State::Moving;
    if (str == "Attacking")
        return State::Attacking;
    if (str == "Casting")
        return State::Casting;
    if (str == "Dead")
        return State::Dead;
    return State::Idle;
}

inline Unit make_slime_by_element(Element elem, Owner owner, int level) {
    switch (elem) {
    case Element::Pyro:
        return PyroSlime(owner, level);
    case Element::Hydro:
        return HydroSlime(owner, level);
    case Element::Anemo:
        return AnemoSlime(owner, level);
    case Element::Geo:
        return GeoSlime(owner, level);
    case Element::Electro:
        return ElectroSlime(owner, level);
    case Element::Cryo:
        return CryoSlime(owner, level);
    }
    return PyroSlime(owner, level);
}

} // namespace Synera::unit

namespace Synera::serialization {

inline void tag_invoke(serialize_t, std::ostream &out, const unit::Unit &u) {
    auto &s = unit::stats(u);
    out << unit::element_to_string(unit::element(u)) << " "
        << unit::owner_to_string(s.owner) << " "
        << unit::state_to_string(s.state) << " " << s.hp << " " << s.max_hp
        << " " << s.atk << " " << s.range << " " << s.max_mana << " " << s.mana
        << " " << s.level << " " << s.shield << " ";
    serialize(out, s.equipped);
    out << " " << s.attack_cooldown << " " << s.move_cooldown << " "
        << s.stun_ticks << " " << s.attack_interval << " " << s.move_interval;
}

inline void tag_invoke(deserialize_t, std::istream &in, unit::Unit &u) {
    std::string elem_str, owner_str, state_str;
    int hp, max_hp, atk, range, max_mana, mana, lvl, shield;
    int attack_cooldown, move_cooldown, stun_ticks, attack_interval,
        move_interval;

    if (in >> elem_str >> owner_str >> state_str >> hp >> max_hp >> atk >>
        range >> max_mana >> mana >> lvl >> shield) {
        unit::Element elem = unit::string_to_element(elem_str);
        unit::Owner owner = unit::string_to_owner(owner_str);
        unit::State state = unit::string_to_state(state_str);

        unit::Equipment eq;
        deserialize(in, eq);

        if (in >> attack_cooldown >> move_cooldown >> stun_ticks >>
            attack_interval >> move_interval) {
            u = unit::make_slime_by_element(elem, owner, lvl);
            auto &s = unit::stats(u);
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
        }
    }
}

} // namespace Synera::serialization
