#include "Physics.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

/// Nudge used when snapping out of a tile. Landing exactly on the boundary makes
/// the next frame's overlap test ambiguous thanks to float rounding, so we stop
/// a hair short instead.
constexpr float kSkin = 0.01f;

}  // namespace

bool PhysicsSystem::tileSolid(float worldX, float worldY) const {
    if (!grid_) return false;
    const float ts = grid_->tileSize();
    const int tx = static_cast<int>(std::floor(worldX / ts));
    const int ty = static_cast<int>(std::floor(worldY / ts));
    return grid_->isSolid(tx, ty);
}

void PhysicsSystem::moveAndCollide(Transform& transform, const Collider& collider,
                                   Velocity& velocity, float dt) const {
    if (!grid_) {
        transform.x += velocity.x * dt;
        transform.y += velocity.y * dt;
        return;
    }

    const float ts = grid_->tileSize();
    float remainingX = velocity.x * dt;
    float remainingY = velocity.y * dt;

    // Split the motion so no single step can cross a whole tile. Without this a
    // fast entity can start on one side of a wall and end on the other, with the
    // overlap test never seeing it inside.
    const float longest = std::max(std::fabs(remainingX), std::fabs(remainingY));
    const int steps = std::max(1, static_cast<int>(std::ceil(longest / (ts * 0.5f))));
    const float stepX = remainingX / static_cast<float>(steps);
    const float stepY = remainingY / static_cast<float>(steps);

    auto blocked = [&](const AABB& box) {
        // Sample the tiles under each corner, pulled inward by the skin so a box
        // flush against a wall does not test the wall's own cell.
        const float x0 = box.left() + kSkin;
        const float x1 = box.right() - kSkin;
        const float y0 = box.top() + kSkin;
        const float y1 = box.bottom() - kSkin;
        return tileSolid(x0, y0) || tileSolid(x1, y0) ||
               tileSolid(x0, y1) || tileSolid(x1, y1);
    };

    for (int i = 0; i < steps; ++i) {
        // --- X axis ---
        transform.x += stepX;
        AABB box = makeAABB(transform, collider);
        if (blocked(box)) {
            // Snap flush to the tile edge we crossed, then kill the velocity so
            // the entity does not keep accumulating speed into the wall.
            const float edge = (stepX > 0.0f)
                ? std::floor(box.right() / ts) * ts - collider.width * 0.5f
                : (std::floor(box.left() / ts) + 1.0f) * ts + collider.width * 0.5f;
            transform.x = edge - collider.offsetX + (stepX > 0.0f ? -kSkin : kSkin);
            velocity.x = 0.0f;
        }

        // --- Y axis ---
        transform.y += stepY;
        box = makeAABB(transform, collider);
        if (blocked(box)) {
            const float edge = (stepY > 0.0f)
                ? std::floor(box.bottom() / ts) * ts - collider.height * 0.5f
                : (std::floor(box.top() / ts) + 1.0f) * ts + collider.height * 0.5f;
            transform.y = edge - collider.offsetY + (stepY > 0.0f ? -kSkin : kSkin);
            velocity.y = 0.0f;
        }
    }
}

void PhysicsSystem::fixedUpdate(Registry& registry, float dt) {
    overlaps_.clear();

    // 1. Integrate and resolve against level geometry.
    registry.each<Velocity, Transform, Collider>(
        [&](Entity, Velocity& velocity, Transform& transform, Collider& collider) {
            if (collider.isStatic) return;
            if (collider.isTrigger) {
                // Triggers pass through walls by design -- a sword swing should
                // not be stopped by the wall behind the enemy it is hitting.
                transform.x += velocity.x * dt;
                transform.y += velocity.y * dt;
                return;
            }
            moveAndCollide(transform, collider, velocity, dt);
        });

    // 2. Entity-vs-entity overlaps.
    //
    // This is an O(n^2) pass over colliders. With a few dozen entities per floor
    // that is a handful of microseconds, and a spatial hash would be more code to
    // get wrong than it saves. Flagged in docs/design-decisions.md as the first
    // thing to change if entity counts ever grow.
    auto& colliders = registry.pool<Collider>();
    const std::vector<Entity> ids = colliders.entities();

    for (std::size_t i = 0; i < ids.size(); ++i) {
        Transform* ta = registry.tryGet<Transform>(ids[i]);
        Collider* ca = registry.tryGet<Collider>(ids[i]);
        if (!ta || !ca) continue;
        const AABB boxA = makeAABB(*ta, *ca);

        for (std::size_t j = i + 1; j < ids.size(); ++j) {
            Collider* cb = registry.tryGet<Collider>(ids[j]);
            Transform* tb = registry.tryGet<Transform>(ids[j]);
            if (!tb || !cb) continue;

            // Layer/mask filter first: it is a couple of integer ANDs and it
            // discards most pairs before the box maths runs.
            const bool interested = (ca->mask & cb->layer) && (cb->mask & ca->layer);
            if (!interested) continue;

            if (overlaps(boxA, makeAABB(*tb, *cb))) {
                overlaps_.push_back({ids[i], ids[j]});
            }
        }
    }
}

}  // namespace engine
