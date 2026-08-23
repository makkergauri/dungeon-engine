#pragma once

namespace game {
enum class RoomType {
    Normal,
    Entrance,
    Exit,
    Treasure,
};
struct Room {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    RoomType type = RoomType::Normal;

    int left() const { return x; }
    int right() const { return x + width; }
    int top() const { return y; }
    int bottom() const { return y + height; }

    int centerX() const { return x + width / 2; }
    int centerY() const { return y + height / 2; }

    int area() const { return width * height; }

    bool contains(int px, int py) const {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    /// Overlap test with a one-tile buffer, so two rooms never end up sharing a
    /// wall. Rooms that touch read as one lumpy room rather than two.
    bool intersects(const Room& other, int padding = 1) const {
        return left() - padding < other.right() && right() + padding > other.left() &&
               top() - padding < other.bottom() && bottom() + padding > other.top();
    }
};

}  
