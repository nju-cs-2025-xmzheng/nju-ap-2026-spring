#pragma once

#include "common/Serialization.hpp"
#include "unit/Types.hpp"
#include "unit/Unit.hpp"
#include <string>
#include <variant>

namespace Synera::unit {

namespace __tag {
struct apply_equipment_t {};
struct equip_t {};

inline std::string tag_invoke(name_t, std::monostate) { return "None"; }
inline void tag_invoke(apply_equipment_t, std::monostate, UnitStats &) {}
} // namespace __tag

namespace __fn {
struct apply_equipment_fn {
    template <typename E, typename S>
    constexpr void operator()(E &&eq, S &stats) const
        noexcept(noexcept(tag_invoke(__tag::apply_equipment_t{},
                                     std::forward<E>(eq), stats))) {
        tag_invoke(__tag::apply_equipment_t{}, std::forward<E>(eq), stats);
    }
};

struct equip_fn {
    template <typename U, typename E>
    constexpr void operator()(U &&u, E &&eq) const
        noexcept(noexcept(tag_invoke(__tag::equip_t{}, std::forward<U>(u),
                                     std::forward<E>(eq)))) {
        tag_invoke(__tag::equip_t{}, std::forward<U>(u), std::forward<E>(eq));
    }
};
} // namespace __fn

inline constexpr __fn::apply_equipment_fn apply_equipment{};
inline constexpr __fn::equip_fn equip{};

inline std::string tag_invoke(__tag::name_t, const PyroDrop &) {
    return "Pyro Drop";
}
inline std::string tag_invoke(__tag::name_t, const HydroDrop &) {
    return "Hydro Drop";
}
inline std::string tag_invoke(__tag::name_t, const AnemoDrop &) {
    return "Anemo Drop";
}
inline std::string tag_invoke(__tag::name_t, const GeoDrop &) {
    return "Geo Drop";
}
inline std::string tag_invoke(__tag::name_t, const ElectroDrop &) {
    return "Electro Drop";
}
inline std::string tag_invoke(__tag::name_t, const CryoDrop &) {
    return "Cryo Drop";
}

void tag_invoke(__tag::apply_equipment_t, const PyroDrop &, UnitStats &s);
void tag_invoke(__tag::apply_equipment_t, const HydroDrop &, UnitStats &s);
void tag_invoke(__tag::apply_equipment_t, const AnemoDrop &, UnitStats &s);
void tag_invoke(__tag::apply_equipment_t, const GeoDrop &, UnitStats &s);
void tag_invoke(__tag::apply_equipment_t, const ElectroDrop &, UnitStats &s);
void tag_invoke(__tag::apply_equipment_t, const CryoDrop &, UnitStats &s);

template <typename T>
inline void tag_invoke(__tag::equip_t, T &&u, Equipment eq) {
    auto &s = stats(u);
    s.equipped = eq;
    apply_equipment(eq, s);
}

inline void tag_invoke(__tag::apply_equipment_t, const Equipment &v,
                       UnitStats &stats) {
    std::visit([&stats](const auto &eq) { apply_equipment(eq, stats); }, v);
}

inline std::string tag_invoke(__tag::name_t, const Equipment &v) {
    return std::visit([](const auto &eq) -> std::string { return name(eq); },
                      v);
}

template <typename E> inline void tag_invoke(__tag::equip_t, Unit &v, E &&eq) {
    std::visit([&eq](auto &u) { equip(u, std::forward<E>(eq)); }, v);
}

inline std::string equipment_to_string(const Equipment &eq) {
    if (std::holds_alternative<PyroDrop>(eq))
        return "PyroDrop";
    if (std::holds_alternative<HydroDrop>(eq))
        return "HydroDrop";
    if (std::holds_alternative<AnemoDrop>(eq))
        return "AnemoDrop";
    if (std::holds_alternative<GeoDrop>(eq))
        return "GeoDrop";
    if (std::holds_alternative<ElectroDrop>(eq))
        return "ElectroDrop";
    if (std::holds_alternative<CryoDrop>(eq))
        return "CryoDrop";
    return "None";
}

inline Equipment string_to_equipment(const std::string &str) {
    if (str == "PyroDrop")
        return PyroDrop{};
    if (str == "HydroDrop")
        return HydroDrop{};
    if (str == "AnemoDrop")
        return AnemoDrop{};
    if (str == "GeoDrop")
        return GeoDrop{};
    if (str == "ElectroDrop")
        return ElectroDrop{};
    if (str == "CryoDrop")
        return CryoDrop{};
    return std::monostate{};
}

} // namespace Synera::unit

namespace Synera::serialization {

inline void tag_invoke(serialize_t, std::ostream &os,
                       const unit::Equipment &eq) {
    os << unit::equipment_to_string(eq);
}

inline void tag_invoke(deserialize_t, std::istream &is, unit::Equipment &eq) {
    std::string str;
    if (is >> str) {
        eq = unit::string_to_equipment(str);
    }
}

} // namespace Synera::serialization
