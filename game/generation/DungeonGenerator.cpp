#include "DungeonGenerator.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

namespace game {


// Dungeon

Dungeon::Dungeon(int width, int height, float tileSize) { reset(width, height, tileSize); }

void Dungeon::reset(int width, int height, float tileSize) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    tileSize_ = tileSize;
    tiles_.assign(static_cast<std::size_t>(width_) * height_, Tile::Wall);
    rooms_.clear();
    entrance_ = GridPoint{};
    exit_ = GridPoint{};
}

bool Dungeon::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

Tile Dungeon::at(int x, int y) const {
    if (!inBounds(x, y)) return Tile::Wall;
    return tiles_[static_cast<std::size_t>(y) * width_ + x];
}

void Dungeon::set(int x, int y, Tile tile) {
    if (!inBounds(x, y)) return;
    tiles_[static_cast<std::size_t>(y) * width_ + x] = tile;
}

bool Dungeon::isFloor(int x, int y) const {
    const Tile t = at(x, y);
    return t == Tile::Floor || t == Tile::StairsDown;
}

bool Dungeon::isSolid(int tileX, int tileY) const {
    if (!inBounds(tileX, tileY)) return true;
    return !isFloor(tileX, tileY);
}

int Dungeon::tileXAt(float wx) const { return static_cast<int>(std::floor(wx / tileSize_)); }
int Dungeon::tileYAt(float wy) const { return static_cast<int>(std::floor(wy / tileSize_)); }

NavGrid Dungeon::buildNavGrid() const {
    NavGrid grid(width_, height_);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            grid.setBlocked(x, y, !isFloor(x, y));
        }
    }
    return grid;
}

int Dungeon::reachableFrom(GridPoint origin, std::vector<std::uint8_t>* outVisited) const {
    std::vector<std::uint8_t> visited(static_cast<std::size_t>(width_) * height_, 0);
    if (!isFloor(origin.x, origin.y)) {
        if (outVisited) *outVisited = std::move(visited);
        return 0;
    }

    std::deque<GridPoint> queue;
    queue.push_back(origin);
    visited[static_cast<std::size_t>(origin.y) * width_ + origin.x] = 1;
    int count = 0;

    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    while (!queue.empty()) {
        const GridPoint p = queue.front();
        queue.pop_front();
        ++count;
        for (int i = 0; i < 4; ++i) {
            const int nx = p.x + dx[i];
            const int ny = p.y + dy[i];
            if (!isFloor(nx, ny)) continue;
            std::uint8_t& mark = visited[static_cast<std::size_t>(ny) * width_ + nx];
            if (mark) continue;
            mark = 1;
            queue.push_back({nx, ny});
        }
    }

    if (outVisited) *outVisited = std::move(visited);
    return count;
}

int Dungeon::floorTileCount() const {
    int count = 0;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (isFloor(x, y)) ++count;
        }
    }
    return count;
}

bool Dungeon::isFullyConnected() const {
    const int total = floorTileCount();
    if (total == 0) return false;
    return reachableFrom(entrance_) == total;
}


// DungeonGenerator - entry point


int DungeonGenerator::randRange(int lo, int hi) {
    if (hi <= lo) return lo;
    return std::uniform_int_distribution<int>(lo, hi)(rng_);
}

float DungeonGenerator::randFloat() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_);
}

Dungeon DungeonGenerator::generate(const GenerationParams& params) {
    rng_.seed(params.seed);
    leaves_.clear();

    Dungeon dungeon(params.width, params.height, params.tileSize);

    if (params.style == GenerationStyle::BSP) {
        generateBSP(dungeon, params);
    } else {
        generateCaves(dungeon, params);
    }

    placeEntranceAndExit(dungeon);


    if (!dungeon.isFullyConnected()) {
        repairConnectivity(dungeon);
        placeEntranceAndExit(dungeon);
    }

    return dungeon;
}

// BSP generation


void DungeonGenerator::generateBSP(Dungeon& dungeon, const GenerationParams& params) {
    // Keep a one-tile margin so rooms never sit flush against the map border.
    Leaf root;
    root.region = Room{1, 1, params.width - 2, params.height - 2};
    leaves_.push_back(root);


    std::deque<int> pending{0};
    while (!pending.empty()) {
        const int index = pending.front();
        pending.pop_front();
        if (splitLeaf(index, params)) {
            pending.push_back(leaves_[index].left);
            pending.push_back(leaves_[index].right);
        }
    }

    carveRooms(dungeon, params);
    connectLeaves(dungeon, 0);
}

bool DungeonGenerator::splitLeaf(int leafIndex, const GenerationParams& params) {
    Leaf& leaf = leaves_[leafIndex];
    if (leaf.left != -1) return false;  // already split

    const int w = leaf.region.width;
    const int h = leaf.region.height;
    const bool tooSmall = w < params.minLeafSize * 2 && h < params.minLeafSize * 2;
    if (tooSmall) return false;
    const bool oversized = w > params.maxLeafSize || h > params.maxLeafSize;
    if (!oversized && randFloat() < 0.25f) return false;

    // Split across the long axis. Splitting the short axis produces slivers that
    // cannot hold a room, and those slivers become dead corridor space.
    bool splitHorizontally;
    if (w > h * 1.25f) {
        splitHorizontally = false;  // wide region -> vertical cut
    } else if (h > w * 1.25f) {
        splitHorizontally = true;
    } else {
        splitHorizontally = randFloat() < 0.5f;
    }

    const int extent = splitHorizontally ? h : w;
    if (extent < params.minLeafSize * 2) return false;

    const int cut = randRange(params.minLeafSize, extent - params.minLeafSize);

    Leaf a;
    Leaf b;
    if (splitHorizontally) {
        a.region = Room{leaf.region.x, leaf.region.y, w, cut};
        b.region = Room{leaf.region.x, leaf.region.y + cut, w, h - cut};
    } else {
        a.region = Room{leaf.region.x, leaf.region.y, cut, h};
        b.region = Room{leaf.region.x + cut, leaf.region.y, w - cut, h};
    }

    const int leftIndex = static_cast<int>(leaves_.size());
    leaves_.push_back(a);
    leaves_.push_back(b);
    // Re-index: the push_backs above may have reallocated the vector.
    leaves_[leafIndex].left = leftIndex;
    leaves_[leafIndex].right = leftIndex + 1;
    return true;
}

void DungeonGenerator::carveRooms(Dungeon& dungeon, const GenerationParams& params) {
    for (std::size_t i = 0; i < leaves_.size(); ++i) {
        Leaf& leaf = leaves_[i];
        if (leaf.left != -1) continue;  // only leaves get rooms

        const Room& region = leaf.region;
        const int maxW = std::max(params.minRoomSize,
                                  static_cast<int>(region.width * params.roomFill));
        const int maxH = std::max(params.minRoomSize,
                                  static_cast<int>(region.height * params.roomFill));
        if (region.width < params.minRoomSize + 2 || region.height < params.minRoomSize + 2) {
            continue;  // region too tight to hold a room with walls around it
        }

        const int roomW = randRange(params.minRoomSize, std::min(maxW, region.width - 2));
        const int roomH = randRange(params.minRoomSize, std::min(maxH, region.height - 2));
        const int roomX = region.x + randRange(1, region.width - roomW - 1);
        const int roomY = region.y + randRange(1, region.height - roomH - 1);

        Room room{roomX, roomY, roomW, roomH};
        for (int y = room.top(); y < room.bottom(); ++y) {
            for (int x = room.left(); x < room.right(); ++x) {
                dungeon.set(x, y, Tile::Floor);
            }
        }

        leaf.roomIndex = static_cast<int>(dungeon.rooms().size());
        dungeon.rooms().push_back(room);
    }
}

void DungeonGenerator::connectLeaves(Dungeon& dungeon, int leafIndex) {
    Leaf& leaf = leaves_[leafIndex];
    if (leaf.left == -1) return;

    connectLeaves(dungeon, leaf.left);
    connectLeaves(dungeon, leaf.right);
    auto findRoom = [&](int index, GridPoint& out) -> bool {
        std::deque<int> stack{index};
        std::vector<int> candidates;
        while (!stack.empty()) {
            const int current = stack.front();
            stack.pop_front();
            const Leaf& node = leaves_[current];
            if (node.roomIndex != -1) candidates.push_back(node.roomIndex);
            if (node.left != -1) {
                stack.push_back(node.left);
                stack.push_back(node.right);
            }
        }
        if (candidates.empty()) return false;
        const Room& room = dungeon.rooms()[candidates[randRange(0, static_cast<int>(candidates.size()) - 1)]];
        out = {room.centerX(), room.centerY()};
        return true;
    };

    GridPoint a;
    GridPoint b;
    if (findRoom(leaf.left, a) && findRoom(leaf.right, b)) {
        carveCorridor(dungeon, a, b);
    }
}


// Cellular automata generation


void DungeonGenerator::generateCaves(Dungeon& dungeon, const GenerationParams& params) {
    // 1. Random noise, with a solid border.
    for (int y = 1; y < params.height - 1; ++y) {
        for (int x = 1; x < params.width - 1; ++x) {
            dungeon.set(x, y, randFloat() < params.initialWallChance ? Tile::Wall : Tile::Floor);
        }
    }

    for (int pass = 0; pass < params.smoothingPasses; ++pass) {
        Dungeon next = dungeon;
        for (int y = 1; y < params.height - 1; ++y) {
            for (int x = 1; x < params.width - 1; ++x) {
                const int walls = countWallNeighbours(dungeon, x, y);
                if (walls > 4) {
                    next.set(x, y, Tile::Wall);
                } else if (walls < 4) {
                    next.set(x, y, Tile::Floor);
                }
    
            }
        }
        dungeon = std::move(next);
    }

    connectCaveRegions(dungeon, params);
    deriveRoomsFromCaves(dungeon);
}

int DungeonGenerator::countWallNeighbours(const Dungeon& dungeon, int x, int y) const {
    int walls = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (!dungeon.isFloor(x + dx, y + dy)) ++walls;  // out of bounds counts as wall
        }
    }
    return walls;
}

void DungeonGenerator::connectCaveRegions(Dungeon& dungeon, const GenerationParams& params) {
    const int width = dungeon.widthInTiles();
    const int height = dungeon.heightInTiles();

    // Label every connected floor region.
    std::vector<int> label(static_cast<std::size_t>(width) * height, -1);
    std::vector<std::vector<GridPoint>> regions;

    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!dungeon.isFloor(x, y)) continue;
            if (label[static_cast<std::size_t>(y) * width + x] != -1) continue;

            const int id = static_cast<int>(regions.size());
            std::vector<GridPoint> cells;
            std::deque<GridPoint> queue{{x, y}};
            label[static_cast<std::size_t>(y) * width + x] = id;

            while (!queue.empty()) {
                const GridPoint p = queue.front();
                queue.pop_front();
                cells.push_back(p);
                for (int i = 0; i < 4; ++i) {
                    const int nx = p.x + dx[i];
                    const int ny = p.y + dy[i];
                    if (!dungeon.isFloor(nx, ny)) continue;
                    int& mark = label[static_cast<std::size_t>(ny) * width + nx];
                    if (mark != -1) continue;
                    mark = id;
                    queue.push_back({nx, ny});
                }
            }
            regions.push_back(std::move(cells));
        }
    }

    if (regions.empty()) return;

    std::vector<std::vector<GridPoint>> kept;
    for (auto& region : regions) {
        if (static_cast<int>(region.size()) < params.minCaveSize) {
            for (const GridPoint& p : region) dungeon.set(p.x, p.y, Tile::Wall);
        } else {
            kept.push_back(std::move(region));
        }
    }

    // Everything got wiped out (possible with an unlucky seed and aggressive
    // smoothing). Carve a fallback chamber so the floor is at least playable.
    if (kept.empty()) {
        const int cx = width / 2;
        const int cy = height / 2;
        for (int y = cy - 4; y <= cy + 4; ++y) {
            for (int x = cx - 6; x <= cx + 6; ++x) dungeon.set(x, y, Tile::Floor);
        }
        return;
    }

    std::sort(kept.begin(), kept.end(),
              [](const auto& a, const auto& b) { return a.size() > b.size(); });

    for (std::size_t i = 1; i < kept.size(); ++i) {
        long long best = std::numeric_limits<long long>::max();
        GridPoint from{};
        GridPoint to{};

        for (std::size_t a = 0; a < kept[0].size(); a += 4) {
            for (std::size_t b = 0; b < kept[i].size(); b += 4) {
                const long long ddx = kept[0][a].x - kept[i][b].x;
                const long long ddy = kept[0][a].y - kept[i][b].y;
                const long long distance = ddx * ddx + ddy * ddy;
                if (distance < best) {
                    best = distance;
                    from = kept[0][a];
                    to = kept[i][b];
                }
            }
        }
        carveCorridor(dungeon, from, to);
    }
}

void DungeonGenerator::deriveRoomsFromCaves(Dungeon& dungeon) {
    const int width = dungeon.widthInTiles();
    const int height = dungeon.heightInTiles();
    const int cell = 12;

    dungeon.rooms().clear();
    for (int gy = 0; gy < height; gy += cell) {
        for (int gx = 0; gx < width; gx += cell) {
            int minX = width, minY = height, maxX = -1, maxY = -1, count = 0;
            for (int y = gy; y < std::min(gy + cell, height); ++y) {
                for (int x = gx; x < std::min(gx + cell, width); ++x) {
                    if (!dungeon.isFloor(x, y)) continue;
                    ++count;
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    maxX = std::max(maxX, x);
                    maxY = std::max(maxY, y);
                }
            }
            if (count < 12) continue;
            dungeon.rooms().push_back(Room{minX, minY, maxX - minX + 1, maxY - minY + 1});
        }
    }
}


// Shared helpers

void DungeonGenerator::carveHorizontal(Dungeon& dungeon, int x1, int x2, int y) {
    for (int x = std::min(x1, x2); x <= std::max(x1, x2); ++x) {
        dungeon.set(x, y, Tile::Floor);
        dungeon.set(x, y + 1, Tile::Floor);
    }
}

void DungeonGenerator::carveVertical(Dungeon& dungeon, int y1, int y2, int x) {
    for (int y = std::min(y1, y2); y <= std::max(y1, y2); ++y) {
        dungeon.set(x, y, Tile::Floor);
        dungeon.set(x + 1, y, Tile::Floor);
    }
}

void DungeonGenerator::carveCorridor(Dungeon& dungeon, GridPoint from, GridPoint to) {
    if (randFloat() < 0.5f) {
        carveHorizontal(dungeon, from.x, to.x, from.y);
        carveVertical(dungeon, from.y, to.y, to.x);
    } else {
        carveVertical(dungeon, from.y, to.y, from.x);
        carveHorizontal(dungeon, from.x, to.x, to.y);
    }
}

GridPoint DungeonGenerator::nearestFloor(const Dungeon& dungeon, GridPoint origin) const {
    if (dungeon.isFloor(origin.x, origin.y)) return origin;

    const int maxRadius = std::max(dungeon.widthInTiles(), dungeon.heightInTiles());
    for (int radius = 1; radius < maxRadius; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                // Only the ring at this radius; the interior was covered already.
                if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
                if (dungeon.isFloor(origin.x + dx, origin.y + dy)) {
                    return GridPoint{origin.x + dx, origin.y + dy};
                }
            }
        }
    }
    return origin;
}

std::vector<int> DungeonGenerator::distanceField(const Dungeon& dungeon,
                                                 GridPoint origin) const {
    const int width = dungeon.widthInTiles();
    const int height = dungeon.heightInTiles();
    std::vector<int> distance(static_cast<std::size_t>(width) * height, -1);
    if (!dungeon.isFloor(origin.x, origin.y)) return distance;

    std::deque<GridPoint> queue{origin};
    distance[static_cast<std::size_t>(origin.y) * width + origin.x] = 0;

    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    while (!queue.empty()) {
        const GridPoint p = queue.front();
        queue.pop_front();
        const int here = distance[static_cast<std::size_t>(p.y) * width + p.x];
        for (int i = 0; i < 4; ++i) {
            const int nx = p.x + dx[i];
            const int ny = p.y + dy[i];
            if (!dungeon.isFloor(nx, ny)) continue;
            int& next = distance[static_cast<std::size_t>(ny) * width + nx];
            if (next != -1) continue;
            next = here + 1;
            queue.push_back({nx, ny});
        }
    }
    return distance;
}

void DungeonGenerator::placeEntranceAndExit(Dungeon& dungeon) {
    // --- Entrance ---
    GridPoint entrance{-1, -1};
    if (!dungeon.rooms().empty()) {
        Room& entranceRoom = dungeon.rooms().front();
        entranceRoom.type = RoomType::Entrance;
        entrance = nearestFloor(dungeon, {entranceRoom.centerX(), entranceRoom.centerY()});
    }
    if (entrance.x < 0 || !dungeon.isFloor(entrance.x, entrance.y)) {
        // No usable rooms at all: fall back to the first floor tile on the map.
        for (int y = 0; y < dungeon.heightInTiles() && entrance.x < 0; ++y) {
            for (int x = 0; x < dungeon.widthInTiles(); ++x) {
                if (dungeon.isFloor(x, y)) {
                    entrance = {x, y};
                    break;
                }
            }
        }
    }
    if (entrance.x < 0) return;  // nothing was carved; caller's repair pass handles it
    dungeon.setEntrance(entrance);


    const std::vector<int> distance = distanceField(dungeon, entrance);
    const int width = dungeon.widthInTiles();

    GridPoint exit = entrance;
    int bestDistance = -1;
    for (int y = 0; y < dungeon.heightInTiles(); ++y) {
        for (int x = 0; x < width; ++x) {
            const int d = distance[static_cast<std::size_t>(y) * width + x];
            if (d > bestDistance) {
                bestDistance = d;
                exit = {x, y};
            }
        }
    }

    dungeon.setExit(exit);
    dungeon.set(exit.x, exit.y, Tile::StairsDown);

    // Tag whichever room contains the exit, for anything that reasons in rooms.
    for (Room& room : dungeon.rooms()) {
        if (room.contains(exit.x, exit.y)) {
            room.type = RoomType::Exit;
            break;
        }
    }
}

void DungeonGenerator::repairConnectivity(Dungeon& dungeon) {
    const int width = dungeon.widthInTiles();
    const int height = dungeon.heightInTiles();
    for (int attempt = 0; attempt < 32; ++attempt) {
        std::vector<std::uint8_t> visited;
        const int reached = dungeon.reachableFrom(dungeon.entrance(), &visited);
        if (reached == dungeon.floorTileCount()) return;

        GridPoint orphan{-1, -1};
        for (int y = 0; y < height && orphan.x < 0; ++y) {
            for (int x = 0; x < width; ++x) {
                if (dungeon.isFloor(x, y) && !visited[static_cast<std::size_t>(y) * width + x]) {
                    orphan = {x, y};
                    break;
                }
            }
        }
        if (orphan.x < 0) return;

        // Nearest reachable tile to that orphan.
        GridPoint anchor{-1, -1};
        long long best = std::numeric_limits<long long>::max();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (!visited[static_cast<std::size_t>(y) * width + x]) continue;
                const long long dx = x - orphan.x;
                const long long dy = y - orphan.y;
                const long long distance = dx * dx + dy * dy;
                if (distance < best) {
                    best = distance;
                    anchor = {x, y};
                }
            }
        }
        if (anchor.x < 0) return;

        carveCorridor(dungeon, anchor, orphan);
    }
}

std::vector<GridPoint> DungeonGenerator::spawnCandidates(const Dungeon& dungeon,
                                                         int minDistanceFromEntrance) const {
    std::vector<GridPoint> candidates;
    const GridPoint entrance = dungeon.entrance();
    const long long minSquared =
        static_cast<long long>(minDistanceFromEntrance) * minDistanceFromEntrance;

    for (int y = 1; y < dungeon.heightInTiles() - 1; ++y) {
        for (int x = 1; x < dungeon.widthInTiles() - 1; ++x) {
            if (dungeon.at(x, y) != Tile::Floor) continue;  // never on the stairs


            int open = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dungeon.isFloor(x + dx, y + dy)) ++open;
                }
            }
            if (open < 7) continue;

            const long long dx = x - entrance.x;
            const long long dy = y - entrance.y;
            if (dx * dx + dy * dy < minSquared) continue;

            candidates.push_back({x, y});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [entrance](const GridPoint& a, const GridPoint& b) {
                  const long long da = (a.x - entrance.x) * (a.x - entrance.x) +
                                       (a.y - entrance.y) * (a.y - entrance.y);
                  const long long db = (b.x - entrance.x) * (b.x - entrance.x) +
                                       (b.y - entrance.y) * (b.y - entrance.y);
                  return da < db;
              });
    return candidates;
}

}  
