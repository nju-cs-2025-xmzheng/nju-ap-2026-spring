#include "unit/Equipment.hpp"
#include "unit/Unit.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <iostream>

using namespace Synera::unit;

void test_equipment_names() {
    assert(name(Equipment(std::monostate{})) == "None");
    assert(name(Equipment(PyroDrop{})) == "Pyro Drop");
    assert(name(Equipment(HydroDrop{})) == "Hydro Drop");
    assert(name(Equipment(AnemoDrop{})) == "Anemo Drop");
    assert(name(Equipment(GeoDrop{})) == "Geo Drop");
    assert(name(Equipment(ElectroDrop{})) == "Electro Drop");
    assert(name(Equipment(CryoDrop{})) == "Cryo Drop");
}

void test_apply_equipment() {
    // 1. PyroDrop: ATK +15
    {
        Unit u = PyroSlime(Owner::PlayerCtrl, 1);
        int base_atk = stats(u).atk;
        equip(u, PyroDrop{});
        assert(stats(u).atk == base_atk + 15);
        assert(std::holds_alternative<PyroDrop>(stats(u).equipped));
    }

    // 2. HydroDrop: Max HP +150, HP +150
    {
        Unit u = HydroSlime(Owner::PlayerCtrl, 1);
        int base_hp = stats(u).hp;
        int base_max_hp = stats(u).max_hp;
        equip(u, HydroDrop{});
        assert(stats(u).hp == base_hp + 150);
        assert(stats(u).max_hp == base_max_hp + 150);
        assert(std::holds_alternative<HydroDrop>(stats(u).equipped));
    }

    // 3. AnemoDrop: Max Mana -30
    {
        Unit u = AnemoSlime(Owner::PlayerCtrl, 1);
        int base_max_mana = stats(u).max_mana;
        equip(u, AnemoDrop{});
        assert(stats(u).max_mana == base_max_mana - 30);
        assert(std::holds_alternative<AnemoDrop>(stats(u).equipped));
    }

    // 4. GeoDrop: Shield +100
    {
        Unit u = GeoSlime(Owner::PlayerCtrl, 1);
        int base_shield = stats(u).shield;
        equip(u, GeoDrop{});
        assert(stats(u).shield == base_shield + 100);
        assert(std::holds_alternative<GeoDrop>(stats(u).equipped));
    }
}

int main() {
    std::cout << "Running test_equipment..." << std::endl;
    test_equipment_names();
    test_apply_equipment();
    std::cout << "test_equipment passed!" << std::endl;
    return 0;
}
