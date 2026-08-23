#pragma once

#include <vector>

#include "../ecs/System.h"
#include "Collision.h"

namespace engine {

/// Reported when two trigger-eligible colliders overlap during a fixed step.
/// The physics system only detects these -- deciding that "player touched enemy"
/// means damage is a game-layer concern, so it just hands over the pairs.
struct OverlapEvent {
    Entity a = kNullEntity;
    Entity b = kNullEntity;
};

/// Moves everything with a Velocity, keeps solid bodies out of walls, and
/// collects overlaps for the game to interpret.
class PhysicsSystem final : public System {
public:
    /// The grid is injected rather than owned; it changes every time a new floor
    /// is generated and the physics system should not care when that happens.
    void setSolidGrid(const SolidGrid* grid) { grid_ = grid; }

    void fixedUpdate(Registry& registry, float dt) override;

    const std::vector<OverlapEvent>& overlapEvents() const { return overlaps_; }
    void clearOverlapEvents() { overlaps_.clear(); }

private:
    /// Move one axis at a time and push out of any tile we ended up inside.
    ///
    /// Doing X and Y separately is what gives clean wall sliding: run diagonally
    /// into a wall and the blocked axis is cancelled while the free axis keeps
    /// its full speed. Resolving both at once instead makes the character stick
    /// to walls, which feels broken long before anyone can explain why.
    void moveAndCollide(Transform& transform, const Collider& collider,
                        Velocity& velocity, float dt) const;

    bool tileSolid(float worldX, float worldY) const;

    const SolidGrid* grid_ = nullptr;
    std::vector<OverlapEvent> overlaps_;
};

}  // namespace engine
