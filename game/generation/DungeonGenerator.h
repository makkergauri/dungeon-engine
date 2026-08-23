#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "../../engine/physics/Collision.h"
#include "../ai/Pathfinding.h"
#include "Room.h"

namespace game {

enum class Tile : std::uint8_t {
    Wall,
    Floor,
    StairsDown,
};

class Dungeon final : public engine::SolidGrid {
public:
    Dungeon() = default;
    Dungeon(int width, int height, float tileSize = 32.0f);

    void reset(int width, int height, float tileSize = 32.0f);

    Tile at(int x, int y) const;
    void set(int x, int y, Tile tile);
    bool inBounds(int x, int y) const;
    bool isFloor(int x, int y) const;

    // --- engine::SolidGrid ---
    bool isSolid(int tileX, int tileY) const override;
    float tileSize() const override { return tileSize_; }
    int widthInTiles() const override { return width_; }
    int heightInTiles() const override { return height_; }

    
    float worldX(int tileX) const { return (tileX + 0.5f) * tileSize_; }
    float worldY(int tileY) const { return (tileY + 0.5f) * tileSize_; }
    int tileXAt(float worldX) const;
    int tileYAt(float worldY) const;

    const std::vector<Room>& rooms() const { return rooms_; }
    std::vector<Room>& rooms() { return rooms_; }

    GridPoint entrance() const { return entrance_; }
    GridPoint exit() const { return exit_; }
    void setEntrance(GridPoint p) { entrance_ = p; }
    void setExit(GridPoint p) { exit_ = p; }

    /// Walkability view for the pathfinder, rebuilt once per floor.
    NavGrid buildNavGrid() const;

    /// Flood fill from `origin`, returning how many floor tiles were reached and
    /// optionally which ones.
    int reachableFrom(GridPoint origin, std::vector<std::uint8_t>* outVisited = nullptr) const;

    /// True when every floor tile can be walked to from the entrance. This is the
    /// correctness property that actually matters -- a dungeon with a sealed-off
    /// treasure room is a bug the player experiences as "the game is broken".
    bool isFullyConnected() const;

    int floorTileCount() const;

private:
    int width_ = 0;
    int height_ = 0;
    float tileSize_ = 32.0f;
    std::vector<Tile> tiles_;
    std::vector<Room> rooms_;
    GridPoint entrance_;
    GridPoint exit_;
};

enum class GenerationStyle {
    BSP,
    CellularAutomata,
};

struct GenerationParams {
    int width = 96;
    int height = 64;
    float tileSize = 32.0f;
    GenerationStyle style = GenerationStyle::BSP;
    std::uint32_t seed = 0;

    // --- BSP ---
    
    int minLeafSize = 14;
    int maxLeafSize = 26;
    int minRoomSize = 6;
    float roomFill = 0.75f;

    // --- Cellular automata ---
    float initialWallChance = 0.45f;
    int smoothingPasses = 5;
    int minCaveSize = 40;

    /// Difficulty scaling, applied by the game layer rather than the generator.
    int floorNumber = 1;
};

class DungeonGenerator {
public:
    
    Dungeon generate(const GenerationParams& params);


    std::vector<GridPoint> spawnCandidates(const Dungeon& dungeon,
                                           int minDistanceFromEntrance = 6) const;

private:
    // --- BSP ---
    struct Leaf {
        Room region;
        int left = -1;   // index into leaves_, -1 when this is a leaf node
        int right = -1;
        int roomIndex = -1;
    };

    void generateBSP(Dungeon& dungeon, const GenerationParams& params);
    bool splitLeaf(int leafIndex, const GenerationParams& params);
    void carveRooms(Dungeon& dungeon, const GenerationParams& params);
    void connectLeaves(Dungeon& dungeon, int leafIndex);

    // --- Cellular automata ---
    void generateCaves(Dungeon& dungeon, const GenerationParams& params);
    int countWallNeighbours(const Dungeon& dungeon, int x, int y) const;
    /// Label connected floor regions, discard the small ones, tunnel between the
    /// survivors. This is where cave generation earns its keep or falls apart.
    void connectCaveRegions(Dungeon& dungeon, const GenerationParams& params);
    void deriveRoomsFromCaves(Dungeon& dungeon);

    // --- Shared ---
    void carveCorridor(Dungeon& dungeon, GridPoint from, GridPoint to);
    void carveHorizontal(Dungeon& dungeon, int x1, int x2, int y);
    void carveVertical(Dungeon& dungeon, int y1, int y2, int x);
    void placeEntranceAndExit(Dungeon& dungeon);
    /// Nearest floor tile to a point, for when a room's centre is a wall.
    GridPoint nearestFloor(const Dungeon& dungeon, GridPoint origin) const;
    std::vector<int> distanceField(const Dungeon& dungeon, GridPoint origin) const;
    void repairConnectivity(Dungeon& dungeon);

    int randRange(int lo, int hi);  // inclusive
    float randFloat();

    std::mt19937 rng_;
    std::vector<Leaf> leaves_;
};

} 
