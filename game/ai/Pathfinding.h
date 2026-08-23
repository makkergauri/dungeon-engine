#pragma once

#include <cstdint>
#include <vector>

namespace game {

struct GridPoint {
    int x = 0;
    int y = 0;

    bool operator==(const GridPoint& other) const { return x == other.x && y == other.y; }
    bool operator!=(const GridPoint& other) const { return !(*this == other); }
};

/// Flat walkability grid. Deliberately decoupled from the tile map: the
/// pathfinder is handed a plain bitmap, which means the unit tests can build a

class NavGrid {
public:
    NavGrid() = default;
    NavGrid(int width, int height);

    void resize(int width, int height);
    void setBlocked(int x, int y, bool blocked);

    bool inBounds(int x, int y) const;
    bool walkable(int x, int y) const;

    int width() const { return width_; }
    int height() const { return height_; }

    /// Build from an ASCII map, one string per row, '#' meaning blocked.
    /// Exists purely so tests read like the maze they describe.
    static NavGrid fromRows(const std::vector<const char*>& rows);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> blocked_;
};


struct PathStats {
    int nodesExpanded = 0;
    float cost = 0.0f;
};


struct PathOptions {
    bool allowDiagonal = true;
    bool preventCornerCutting = true;
    int maxNodes = 20000;
};

class Pathfinder {
public:
    /// Find a path from start to goal, returning true if one was found. The
    bool findPath(const NavGrid& grid, GridPoint start, GridPoint goal,
                  std::vector<GridPoint>& outPath, const PathOptions& options = PathOptions{});

    const PathStats& stats() const { return stats_; }

    
    static void simplify(const NavGrid& grid, std::vector<GridPoint>& path);

   
    static bool lineOfSight(const NavGrid& grid, GridPoint a, GridPoint b);

private:
    void prepare(const NavGrid& grid);

    int width_ = 0;
    int height_ = 0;
    std::vector<float> gScore_;
    std::vector<int> parent_;
    std::vector<std::uint32_t> visitStamp_;  // which search last touched a cell
    std::vector<std::uint8_t> closed_;
    std::uint32_t searchId_ = 0;
    PathStats stats_;
};

}  
