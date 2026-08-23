#include "Pathfinding.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>

namespace game {
namespace {

constexpr float kStraightCost = 1.0f;
constexpr float kDiagonalCost = 1.41421356f;


float octile(int dx, int dy) {
    dx = std::abs(dx);
    dy = std::abs(dy);
    const int lo = std::min(dx, dy);
    const int hi = std::max(dx, dy);
    return kDiagonalCost * static_cast<float>(lo) +
           kStraightCost * static_cast<float>(hi - lo);
}

struct OpenNode {
    float f = 0.0f;
    int index = 0;
    // Greater-than gives std::priority_queue (a max-heap) min-heap behaviour.
    bool operator<(const OpenNode& other) const { return f > other.f; }
};

}  


// NavGrid

NavGrid::NavGrid(int width, int height) { resize(width, height); }

void NavGrid::resize(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    blocked_.assign(static_cast<std::size_t>(width_) * height_, 0);
}

void NavGrid::setBlocked(int x, int y, bool blocked) {
    if (!inBounds(x, y)) return;
    blocked_[static_cast<std::size_t>(y) * width_ + x] = blocked ? 1 : 0;
}

bool NavGrid::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

bool NavGrid::walkable(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return blocked_[static_cast<std::size_t>(y) * width_ + x] == 0;
}

NavGrid NavGrid::fromRows(const std::vector<const char*>& rows) {
    if (rows.empty()) return NavGrid();
    const int height = static_cast<int>(rows.size());
    const int width = static_cast<int>(std::strlen(rows[0]));
    NavGrid grid(width, height);
    for (int y = 0; y < height; ++y) {
        const char* row = rows[y];
        for (int x = 0; x < width && row[x] != '\0'; ++x) {
            grid.setBlocked(x, y, row[x] == '#');
        }
    }
    return grid;
}


// Pathfinder


void Pathfinder::prepare(const NavGrid& grid) {
    const std::size_t cells = static_cast<std::size_t>(grid.width()) * grid.height();
    if (width_ != grid.width() || height_ != grid.height()) {
        width_ = grid.width();
        height_ = grid.height();
        gScore_.assign(cells, 0.0f);
        parent_.assign(cells, -1);
        closed_.assign(cells, 0);
        visitStamp_.assign(cells, 0);
        searchId_ = 0;
    }

    ++searchId_;
    stats_ = PathStats{};
}

bool Pathfinder::findPath(const NavGrid& grid, GridPoint start, GridPoint goal,
                          std::vector<GridPoint>& outPath, const PathOptions& options) {
    outPath.clear();

    if (!grid.walkable(start.x, start.y) || !grid.walkable(goal.x, goal.y)) {
        return false;
    }
    if (start == goal) {
        outPath.push_back(start);
        return true;
    }

    prepare(grid);

    const int width = grid.width();
    auto indexOf = [width](int x, int y) { return y * width + x; };

    // 8-way neighbour offsets, straight moves first so ties resolve to the
    // cheaper move and paths look less jittery.
    struct Step { int dx, dy; float cost; };
    static const Step kSteps[8] = {
        {1, 0, kStraightCost},  {-1, 0, kStraightCost},
        {0, 1, kStraightCost},  {0, -1, kStraightCost},
        {1, 1, kDiagonalCost},  {1, -1, kDiagonalCost},
        {-1, 1, kDiagonalCost}, {-1, -1, kDiagonalCost},
    };
    const int stepCount = options.allowDiagonal ? 8 : 4;

    std::priority_queue<OpenNode> open;
    const int startIndex = indexOf(start.x, start.y);
    const int goalIndex = indexOf(goal.x, goal.y);

    gScore_[startIndex] = 0.0f;
    parent_[startIndex] = -1;
    visitStamp_[startIndex] = searchId_;
    closed_[startIndex] = 0;
    open.push({octile(start.x - goal.x, start.y - goal.y), startIndex});

    bool found = false;
    while (!open.empty()) {
        const OpenNode current = open.top();
        open.pop();


        if (closed_[current.index]) continue;
        closed_[current.index] = 1;
        ++stats_.nodesExpanded;

        if (current.index == goalIndex) {
            found = true;
            break;
        }
        if (stats_.nodesExpanded >= options.maxNodes) break;

        const int cx = current.index % width;
        const int cy = current.index / width;

        for (int i = 0; i < stepCount; ++i) {
            const int nx = cx + kSteps[i].dx;
            const int ny = cy + kSteps[i].dy;
            if (!grid.walkable(nx, ny)) continue;

            const bool diagonal = kSteps[i].dx != 0 && kSteps[i].dy != 0;
            if (diagonal && options.preventCornerCutting) {
                // Both orthogonal neighbours must be open, otherwise the move
                // squeezes through the diagonal gap between two wall corners.
                if (!grid.walkable(cx + kSteps[i].dx, cy) ||
                    !grid.walkable(cx, cy + kSteps[i].dy)) {
                    continue;
                }
            }

            const int neighbour = indexOf(nx, ny);
            if (closed_[neighbour] && visitStamp_[neighbour] == searchId_) continue;

            const float tentative = gScore_[current.index] + kSteps[i].cost;
            const bool seen = visitStamp_[neighbour] == searchId_;
            if (seen && tentative >= gScore_[neighbour]) continue;

            gScore_[neighbour] = tentative;
            parent_[neighbour] = current.index;
            visitStamp_[neighbour] = searchId_;
            closed_[neighbour] = 0;
            open.push({tentative + octile(nx - goal.x, ny - goal.y), neighbour});
        }
    }

    if (!found) return false;

    stats_.cost = gScore_[goalIndex];

    // Walk the parent chain back and flip it.
    for (int index = goalIndex; index != -1; index = parent_[index]) {
        outPath.push_back({index % width, index / width});
        if (index == startIndex) break;
    }
    std::reverse(outPath.begin(), outPath.end());
    return true;
}

bool Pathfinder::lineOfSight(const NavGrid& grid, GridPoint a, GridPoint b) {
    int x = a.x;
    int y = a.y;
    const int dx = std::abs(b.x - a.x);
    const int dy = -std::abs(b.y - a.y);
    const int sx = a.x < b.x ? 1 : -1;
    const int sy = a.y < b.y ? 1 : -1;
    int error = dx + dy;

    while (true) {
        if (!grid.walkable(x, y)) return false;
        if (x == b.x && y == b.y) return true;

        const int doubled = 2 * error;
        // Stepping both axes at once is a diagonal move, so apply the same
        // corner rule the search uses -- otherwise simplify() would happily
        // shortcut through a wall the pathfinder carefully avoided.
        if (doubled >= dy && doubled <= dx) {
            if (!grid.walkable(x + sx, y) || !grid.walkable(x, y + sy)) return false;
        }
        if (doubled >= dy) { error += dy; x += sx; }
        if (doubled <= dx) { error += dx; y += sy; }
    }
}

void Pathfinder::simplify(const NavGrid& grid, std::vector<GridPoint>& path) {
    if (path.size() < 3) return;

    std::vector<GridPoint> result;
    result.push_back(path.front());

    std::size_t anchor = 0;
    while (anchor < path.size() - 1) {
        // Greedily reach for the furthest waypoint still in sight of the anchor.
        std::size_t furthest = anchor + 1;
        for (std::size_t probe = path.size() - 1; probe > anchor; --probe) {
            if (lineOfSight(grid, path[anchor], path[probe])) {
                furthest = probe;
                break;
            }
        }
        result.push_back(path[furthest]);
        anchor = furthest;
    }

    path.swap(result);
}

}  
