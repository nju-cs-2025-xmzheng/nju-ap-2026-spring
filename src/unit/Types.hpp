#pragma once

#include <variant>

namespace Synera::unit {

enum class Owner { PlayerCtrl, EnemyCtrl };
enum class State { Idle, Moving, Attacking, Casting, Dead };
enum class Element { Anemo, Geo, Electro, Cryo, Pyro, Hydro };

struct PyroDrop {};
struct HydroDrop {};
struct AnemoDrop {};
struct GeoDrop {};
struct ElectroDrop {};
struct CryoDrop {};

using Equipment = std::variant<std::monostate, PyroDrop, HydroDrop, AnemoDrop,
                               GeoDrop, ElectroDrop, CryoDrop>;

struct UnitStats {
    Owner owner = Owner::PlayerCtrl;
    State state = State::Idle;
    int hp = 0;
    int max_hp = 0;
    int atk = 0;
    int range = 0;
    int max_mana = 0;
    int mana = 0;
    int level = 1;
    int shield = 0;
    Equipment equipped = std::monostate{};

    // FSM & Combat
    int attack_cooldown = 0;
    int move_cooldown = 0;
    int stun_ticks = 0;
    int attack_interval = 60; // default 60 ticks per attack
    int move_interval = 20;   // default 20 ticks per move
};

struct PyroSlime;
struct HydroSlime;
struct AnemoSlime;
struct GeoSlime;
struct ElectroSlime;
struct CryoSlime;

using Unit = std::variant<PyroSlime, HydroSlime, AnemoSlime, GeoSlime,
                          ElectroSlime, CryoSlime>;

} // namespace Synera::unit

#include "common/Serialization.hpp"

namespace Synera::serialization {
void tag_invoke(serialize_t, std::ostream &, const unit::Equipment &);
void tag_invoke(deserialize_t, std::istream &, unit::Equipment &);
void tag_invoke(serialize_t, std::ostream &, const unit::Unit &);
void tag_invoke(deserialize_t, std::istream &, unit::Unit &);
} // namespace Synera::serialization
