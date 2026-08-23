#pragma once

#include "../ecs/Component.h"
#include "../ecs/Entity.h"

namespace engine {

/// Axis-aligned bounding box stored as min corner + size, which is the form
/// that makes the overlap test cheapest.
struct AABB {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float left() const { return x; }
    float right() const { return x + w; }
    float top() const { return y; }
    float bottom() const { return y + h; }
    float centerX() const { return x + w * 0.5f; }
    float centerY() const { return y + h * 0.5f; }
};

/// Build the world-space box for an entity from its transform and collider.
AABB makeAABB(const Transform& transform, const Collider& collider);

/// Boxes that merely touch edge-to-edge do not count as overlapping. Using a
/// strict comparison here is what stops an entity resting exactly against a wall
/// from re-triggering a collision every single frame.
bool overlaps(const AABB& a, const AABB& b);

/// Result of pushing box `a` out of box `b`.
struct Penetration {
    bool hit = false;
    float normalX = 0.0f;  // direction to push `a` along, unit length on one axis
    float normalY = 0.0f;
    float depth = 0.0f;    // how far to push
};

/// Minimum translation vector: resolve along whichever axis is least overlapped,
/// because that is the axis the box most recently crossed. Resolving the deeper
/// axis instead is the classic bug that teleports a character through a wall
/// when they graze a corner.
Penetration resolve(const AABB& a, const AABB& b);

/// Swept test: how far along `(dx, dy)` can `box` travel before hitting `solid`?
///
/// The tile-grid mover below resolves axis-by-axis and does not need this, but
/// anything fast and small -- projectiles, dashes -- would tunnel straight
/// through a wall without it, so the primitive is here and unit tested.
struct Sweep {
    bool hit = false;
    float time = 1.0f;     // fraction of the motion completed before impact, [0,1]
    float normalX = 0.0f;
    float normalY = 0.0f;
};

Sweep sweep(const AABB& box, float dx, float dy, const AABB& solid);

/// Grid of solid tiles, implemented by whatever owns the level.
///
/// This interface is the entire contract between the physics code and the level
/// representation. The engine never learns what a dungeon is; it only ever asks
/// "is this cell solid".
class SolidGrid {
public:
    virtual ~SolidGrid() = default;
    virtual bool isSolid(int tileX, int tileY) const = 0;
    virtual float tileSize() const = 0;
    virtual int widthInTiles() const = 0;
    virtual int heightInTiles() const = 0;
};

}  // namespace engine
