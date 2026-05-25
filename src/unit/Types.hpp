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
