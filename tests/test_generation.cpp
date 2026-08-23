#include <catch2/catch_test_macros.hpp>

#include "game/ai/Pathfinding.h"
#include "game/generation/DungeonGenerator.h"

using namespace game;

namespace {

GenerationParams paramsFor(GenerationStyle style, std::uint32_t seed) {
    GenerationParams params;
    params.style = style;
    params.seed = seed;
    params.width = 96;
    params.height = 64;
    return params;
}

}  // namespace

TEST_CASE("generated dungeons are always fully connected", "[generation]") {
    // The headline correctness property. A dungeon with a walled-off treasure
    // room is not a quirk -- to the player it is simply a broken game -- so this
    // sweeps a wide range of seeds rather than spot-checking one.
    constexpr std::uint32_t kSeedCount = 120;

    SECTION("BSP") {
        for (std::uint32_t seed = 1; seed <= kSeedCount; ++seed) {
            DungeonGenerator generator;
            const Dungeon dungeon = generator.generate(paramsFor(GenerationStyle::BSP, seed));
            INFO("BSP seed " << seed);
            REQUIRE(dungeon.isFullyConnected());
        }
    }

    SECTION("cellular automata") {
        for (std::uint32_t seed = 1; seed <= kSeedCount; ++seed) {
            DungeonGenerator generator;
            const Dungeon dungeon =
                generator.generate(paramsFor(GenerationStyle::CellularAutomata, seed));
            INFO("cave seed " << seed);
            REQUIRE(dungeon.isFullyConnected());
        }
    }
}

TEST_CASE("the exit is always reachable from the entrance", "[generation]") {
    Pathfinder pathfinder;
    std::vector<GridPoint> path;

    for (std::uint32_t seed = 1; seed <= 60; ++seed) {
        for (GenerationStyle style : {GenerationStyle::BSP, GenerationStyle::CellularAutomata}) {
            DungeonGenerator generator;
            const Dungeon dungeon = generator.generate(paramsFor(style, seed));
            INFO("seed " << seed << " style " << static_cast<int>(style));

            REQUIRE(dungeon.isFloor(dungeon.entrance().x, dungeon.entrance().y));
            REQUIRE(dungeon.isFloor(dungeon.exit().x, dungeon.exit().y));

            // Connectivity is checked with a 4-connected flood fill; this
            // confirms the 8-way pathfinder the enemies actually use agrees.
            const NavGrid navGrid = dungeon.buildNavGrid();
            REQUIRE(pathfinder.findPath(navGrid, dungeon.entrance(), dungeon.exit(), path));
        }
    }
}

TEST_CASE("generation is deterministic for a given seed", "[generation]") {
    // Without this, a bug report of "the dungeon on floor 3 had no exit" is
    // unreproducible and therefore unfixable.
    for (GenerationStyle style : {GenerationStyle::BSP, GenerationStyle::CellularAutomata}) {
        DungeonGenerator generatorA;
        DungeonGenerator generatorB;
        const Dungeon a = generatorA.generate(paramsFor(style, 4242));
        const Dungeon b = generatorB.generate(paramsFor(style, 4242));

        REQUIRE(a.floorTileCount() == b.floorTileCount());
        REQUIRE(a.rooms().size() == b.rooms().size());
        REQUIRE(a.entrance() == b.entrance());
        REQUIRE(a.exit() == b.exit());

        for (int y = 0; y < a.heightInTiles(); ++y) {
            for (int x = 0; x < a.widthInTiles(); ++x) {
                REQUIRE(a.at(x, y) == b.at(x, y));
            }
        }
    }

    SECTION("different seeds produce different layouts") {
        DungeonGenerator generator;
        const Dungeon a = generator.generate(paramsFor(GenerationStyle::BSP, 1));
        const Dungeon b = generator.generate(paramsFor(GenerationStyle::BSP, 2));

        int differences = 0;
        for (int y = 0; y < a.heightInTiles(); ++y) {
            for (int x = 0; x < a.widthInTiles(); ++x) {
                if (a.at(x, y) != b.at(x, y)) ++differences;
            }
        }
        REQUIRE(differences > 100);
    }
}

TEST_CASE("generated floors are actually playable", "[generation]") {
    for (std::uint32_t seed = 1; seed <= 40; ++seed) {
        for (GenerationStyle style : {GenerationStyle::BSP, GenerationStyle::CellularAutomata}) {
            DungeonGenerator generator;
            const Dungeon dungeon = generator.generate(paramsFor(style, seed));
            INFO("seed " << seed << " style " << static_cast<int>(style));

            // Enough open space to be a level rather than a corridor.
            REQUIRE(dungeon.floorTileCount() > 400);
            REQUIRE(dungeon.rooms().size() >= 2);

            // The border must be sealed, or entities walk off the map.
            for (int x = 0; x < dungeon.widthInTiles(); ++x) {
                REQUIRE(dungeon.isSolid(x, 0));
                REQUIRE(dungeon.isSolid(x, dungeon.heightInTiles() - 1));
            }
            for (int y = 0; y < dungeon.heightInTiles(); ++y) {
                REQUIRE(dungeon.isSolid(0, y));
                REQUIRE(dungeon.isSolid(dungeon.widthInTiles() - 1, y));
            }

            // The exit should not be sitting on top of the entrance.
            const int dx = dungeon.exit().x - dungeon.entrance().x;
            const int dy = dungeon.exit().y - dungeon.entrance().y;
            REQUIRE(dx * dx + dy * dy > 100);
        }
    }
}

TEST_CASE("spawn candidates are safe places to put things", "[generation]") {
    DungeonGenerator generator;
    const Dungeon dungeon = generator.generate(paramsFor(GenerationStyle::BSP, 99));
    const std::vector<GridPoint> candidates = generator.spawnCandidates(dungeon, 8);

    REQUIRE(candidates.size() > 20);

    std::vector<std::uint8_t> visited;
    dungeon.reachableFrom(dungeon.entrance(), &visited);

    for (const GridPoint& p : candidates) {
        INFO("candidate " << p.x << "," << p.y);
        REQUIRE(dungeon.isFloor(p.x, p.y));
        // Never on the stairs -- an item there would be invisible under the exit.
        REQUIRE(dungeon.at(p.x, p.y) == Tile::Floor);
        // Reachable, or the player can never collect it.
        REQUIRE(visited[static_cast<std::size_t>(p.y) * dungeon.widthInTiles() + p.x]);
        // Clear of the entrance, so the player is not ambushed on arrival.
        const int dx = p.x - dungeon.entrance().x;
        const int dy = p.y - dungeon.entrance().y;
        REQUIRE(dx * dx + dy * dy >= 64);
    }

    SECTION("candidates are ordered by distance from the entrance") {
        // Lets the caller put nastier things deeper into the floor.
        long long previous = -1;
        for (const GridPoint& p : candidates) {
            const long long dx = p.x - dungeon.entrance().x;
            const long long dy = p.y - dungeon.entrance().y;
            const long long distance = dx * dx + dy * dy;
            REQUIRE(distance >= previous);
            previous = distance;
        }
    }
}

TEST_CASE("reachability counting is correct on a hand-built map", "[generation]") {
    Dungeon dungeon(10, 10, 32.0f);
    // One 3x3 room...
    for (int y = 1; y <= 3; ++y) {
        for (int x = 1; x <= 3; ++x) dungeon.set(x, y, Tile::Floor);
    }
    // ...and a second, deliberately sealed off from the first.
    for (int y = 6; y <= 8; ++y) {
        for (int x = 6; x <= 8; ++x) dungeon.set(x, y, Tile::Floor);
    }
    dungeon.setEntrance({1, 1});

    REQUIRE(dungeon.floorTileCount() == 18);
    REQUIRE(dungeon.reachableFrom({1, 1}) == 9);
    REQUIRE_FALSE(dungeon.isFullyConnected());

    SECTION("joining the rooms makes the map fully connected") {
        for (int i = 3; i <= 6; ++i) dungeon.set(i, i, Tile::Floor);
        // Diagonal steps alone are not enough: the flood fill is 4-connected on
        // purpose, because corner-cutting is disabled for movement too.
        REQUIRE_FALSE(dungeon.isFullyConnected());

        for (int x = 3; x <= 6; ++x) dungeon.set(x, 3, Tile::Floor);
        for (int y = 3; y <= 6; ++y) dungeon.set(6, y, Tile::Floor);
        REQUIRE(dungeon.isFullyConnected());
    }
}
