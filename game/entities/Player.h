#pragma once

#include "../../engine/ecs/System.h"
#include "../../engine/input/InputManager.h"
#include "../GameCommon.h"
#include "../GameEvents.h"

namespace game {

engine::Entity spawnPlayer(engine::Registry& registry, const GameAssets& assets, float x,
                           float y, int maxHealth = 12);
class PlayerSystem final : public engine::System {
public:
    PlayerSystem(const engine::InputManager& input, const GameAssets& assets,
                 GameEvents& events);

    void fixedUpdate(engine::Registry& registry, float dt) override;
    void update(engine::Registry& registry, float dt) override;

    void setPlayer(engine::Entity player) { player_ = player; }
    /// Suspended during menus and after death, so the corpse does not walk.
    void setControlEnabled(bool enabled) { controlEnabled_ = enabled; }

private:
    void chooseAnimation(engine::Registry& registry);

    const engine::InputManager& input_;
    const GameAssets& assets_;
    GameEvents& events_;
    engine::Entity player_ = engine::kNullEntity;
    bool controlEnabled_ = true;
};

inline constexpr float kAttackDuration = 0.18f;
inline constexpr float kAttackCooldown = 0.34f;
inline constexpr float kAttackReach = 34.0f;

}  
