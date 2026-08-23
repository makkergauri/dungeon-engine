#include "Item.h"

#include <cmath>

namespace game {

using engine::Collider;
using engine::Entity;
using engine::Registry;
using engine::SpriteComponent;
using engine::Transform;

engine::Entity spawnItem(Registry& registry, const GameAssets& assets, ItemType type, float x,
                         float y, int amount) {
    const Entity e = registry.create();
    registry.add<Transform>(e, Transform{x, y});

    Collider collider;
    collider.width = 26.0f;
    collider.height = 26.0f;
    collider.isTrigger = true;
    collider.layer = layers::kPickup;
    collider.mask = layers::kPlayer;
    registry.add<Collider>(e, collider);

    ItemPickup pickup;
    pickup.type = type;
    pickup.amount = amount;
    pickup.bobPhase = static_cast<float>(e % 32) * 0.2f;
    registry.add<ItemPickup>(e, pickup);

    SpriteComponent sprite;
    sprite.texture = assets.atlas;
    sprite.width = 16.0f;
    sprite.height = 16.0f;
    sprite.layer = engine::kLayerItem;
    sprite.tint = assets.hasAtlas() ? engine::colors::kWhite : itemColor(type);
    if (assets.hasAtlas()) {
        switch (type) {
            case ItemType::HealthPotion:  sprite.source = assets.itemPotion; break;
            case ItemType::WeaponUpgrade: sprite.source = assets.itemWeapon; break;
            case ItemType::ArmourScrap:   sprite.source = assets.itemArmour; break;
            case ItemType::Coin:          sprite.source = assets.itemCoin;   break;
        }
    }
    registry.add<SpriteComponent>(e, sprite);

    if (assets.hasAtlas() && type == ItemType::Coin && assets.coinSpin) {
        engine::Animator animator;
        animator.clip = assets.coinSpin;
        // Desynchronise so a pile of coins does not flash as one object.
        animator.frame = static_cast<int>(e % 4);
        registry.add<engine::Animator>(e, animator);
    }
    return e;
}

void ItemSystem::update(Registry& registry, float dt) {
    elapsed_ += dt;
    registry.each<ItemPickup, SpriteComponent>(
        [&](Entity, ItemPickup& pickup, SpriteComponent& sprite) {
            sprite.offsetY = std::sin(elapsed_ * 3.2f + pickup.bobPhase) * 3.0f - 4.0f;
        });
}

}  