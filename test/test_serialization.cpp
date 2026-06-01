#include "common/Serialization.hpp"
#include "engine/GameSession.hpp"
#include "unit/Unit.hpp"
#include "unit/UnitImpl.hpp" // IWYU pragma: keep
#include <cassert>
#include <iostream>
#include <memory>

using namespace Synera;
using namespace Synera::engine;
using namespace Synera::unit;

void test_serialization() {
    GameSession session_save;
    init_board(session_save.board_);

    // 1. Populate Player
    session_save.player_.hp = 87;
    session_save.player_.gold = 42;
    session_save.player_.level = 4;

    // 2. Populate Shop
    session_save.shop_.fill(std::nullopt);
    session_save.shop_[2] = std::make_pair(PyroSlime(Owner::PlayerCtrl, 2), 5);
    session_save.shop_[4] = std::make_pair(HydroSlime(Owner::PlayerCtrl, 1), 2);

    // 3. Populate Equipment Pool
    session_save.equip_pool_.clear();
    session_save.equip_pool_.push_back(PyroDrop{});
    session_save.equip_pool_.push_back(HydroDrop{});

    // 4. Populate Board
    auto u_board = std::make_shared<Unit>(GeoSlime(Owner::PlayerCtrl, 2));
    // Equip Geo Slime with GeoDrop (increases shield by 100)
    equip(*u_board, GeoDrop{});
    // Set some custom FSM fields
    stats(*u_board).hp = 444;
    stats(*u_board).mana = 25;
    stats(*u_board).stun_ticks = 12;
    set_unit(session_save.board_, HexCoord{4, 4}, u_board);

    // 5. Populate Bench
    auto u_bench = std::make_shared<Unit>(ElectroSlime(Owner::EnemyCtrl, 1));
    set_unit(session_save.board_, LinearCoord{3}, u_bench);

    // 6. Save
    std::string filename = "serialization_test_save.txt";
    bool ok = save(session_save, filename);
    assert(ok);

    // 7. Load into a fresh session
    GameSession session_load;
    ok = load(session_load, filename);
    assert(ok);

    // 8. Assert Player
    assert(session_load.player_.hp == 87);
    assert(session_load.player_.gold == 42);
    assert(session_load.player_.level == 4);

    // 9. Assert Shop
    assert(!session_load.shop_[0].has_value());
    assert(session_load.shop_[2].has_value());
    assert(name(session_load.shop_[2]->first) == "Pyro Slime");
    assert(stats(session_load.shop_[2]->first).level == 2);
    assert(session_load.shop_[2]->second == 5);
    assert(session_load.shop_[4].has_value());
    assert(name(session_load.shop_[4]->first) == "Hydro Slime");
    assert(stats(session_load.shop_[4]->first).level == 1);
    assert(session_load.shop_[4]->second == 2);

    // 10. Assert Equipment Pool
    assert(session_load.equip_pool_.size() == 2);
    assert(std::holds_alternative<PyroDrop>(session_load.equip_pool_[0]));
    assert(std::holds_alternative<HydroDrop>(session_load.equip_pool_[1]));

    // 11. Assert Board Unit
    auto u_board_loaded = get_unit(session_load.board_, HexCoord{4, 4});
    assert(u_board_loaded != nullptr);
    assert(name(*u_board_loaded) == "Geo Slime");
    assert(stats(*u_board_loaded).level == 2);
    assert(stats(*u_board_loaded).hp == 444);
    assert(stats(*u_board_loaded).mana == 25);
    assert(stats(*u_board_loaded).stun_ticks == 12);
    // Base Geo Slime level 2 shield = 200 * 2.0 = 400. GeoDrop adds 100. Total
    // = 500.
    assert(stats(*u_board_loaded).shield == 500);
    assert(std::holds_alternative<GeoDrop>(stats(*u_board_loaded).equipped));

    // 12. Assert Bench Unit
    auto u_bench_loaded = get_unit(session_load.board_, LinearCoord{3});
    assert(u_bench_loaded != nullptr);
    assert(name(*u_bench_loaded) == "Electro Slime");
    assert(stats(*u_bench_loaded).owner == Owner::EnemyCtrl);
    assert(stats(*u_bench_loaded).level == 1);

    std::cout << "Serialization test case passed!" << std::endl;
}

int main() {
    std::cout << "Running test_serialization..." << std::endl;
    test_serialization();
    std::cout << "test_serialization passed!" << std::endl;
    return 0;
}
