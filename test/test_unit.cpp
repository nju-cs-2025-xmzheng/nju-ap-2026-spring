#include "unit/Unit.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <iostream>

using namespace Synera::unit;

void test_slime_types_and_cpos() {
    PyroSlime pyro(Owner::PlayerCtrl);
    HydroSlime hydro(Owner::PlayerCtrl);
    AnemoSlime anemo(Owner::PlayerCtrl);
    GeoSlime geo(Owner::EnemyCtrl);
    ElectroSlime electro(Owner::EnemyCtrl);
    CryoSlime cryo(Owner::EnemyCtrl);

    // 1. Verify Elements
    assert(element(pyro) == Element::Pyro);
    assert(element(hydro) == Element::Hydro);
    assert(element(anemo) == Element::Anemo);
    assert(element(geo) == Element::Geo);
    assert(element(electro) == Element::Electro);
    assert(element(cryo) == Element::Cryo);

    // 2. Verify Names
    assert(name(pyro) == "Pyro Slime");
    assert(name(hydro) == "Hydro Slime");
    assert(name(anemo) == "Anemo Slime");
    assert(name(geo) == "Geo Slime");
    assert(name(electro) == "Electro Slime");
    assert(name(cryo) == "Cryo Slime");

    // 3. Verify base stats
    assert(stats(pyro).hp == 350);
    assert(stats(pyro).atk == 35);
    assert(stats(pyro).range == 1);
    assert(stats(pyro).max_mana == 60);

    assert(stats(geo).hp == 450);
    assert(stats(geo).shield == 200); // starts with shield

    assert(stats(cryo).hp == 380);
    assert(stats(cryo).shield == 150); // starts with shield
}

void test_star_scaling() {
    // Multipliers: 1.0x, 2.0x, 3.5x, 5.5x
    assert(get_star_scale(1) == 1.0);
    assert(get_star_scale(2) == 2.0);
    assert(get_star_scale(3) == 3.5);
    assert(get_star_scale(4) == 5.5);

    PyroSlime p1(Owner::PlayerCtrl, 1);
    PyroSlime p2(Owner::PlayerCtrl, 2);
    PyroSlime p3(Owner::PlayerCtrl, 3);
    PyroSlime p4(Owner::PlayerCtrl, 4);

    assert(stats(p1).hp == 350);
    assert(stats(p1).atk == 35);

    assert(stats(p2).hp == 700);
    assert(stats(p2).atk == 70);

    assert(stats(p3).hp == int(350 * 3.5));
    assert(stats(p3).atk == int(35 * 3.5));

    assert(stats(p4).hp == int(350 * 5.5));
    assert(stats(p4).atk == int(35 * 5.5));
}

void test_damage_absorption() {
    GeoSlime geo(Owner::PlayerCtrl, 1);
    // starts with 200 shield and 450 HP
    assert(stats(geo).shield == 200);
    assert(stats(geo).hp == 450);

    // Deal 120 damage. Shield should absorb it all.
    take_damage(geo, 120);
    assert(stats(geo).shield == 80);
    assert(stats(geo).hp == 450);

    // Deal 100 damage. Shield absorbs 80, remaining 20 hits HP.
    take_damage(geo, 100);
    assert(stats(geo).shield == 0);
    assert(stats(geo).hp == 430);
}

int main() {
    std::cout << "Running test_unit..." << std::endl;
    test_slime_types_and_cpos();
    test_star_scaling();
    test_damage_absorption();
    std::cout << "test_unit passed!" << std::endl;
    return 0;
}
