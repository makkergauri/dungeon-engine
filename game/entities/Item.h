#pragma once

#include "../../engine/ecs/System.h"
#include "../GameCommon.h"

namespace game {

engine::Entity spawnItem(engine::Registry& registry, const GameAssets& assets, ItemType type,
                         float x, float y, int amount = 1);

/// Cosmetic bob for pickups.
///
/// Purely a render-rate system: it writes to the sprite offset rather than the
/// transform, so bobbing never moves the pickup's collider and the item cannot
/// bob out of the player's reach.
class ItemSystem final : public engine::System {
public:
    void update(engine::Registry& registry, float dt) override;

private:
    float elapsed_ = 0.0f;
};

}  // namespace game
