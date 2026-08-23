#include "Collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {

AABB makeAABB(const Transform& transform, const Collider& collider) {
    AABB box;
    box.w = collider.width;
    box.h = collider.height;
    // Transform position is the entity's centre, which keeps rotation and
    // scaling sane elsewhere, so shift by half the extents to get the min corner.
    box.x = transform.x + collider.offsetX - collider.width * 0.5f;
    box.y = transform.y + collider.offsetY - collider.height * 0.5f;
    return box;
}

bool overlaps(const AABB& a, const AABB& b) {
    return a.left() < b.right() && a.right() > b.left() &&
           a.top() < b.bottom() && a.bottom() > b.top();
}

Penetration resolve(const AABB& a, const AABB& b) {
    Penetration out;
    if (!overlaps(a, b)) return out;

    // Overlap on each axis, measured from a's perspective.
    const float overlapLeft = b.right() - a.left();    // push a right
    const float overlapRight = a.right() - b.left();   // push a left
    const float overlapTop = b.bottom() - a.top();     // push a down
    const float overlapBottom = a.bottom() - b.top();  // push a up

    const float xDepth = std::min(overlapLeft, overlapRight);
    const float yDepth = std::min(overlapTop, overlapBottom);

    out.hit = true;
    if (xDepth < yDepth) {
        out.depth = xDepth;
        out.normalX = (overlapLeft < overlapRight) ? 1.0f : -1.0f;
        out.normalY = 0.0f;
    } else {
        out.depth = yDepth;
        out.normalX = 0.0f;
        out.normalY = (overlapTop < overlapBottom) ? 1.0f : -1.0f;
    }
    return out;
}

Sweep sweep(const AABB& box, float dx, float dy, const AABB& solid) {
    Sweep out;

    // Already inside: report an immediate hit so the caller can push out rather
    // than sweep. Otherwise the slab maths below produces a negative entry time
    // and the mover happily slides deeper into the wall.
    if (overlaps(box, solid)) {
        out.hit = true;
        out.time = 0.0f;
        const Penetration p = resolve(box, solid);
        out.normalX = p.normalX;
        out.normalY = p.normalY;
        return out;
    }

    // Slab method. For each axis work out when the moving box enters and exits
    // the solid's span; an intersection exists if the entry times overlap.
    const float inf = std::numeric_limits<float>::infinity();
    float entryX, exitX, entryY, exitY;

    auto axis = [](float boxMin, float boxMax, float solidMin, float solidMax,
                   float delta, float& entry, float& exit, float infinity) {
        if (delta > 0.0f) {
            entry = (solidMin - boxMax) / delta;
            exit = (solidMax - boxMin) / delta;
        } else if (delta < 0.0f) {
            entry = (solidMax - boxMin) / delta;
            exit = (solidMin - boxMax) / delta;
        } else {
            // No motion on this axis: either permanently overlapping its slab or
            // permanently outside it.
            const bool overlapping = boxMax > solidMin && boxMin < solidMax;
            entry = overlapping ? -infinity : infinity;
            exit = overlapping ? infinity : -infinity;
        }
    };

    axis(box.left(), box.right(), solid.left(), solid.right(), dx, entryX, exitX, inf);
    axis(box.top(), box.bottom(), solid.top(), solid.bottom(), dy, entryY, exitY, inf);

    const float entryTime = std::max(entryX, entryY);
    const float exitTime = std::min(exitX, exitY);

    if (entryTime > exitTime || entryTime > 1.0f || entryTime < 0.0f) {
        return out;  // misses entirely, or the hit is beyond this frame's motion
    }

    out.hit = true;
    out.time = entryTime;
    if (entryX > entryY) {
        out.normalX = (dx > 0.0f) ? -1.0f : 1.0f;
        out.normalY = 0.0f;
    } else {
        out.normalX = 0.0f;
        out.normalY = (dy > 0.0f) ? -1.0f : 1.0f;
    }
    return out;
}

}  // namespace engine
