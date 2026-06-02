#include "engine/Board.hpp"
#include "unit/Unit.hpp"
#include <cassert>
#include <iostream>
#include <memory>

using namespace Synera::engine;
using namespace Synera::unit;

void test_board_placement() {
    Board board;
    init_board(board);

    auto u = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));

    // Initially unoccupied
    assert(!is_occupied(board, HexCoord{4, 4}));
    assert(get_unit(board, HexCoord{4, 4}) == nullptr);

    // Place unit
    bool set_ok = set_unit(board, HexCoord{4, 4}, u);
    assert(set_ok);
    assert(is_occupied(board, HexCoord{4, 4}));
    assert(get_unit(board, HexCoord{4, 4}) == u);

    // Count player units
    assert(count_player_units_on_board(board) == 1);

    // Remove unit
    remove_unit(board, HexCoord{4, 4});
    assert(!is_occupied(board, HexCoord{4, 4}));
    assert(get_unit(board, HexCoord{4, 4}) == nullptr);
    assert(count_player_units_on_board(board) == 0);
}

void test_board_territories() {
    // Player territory is rows 4 to 7
    assert(Board::is_player_territory(HexCoord{4, 0}));
    assert(Board::is_player_territory(HexCoord{7, 7}));
    assert(!Board::is_player_territory(HexCoord{3, 0}));

    // Enemy territory is rows 0 to 3
    assert(Board::is_enemy_territory(HexCoord{0, 0}));
    assert(Board::is_enemy_territory(HexCoord{3, 7}));
    assert(!Board::is_enemy_territory(HexCoord{4, 0}));
}

void test_board_move() {
    Board board;
    init_board(board);

    auto u1 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(board, LinearCoord{0}, u1);

    // Move from bench to player territory (row >= 4) with limit 3
    bool move_ok = move_unit(board, LinearCoord{0}, HexCoord{5, 5}, 3);
    assert(move_ok);
    assert(!is_occupied(board, LinearCoord{0}));
    assert(is_occupied(board, HexCoord{5, 5}));
    assert(get_unit(board, HexCoord{5, 5}) == u1);

    // Try moving to enemy territory (should fail for player unit)
    move_ok = move_unit(board, HexCoord{5, 5}, HexCoord{2, 2}, 3);
    assert(!move_ok);
    assert(is_occupied(board, HexCoord{5, 5}));

    // Limit check: set count to limit
    auto u2 = std::make_shared<Unit>(PyroSlime(Owner::PlayerCtrl, 1));
    set_unit(board, LinearCoord{1}, u2);
    // Limit is 1, but we already have 1 unit on board. Moving u2 to board
    // should fail.
    move_ok = move_unit(board, LinearCoord{1}, HexCoord{6, 6}, 1);
    assert(!move_ok);
    assert(is_occupied(board, LinearCoord{1}));
}

int main() {
    std::cout << "Running test_board..." << std::endl;
    test_board_placement();
    test_board_territories();
    test_board_move();
    std::cout << "test_board passed!" << std::endl;
    return 0;
}
