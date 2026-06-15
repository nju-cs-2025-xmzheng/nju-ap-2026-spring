#include "engine/Coord.hpp"
#include <cassert>
#include <iostream>

using namespace Synera::engine;

void test_linear_coord() {
    LinearCoord c1{2};
    LinearCoord c2{5};
    LinearCoord c_out{-1};
    LinearCoord c_out2{8};

    // 1. in_range
    assert(in_range(c1));
    assert(in_range(c2));
    assert(!in_range(c_out));
    assert(!in_range(c_out2));

    // 2. distance
    assert(distance(c1, c2) == 3);
    assert(distance(c2, c1) == 3);
    assert(distance(c1, c1) == 0);

    // 3. neighbor
    auto neighbors = neighbor(c1);
    assert(neighbors.size() == 2);
    assert(neighbors[0].x == 1);
    assert(neighbors[1].x == 3);
}

void test_hex_coord() {
    HexCoord h1{3, 3};
    HexCoord h2{3, 4};
    HexCoord h_out{-1, 0};
    HexCoord h_out2{8, 0};

    // 1. in_range
    assert(in_range(h1));
    assert(in_range(h2));
    assert(!in_range(h_out));
    assert(!in_range(h_out2));

    // 2. distance
    assert(distance(h1, h2) == 1);
    HexCoord h3{5, 3};
    assert(distance(h1, h3) == 2);

    // 3. neighbor
    auto neighbors = neighbor(h1);
    // Neighbors of (3,3) in hexagonal grid
    // shift = 3 & 1 = 1
    // neighbors list: (3, 2), (3, 4), (2, 2), (2, 3), (4, 2), (4, 3)
    assert(neighbors.size() == 6);
}

int main() {
    std::cout << "Running test_coord..." << std::endl;
    test_linear_coord();
    test_hex_coord();
    std::cout << "test_coord passed!" << std::endl;
    return 0;
}
