#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "game/ai/Pathfinding.h"

using Catch::Approx;
using namespace game;

namespace {

/// Every consecutive pair in a path must be adjacent and walkable. A path that
/// merely starts and ends in the right place can still be nonsense.
bool isContiguousAndWalkable(const NavGrid& grid, const std::vector<GridPoint>& path) {
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (!grid.walkable(path[i].x, path[i].y)) return false;
        if (i == 0) continue;
        const int dx = std::abs(path[i].x - path[i - 1].x);
        const int dy = std::abs(path[i].y - path[i - 1].y);
        if (dx > 1 || dy > 1 || (dx + dy) == 0) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("A* finds optimal paths on an open grid", "[pathfinding]") {
    NavGrid grid(20, 20);
    Pathfinder pathfinder;
    std::vector<GridPoint> path;

    SECTION("straight horizontal run costs one per step") {
        REQUIRE(pathfinder.findPath(grid, {0, 0}, {9, 0}, path));
        REQUIRE(pathfinder.stats().cost == Approx(9.0f).margin(0.001f));
        REQUIRE(path.size() == 10);
    }

    SECTION("pure diagonal costs sqrt(2) per step") {
        // If this comes out as 10 the heuristic is treating diagonals as free
        // and A* has stopped returning shortest paths.
        REQUIRE(pathfinder.findPath(grid, {0, 0}, {5, 5}, path));
        REQUIRE(pathfinder.stats().cost == Approx(5.0f * 1.41421356f).margin(0.001f));
    }

    SECTION("mixed move takes the diagonal shortcut, not the L") {
        REQUIRE(pathfinder.findPath(grid, {0, 0}, {8, 3}, path));
        // Octile optimum: 3 diagonals + 5 straights.
        REQUIRE(pathfinder.stats().cost ==
                Approx(3.0f * 1.41421356f + 5.0f).margin(0.001f));
    }

    SECTION("start equal to goal is a single-node path") {
        REQUIRE(pathfinder.findPath(grid, {4, 4}, {4, 4}, path));
        REQUIRE(path.size() == 1);
    }

    SECTION("4-connected mode falls back to Manhattan distance") {
        PathOptions options;
        options.allowDiagonal = false;
        REQUIRE(pathfinder.findPath(grid, {0, 0}, {5, 5}, path, options));
        REQUIRE(pathfinder.stats().cost == Approx(10.0f).margin(0.001f));
    }
}

TEST_CASE("A* navigates a maze that defeats a greedy walk", "[pathfinding]") {
    // The only way south alternates between the far-left and far-right gaps, so
    // anything that just steps toward the goal gets stuck immediately.
    const NavGrid grid = NavGrid::fromRows({
        "..........",
        "#########.",
        "..........",
        ".#########",
        "..........",
        "#########.",
        "..........",
        ".#########",
        "..........",
        ".........."});

    Pathfinder pathfinder;
    std::vector<GridPoint> path;

    REQUIRE(pathfinder.findPath(grid, {0, 0}, {9, 9}, path));
    REQUIRE(path.front() == GridPoint{0, 0});
    REQUIRE(path.back() == GridPoint{9, 9});
    REQUIRE(isContiguousAndWalkable(grid, path));

    SECTION("the heuristic keeps the search far below brute force") {
        // 100 cells in the grid. Expanding anywhere near all of them would mean
        // the heuristic is doing nothing and this is really Dijkstra.
        REQUIRE(pathfinder.stats().nodesExpanded < 100);
    }
}

TEST_CASE("A* reports failure rather than an approximate path", "[pathfinding]") {
    Pathfinder pathfinder;
    std::vector<GridPoint> path;

    SECTION("a full-height wall makes the goal unreachable") {
        const NavGrid grid = NavGrid::fromRows({"..#..", "..#..", "..#..", "..#..", "..#.."});
        REQUIRE_FALSE(pathfinder.findPath(grid, {0, 0}, {4, 4}, path));
        REQUIRE(path.empty());
    }

    SECTION("a blocked start fails immediately") {
        const NavGrid grid = NavGrid::fromRows({"#....", ".....", "....."});
        REQUIRE_FALSE(pathfinder.findPath(grid, {0, 0}, {4, 2}, path));
    }

    SECTION("a blocked goal fails immediately") {
        const NavGrid grid = NavGrid::fromRows({".....", ".....", "....#"});
        REQUIRE_FALSE(pathfinder.findPath(grid, {0, 0}, {4, 2}, path));
    }

    SECTION("out-of-bounds coordinates fail rather than crash") {
        NavGrid grid(5, 5);
        REQUIRE_FALSE(pathfinder.findPath(grid, {-1, 0}, {4, 4}, path));
        REQUIRE_FALSE(pathfinder.findPath(grid, {0, 0}, {99, 99}, path));
    }

    SECTION("the node budget bounds the search") {
        NavGrid grid(200, 200);
        for (int y = 0; y < 200; ++y) grid.setBlocked(100, y, true);  // sealed halves
        PathOptions options;
        options.maxNodes = 500;
        REQUIRE_FALSE(pathfinder.findPath(grid, {0, 0}, {199, 199}, path, options));
        REQUIRE(pathfinder.stats().nodesExpanded <= 500);
    }
}

TEST_CASE("diagonal moves do not cut wall corners", "[pathfinding]") {
    // (2,0) and (2,1) are wall; (0,2) and (1,2) are wall. The only way from the
    // top-left region to the right side is diagonally between two corners.
    const NavGrid grid = NavGrid::fromRows({"..#.", "..#.", "##..", "...."});

    Pathfinder pathfinder;
    std::vector<GridPoint> path;

    PathOptions strict;
    strict.preventCornerCutting = true;
    PathOptions loose;
    loose.preventCornerCutting = false;

    REQUIRE_FALSE(pathfinder.findPath(grid, {0, 0}, {3, 0}, path, strict));
    REQUIRE(pathfinder.findPath(grid, {0, 0}, {3, 0}, path, loose));
}

TEST_CASE("path simplification preserves validity", "[pathfinding]") {
    const NavGrid grid = NavGrid::fromRows({
        "..........",
        "#########.",
        "..........",
        ".#########",
        "..........",
        "..........",
        "..........",
        "..........",
        "..........",
        ".........."});

    Pathfinder pathfinder;
    std::vector<GridPoint> path;
    REQUIRE(pathfinder.findPath(grid, {0, 0}, {9, 9}, path));

    const std::size_t before = path.size();
    std::vector<GridPoint> smoothed = path;
    Pathfinder::simplify(grid, smoothed);

    REQUIRE(smoothed.size() <= before);
    REQUIRE(smoothed.front() == path.front());
    REQUIRE(smoothed.back() == path.back());

    // Every retained hop must still be a clear straight line, or the enemy will
    // walk through a wall on its way to the next waypoint.
    for (std::size_t i = 1; i < smoothed.size(); ++i) {
        REQUIRE(Pathfinder::lineOfSight(grid, smoothed[i - 1], smoothed[i]));
    }

    SECTION("a straight open path collapses to its endpoints") {
        NavGrid open(20, 20);
        std::vector<GridPoint> straight;
        REQUIRE(pathfinder.findPath(open, {0, 0}, {15, 0}, straight));
        Pathfinder::simplify(open, straight);
        REQUIRE(straight.size() == 2);
    }
}

TEST_CASE("line of sight respects walls", "[pathfinding]") {
    const NavGrid grid = NavGrid::fromRows({".....", ".....", "..#..", ".....", "....."});

    REQUIRE(Pathfinder::lineOfSight(grid, {0, 0}, {4, 0}));
    REQUIRE_FALSE(Pathfinder::lineOfSight(grid, {0, 2}, {4, 2}));
    REQUIRE(Pathfinder::lineOfSight(grid, {0, 4}, {4, 4}));
    REQUIRE(Pathfinder::lineOfSight(grid, {2, 2}, {2, 2}) == false);  // stands in a wall
}

TEST_CASE("the pathfinder can be reused without stale state", "[pathfinding]") {
    // The search-ID stamping scheme avoids clearing the score arrays between
    // calls. If the stamping is wrong, the second search inherits the first
    // one's g-scores and silently returns a bad path.
    NavGrid grid(30, 30);
    Pathfinder pathfinder;
    std::vector<GridPoint> first;
    std::vector<GridPoint> second;

    REQUIRE(pathfinder.findPath(grid, {0, 0}, {29, 29}, first));
    const float firstCost = pathfinder.stats().cost;

    for (int i = 0; i < 50; ++i) {
        REQUIRE(pathfinder.findPath(grid, {5, 5}, {20, 12}, second));
    }
    REQUIRE(pathfinder.findPath(grid, {0, 0}, {29, 29}, first));

    REQUIRE(pathfinder.stats().cost == Approx(firstCost).margin(0.001f));
}
