#include "engine/Board.hpp"
#include "unit/Synergy.hpp"
#include "unit/Unit.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <iostream>
#include <memory>

using namespace Synera::engine;
using namespace Synera::unit;

void test_compute_synergies() {
    Board board;
    init_board(board);

    // Place 2 Pyro Slimes
    auto p1 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl));
    auto p2 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl));
    set_unit(board, HexCoord{4, 0}, p1);
    set_unit(board, HexCoord{4, 1}, p2);

    auto active = compute_synergies(board);
    assert(active.fervent_flames);
    assert(!active.soothing_water);
    assert(!active.protective_canopy);

    // Place 2 Hydro Slimes
    auto h1 = std::make_shared<Unit>(HydroSlime(Owner::PlayerCtrl));
    auto h2 = std::make_shared<Unit>(HydroSlime(Owner::PlayerCtrl));
    set_unit(board, HexCoord{5, 0}, h1);
    set_unit(board, HexCoord{5, 1}, h2);

    active = compute_synergies(board);
    assert(active.fervent_flames);
    assert(active.soothing_water);
    // Right now we have Pyro (2) and Hydro (2). That is 2 unique elements. So
    // protective_canopy should be false.
    assert(!active.protective_canopy);

    // Let's add Anemo and Geo to get 4 unique elements
    auto a1 = std::make_shared<Unit>(AnemoSlime(Owner::PlayerCtrl));
    auto g1 = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl));
    set_unit(board, HexCoord{6, 0}, a1);
    set_unit(board, HexCoord{6, 1}, g1);

    active = compute_synergies(board);
    assert(
        active
            .protective_canopy); // Pyro, Hydro, Anemo, Geo. 4 unique elements!
}

void test_apply_combat_synergies() {
    // 1. Fervent Flames: ATK +25%
    {
        Board board;
        init_board(board);
        auto p1 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl));
        auto p2 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl));
        set_unit(board, HexCoord{4, 0}, p1);
        set_unit(board, HexCoord{4, 1}, p2);

        int base_atk = stats(*p1).atk;
        auto active = compute_synergies(board);
        apply_combat_synergies(board, active);
        assert(stats(*p1).atk == int(base_atk * 1.25));
    }

    // 2. Soothing Water: HP/MaxHP +25%
    {
        Board board;
        init_board(board);
        auto h1 = std::make_shared<Unit>(HydroSlime(Owner::PlayerCtrl));
        auto h2 = std::make_shared<Unit>(HydroSlime(Owner::PlayerCtrl));
        set_unit(board, HexCoord{4, 0}, h1);
        set_unit(board, HexCoord{4, 1}, h2);

        int base_hp = stats(*h1).hp;
        int base_max_hp = stats(*h1).max_hp;
        auto active = compute_synergies(board);
        apply_combat_synergies(board, active);
        assert(stats(*h1).hp == int(base_hp * 1.25));
        assert(stats(*h1).max_hp == int(base_max_hp * 1.25));
    }

    // 3. Impetuous Winds: Max Mana -15
    {
        Board board;
        init_board(board);
        auto a1 = std::make_shared<Unit>(AnemoSlime(Owner::PlayerCtrl));
        auto a2 = std::make_shared<Unit>(AnemoSlime(Owner::PlayerCtrl));
        set_unit(board, HexCoord{4, 0}, a1);
        set_unit(board, HexCoord{4, 1}, a2);

        int base_max_mana = stats(*a1).max_mana;
        auto active = compute_synergies(board);
        apply_combat_synergies(board, active);
        assert(stats(*a1).max_mana == base_max_mana - 15);
    }

    // 4. Enduring Rock: Shield +15%
    {
        Board board;
        init_board(board);
        auto g1 = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl));
        auto g2 = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl));
        set_unit(board, HexCoord{4, 0}, g1);
        set_unit(board, HexCoord{4, 1}, g2);

        int base_shield = stats(*g1).shield;
        auto active = compute_synergies(board);
        apply_combat_synergies(board, active);
        assert(stats(*g1).shield == int(base_shield * 1.15));
    }
}

void test_deal_damage_synergies() {
    // Enduring Rock: shielded player unit deals +15% damage
    {
        Board board;
        init_board(board);
        auto player_geo1 = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl));
        auto player_geo2 = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl));
        auto enemy = std::make_shared<Unit>(PyroSlime(Owner::EnemyCtrl));

        set_unit(board, HexCoord{4, 0}, player_geo1);
        set_unit(board, HexCoord{4, 1}, player_geo2);
        set_unit(board, HexCoord{0, 0}, enemy);

        int enemy_base_hp = stats(*enemy).hp;
        deal_damage(board, *player_geo1, *enemy, 100);
        // Note: 100 * 1.15 is 114.99999999999999 in double representation,
        // which truncates to 114 when cast to int.
        assert(stats(*enemy).hp == enemy_base_hp - 114);
    }

    // Enemy owner uses the same resonance rules in multiplayer combat.
    {
        Board board;
        init_board(board);
        auto enemy_geo1 = std::make_shared<Unit>(GeoSlime(Owner::EnemyCtrl));
        auto enemy_geo2 = std::make_shared<Unit>(GeoSlime(Owner::EnemyCtrl));
        auto player = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl));

        set_unit(board, HexCoord{0, 0}, enemy_geo1);
        set_unit(board, HexCoord{0, 1}, enemy_geo2);
        set_unit(board, HexCoord{4, 0}, player);

        auto active = compute_synergies(board, Owner::EnemyCtrl);
        assert(active.enduring_rock);

        int player_base_hp = stats(*player).hp;
        deal_damage(board, *enemy_geo1, *player, 100);
        assert(stats(*player).hp == player_base_hp - 114);
    }

    // Protective Canopy: player units take 15% less damage
    {
        Board board;
        init_board(board);
        auto p1 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl));
        auto h1 = std::make_shared<Unit>(HydroSlime(Owner::PlayerCtrl));
        auto a1 = std::make_shared<Unit>(AnemoSlime(Owner::PlayerCtrl));
        auto g1 = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl));
        auto enemy = std::make_shared<Unit>(PyroSlime(Owner::EnemyCtrl));

        set_unit(board, HexCoord{4, 0}, p1);
        set_unit(board, HexCoord{4, 1}, h1);
        set_unit(board, HexCoord{5, 0}, a1);
        set_unit(board, HexCoord{5, 1}, g1);
        set_unit(board, HexCoord{0, 0}, enemy);

        int player_base_hp = stats(*p1).hp;
        // Base damage 100. Because protective_canopy is active (4 unique
        // elements), player unit p1 takes 85 damage.
        deal_damage(board, *enemy, *p1, 100);
        assert(stats(*p1).hp == player_base_hp - 85);
    }
}

int main() {
    std::cout << "Running test_synergy..." << std::endl;
    test_compute_synergies();
    test_apply_combat_synergies();
    test_deal_damage_synergies();
    std::cout << "test_synergy passed!" << std::endl;
    return 0;
}
