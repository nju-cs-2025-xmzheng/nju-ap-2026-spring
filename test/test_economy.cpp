#include "engine/GameSession.hpp"
#include "unit/Unit.hpp"
#include <cassert>
#include <iostream>

using namespace Synera;
using namespace Synera::engine;
using namespace Synera::unit;

void test_gold_only_leveling() {
    GameSession session;
    session.player_.hp = 100;
    session.player_.gold = 15;
    session.player_.level = 1;

    // Level 1 -> 2 costs 1 * 5 + 5 = 10 gold.
    bool ok = session.buy_level();
    assert(ok);
    assert(session.player_.level == 2);
    assert(session.player_.gold == 5);

    // Level 2 -> 3 costs 2 * 5 + 5 = 15 gold. Player has 5 gold, should fail.
    ok = session.buy_level();
    assert(!ok);
    assert(session.player_.level == 2);
    assert(session.player_.gold == 5);

    // Add gold and buy again
    session.player_.gold += 10; // now 15
    ok = session.buy_level();
    assert(ok);
    assert(session.player_.level == 3);
    assert(session.player_.gold == 0);
}

void test_shop_refresh_and_rolling() {
    GameSession session;
    session.player_.level = 1;
    session.player_.gold = 10;

    // Manual refresh should cost 1 gold
    bool ok = session.refresh_shop(false);
    assert(ok);
    assert(session.player_.gold == 9);

    // At level 1, all shop slots should be 1-star and cost 2 gold
    for (int i = 0; i < 5; ++i) {
        assert(session.shop_[i].has_value());
        auto &[u, cost] = *session.shop_[i];
        assert(stats(u).level == 1);
        assert(cost == 2);
    }

    // Test rolling higher level stars at higher level
    session.player_.level = 5;
    bool has_two_star = false;
    bool has_three_star = false;
    // Do 100 free refreshes to check probabilities
    for (int r = 0; r < 100; ++r) {
        session.refresh_shop(true);
        for (int i = 0; i < 5; ++i) {
            assert(session.shop_[i].has_value());
            auto &[u, cost] = *session.shop_[i];
            int lvl = stats(u).level;
            if (lvl == 1) {
                assert(cost == 2);
            } else if (lvl == 2) {
                assert(cost == 5);
                has_two_star = true;
            } else if (lvl == 3) {
                assert(cost == 14);
                has_three_star = true;
            } else {
                assert(false && "rolled invalid star level");
            }
        }
    }
    assert(has_two_star && "No 2-star unit rolled at level 5 in 100 refreshes");
    assert(has_three_star &&
           "No 3-star unit rolled at level 5 in 100 refreshes");
}

void test_buying_units_and_bench_placement() {
    GameSession session;
    init_board(session.board_);
    session.player_.gold = 10;

    // Mock a 1-star Pyro Slime in shop slot 0
    session.shop_[0] = std::make_pair(PyroSlime(Owner::PlayerCtrl, 1), 2);

    bool ok = session.buy_unit(0);
    assert(ok);
    assert(session.player_.gold == 8);
    assert(!session.shop_[0].has_value());

    // First bench slot (LinearCoord{0}) should have the Pyro Slime
    auto bench_unit = get_unit(session.board_, LinearCoord{0});
    assert(bench_unit != nullptr);
    assert(name(*bench_unit) == "Pyro Slime");
    assert(stats(*bench_unit).level == 1);
    assert(stats(*bench_unit).hp == 350);

    // Try buying from empty slot
    ok = session.buy_unit(0);
    assert(!ok);

    // Fill the bench and verify limit
    for (int i = 1; i < 8; ++i) {
        set_unit(session.board_, LinearCoord{i},
                 std::make_shared<Unit>(HydroSlime(Owner::PlayerCtrl, 1)));
    }
    // Bench is full now (8 units). Trying to buy another one should fail even
    // with gold
    session.player_.gold = 10;
    session.shop_[1] = std::make_pair(PyroSlime(Owner::PlayerCtrl, 1), 2);
    ok = session.buy_unit(1);
    assert(!ok); // should fail because bench is full
    assert(session.player_.gold == 10);
}

void test_board_prioritized_merging() {
    // Scenario A: All on bench
    {
        GameSession session;
        init_board(session.board_);
        set_unit(session.board_, LinearCoord{0},
                 std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
        set_unit(session.board_, LinearCoord{1},
                 std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
        set_unit(session.board_, LinearCoord{2},
                 std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

        session.check_and_merge(LinearCoord{2});

        // The 2-star unit should be at LinearCoord{2} (most recently acquired)
        auto u2 = get_unit(session.board_, LinearCoord{2});
        assert(u2 != nullptr);
        assert(stats(*u2).level == 2);
        assert(stats(*u2).hp == 700); // 350 * 2.0
        assert(stats(*u2).atk == 70); // 35 * 2.0

        assert(get_unit(session.board_, LinearCoord{0}) == nullptr);
        assert(get_unit(session.board_, LinearCoord{1}) == nullptr);
    }

    // Scenario B: 1 on board, 2 on bench -> upgraded unit goes to board
    {
        GameSession session;
        init_board(session.board_);
        HexCoord board_coord{5, 5};
        set_unit(session.board_, board_coord,
                 std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
        set_unit(session.board_, LinearCoord{0},
                 std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
        set_unit(session.board_, LinearCoord{1},
                 std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

        session.check_and_merge(LinearCoord{1});

        // The upgraded unit must be on the board (HexCoord{5, 5})
        auto u_board = get_unit(session.board_, board_coord);
        assert(u_board != nullptr);
        assert(stats(*u_board).level == 2);

        assert(get_unit(session.board_, LinearCoord{0}) == nullptr);
        assert(get_unit(session.board_, LinearCoord{1}) == nullptr);
    }
}

void test_equipment_drops_on_merge() {
    GameSession session;
    init_board(session.board_);
    session.equip_pool_.clear();

    auto u1 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    auto u2 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    auto u3 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));

    stats(*u1).equipped = PyroDrop{};
    stats(*u2).equipped = HydroDrop{};
    // u3 has no equipment (monostate)

    set_unit(session.board_, LinearCoord{0}, u1);
    set_unit(session.board_, LinearCoord{1}, u2);
    set_unit(session.board_, LinearCoord{2}, u3);

    session.check_and_merge(LinearCoord{2});

    // Verify the 2-star unit has no equipment
    auto u_upgraded = get_unit(session.board_, LinearCoord{2});
    assert(u_upgraded != nullptr);
    assert(std::holds_alternative<std::monostate>(stats(*u_upgraded).equipped));

    // Verify the equipment was returned to the pool
    assert(session.equip_pool_.size() == 2);
    bool has_pyro = false, has_hydro = false;
    for (auto &eq : session.equip_pool_) {
        if (std::holds_alternative<PyroDrop>(eq))
            has_pyro = true;
        if (std::holds_alternative<HydroDrop>(eq))
            has_hydro = true;
    }
    assert(has_pyro);
    assert(has_hydro);
}

void test_nested_merging_and_max_star_limit() {
    GameSession session;
    init_board(session.board_);

    // Place two 2-star Pyro Slimes on bench
    set_unit(session.board_, LinearCoord{0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 2)));
    set_unit(session.board_, LinearCoord{1},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 2)));

    // Place three 1-star Pyro Slimes on bench
    set_unit(session.board_, LinearCoord{2},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
    set_unit(session.board_, LinearCoord{3},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));
    set_unit(session.board_, LinearCoord{4},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1)));

    // This should merge three 1-stars at 2,3,4 into a 2-star at 4,
    // which then merges with 2-stars at 0, 1, 4 into a 3-star at 4.
    session.check_and_merge(LinearCoord{4});

    auto u3 = get_unit(session.board_, LinearCoord{4});
    assert(u3 != nullptr);
    assert(stats(*u3).level == 3);
    assert(stats(*u3).hp == int(350 * 3.5)); // 3.5x scaling
    assert(stats(*u3).atk == int(35 * 3.5));

    assert(get_unit(session.board_, LinearCoord{0}) == nullptr);
    assert(get_unit(session.board_, LinearCoord{1}) == nullptr);
    assert(get_unit(session.board_, LinearCoord{2}) == nullptr);
    assert(get_unit(session.board_, LinearCoord{3}) == nullptr);

    // Now test 4-star creation
    // Clear board and place two 3-stars
    init_board(session.board_);
    set_unit(session.board_, LinearCoord{0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 3)));
    set_unit(session.board_, LinearCoord{1},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 3)));
    set_unit(session.board_, LinearCoord{2},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 3)));

    session.check_and_merge(LinearCoord{2});

    auto u4 = get_unit(session.board_, LinearCoord{2});
    assert(u4 != nullptr);
    assert(stats(*u4).level == 4);
    assert(stats(*u4).hp == int(350 * 5.5)); // 5.5x scaling
    assert(stats(*u4).atk == int(35 * 5.5));

    // Now test that 4-stars do NOT merge further
    init_board(session.board_);
    set_unit(session.board_, LinearCoord{0},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 4)));
    set_unit(session.board_, LinearCoord{1},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 4)));
    set_unit(session.board_, LinearCoord{2},
             std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 4)));

    session.check_and_merge(LinearCoord{2});

    // None of the 4-stars should be deleted or upgraded
    assert(get_unit(session.board_, LinearCoord{0}) != nullptr);
    assert(get_unit(session.board_, LinearCoord{1}) != nullptr);
    assert(get_unit(session.board_, LinearCoord{2}) != nullptr);
    assert(stats(*get_unit(session.board_, LinearCoord{2})).level == 4);
}

int main() {
    std::cout << "[Test] Running test_gold_only_leveling..." << std::endl;
    test_gold_only_leveling();

    std::cout << "[Test] Running test_shop_refresh_and_rolling..." << std::endl;
    test_shop_refresh_and_rolling();

    std::cout << "[Test] Running test_buying_units_and_bench_placement..."
              << std::endl;
    test_buying_units_and_bench_placement();

    std::cout << "[Test] Running test_board_prioritized_merging..."
              << std::endl;
    test_board_prioritized_merging();

    std::cout << "[Test] Running test_equipment_drops_on_merge..." << std::endl;
    test_equipment_drops_on_merge();

    std::cout << "[Test] Running test_nested_merging_and_max_star_limit..."
              << std::endl;
    test_nested_merging_and_max_star_limit();

    std::cout << "[Test] All test cases passed!" << std::endl;
    return 0;
}
