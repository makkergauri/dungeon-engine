#pragma once
#include "../../engine/ecs/System.h"
#include "../../engine/physics/Physics.h"
#include "../GameCommon.h"
#include "../GameEvents.h"

namespace game {

/// Spawn a short-lived damage volume. Used by the player's swing and by enemy
/// projectiles, which differ only in whether they carry a Velocity.
engine::Entity spawnHitbox(engine::Registry& registry, engine::Entity owner, float x,
                           float y, float width, float height, int damage,
                           std::uint32_t layer, std::uint32_t mask, float lifetime,
                           float knockback);


class CombatSystem final : public engine::System {
public:
    CombatSystem(const engine::PhysicsSystem& physics, const GameAssets& assets,
                 GameEvents& events);

    void fixedUpdate(engine::Registry& registry, float dt) override;

    void setPlayer(engine::Entity player) { player_ = player; }
    engine::Entity player() const { return player_; }
    bool consumeStairsRequest();
    void requestStairs() { stairsRequested_ = true; }

private:
    void tickTimers(engine::Registry& registry, float dt);
    void resolveOverlaps(engine::Registry& registry);
    void applyDamage(engine::Registry& registry, engine::Entity target, int damage,
                     float fromX, float fromY, float knockback);
    void handlePickup(engine::Registry& registry, engine::Entity pickup);
    void killEntity(engine::Registry& registry, engine::Entity entity);

    const engine::PhysicsSystem& physics_;
    const GameAssets& assets_;
    GameEvents& events_;
    engine::Entity player_ = engine::kNullEntity;
    bool stairsRequested_ = false;
};
inline constexpr float kPlayerInvulnerability = 0.55f;
inline constexpr float kEnemyInvulnerability = 0.12f;
inline constexpr float kHitFlashDuration = 0.14f;
inline constexpr float kKnockbackStun = 0.12f;

}  
