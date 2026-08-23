#include "CombatSystem.h"

#include <algorithm>
#include <cmath>

namespace game {

using engine::Collider;
using engine::Entity;
using engine::Health;
using engine::Lifetime;
using engine::Registry;
using engine::SpriteComponent;
using engine::Transform;
using engine::Velocity;

engine::Entity spawnHitbox(Registry& registry, Entity owner, float x, float y, float width,
                           float height, int damage, std::uint32_t layer,
                           std::uint32_t mask, float lifetime, float knockback) {
    const Entity e = registry.create();
    registry.add<Transform>(e, Transform{x, y});

    Collider collider;
    collider.width = width;
    collider.height = height;
    collider.isTrigger = true;  // damage volumes never block movement
    collider.layer = layer;
    collider.mask = mask;
    registry.add<Collider>(e, collider);

    Hitbox hitbox;
    hitbox.owner = owner;
    hitbox.damage = damage;
    hitbox.knockback = knockback;
    registry.add<Hitbox>(e, hitbox);

    registry.add<Lifetime>(e, Lifetime{lifetime});
    return e;
}

CombatSystem::CombatSystem(const engine::PhysicsSystem& physics, const GameAssets& assets,
                           GameEvents& events)
    : physics_(physics), assets_(assets), events_(events) {}

bool CombatSystem::consumeStairsRequest() {
    const bool requested = stairsRequested_;
    stairsRequested_ = false;
    return requested;
}

void CombatSystem::fixedUpdate(Registry& registry, float dt) {
    tickTimers(registry, dt);
    resolveOverlaps(registry);
}

void CombatSystem::tickTimers(Registry& registry, float dt) {
    registry.each<Health>([&](Entity, Health& health) {
        health.invulnerableFor = std::max(0.0f, health.invulnerableFor - dt);
    });

    registry.each<DamageFeedback, SpriteComponent>(
        [&](Entity, DamageFeedback& feedback, SpriteComponent& sprite) {
            feedback.flashTimer = std::max(0.0f, feedback.flashTimer - dt);
            feedback.stunTimer = std::max(0.0f, feedback.stunTimer - dt);

           
            if (feedback.flashTimer > 0.0f) {
                sprite.tint = engine::Color{255, 255, 255, 255};
            }
        });
}

void CombatSystem::resolveOverlaps(Registry& registry) {
    for (const engine::OverlapEvent& event : physics_.overlapEvents()) {
        for (int swapped = 0; swapped < 2; ++swapped) {
            const Entity a = swapped ? event.b : event.a;
            const Entity b = swapped ? event.a : event.b;
            if (!registry.alive(a) || !registry.alive(b)) continue;

            // --- Damage volume hits something with health ---
            if (Hitbox* hitbox = registry.tryGet<Hitbox>(a)) {
                Health* health = registry.tryGet<Health>(b);
                if (!health || !health->alive()) continue;
                if (b == hitbox->owner) continue;  // never hit yourself

                Transform* source = registry.tryGet<Transform>(a);
                const float fromX = source ? source->x : 0.0f;
                const float fromY = source ? source->y : 0.0f;
                applyDamage(registry, b, hitbox->damage, fromX, fromY, hitbox->knockback);

                // Projectiles die on impact; a sword swing keeps going so it can
                // cleave through a second enemy standing behind the first.
                if (registry.has<Projectile>(a)) registry.destroy(a);
                continue;
            }

            // --- Enemy body touching the player: contact damage ---
            if (registry.has<EnemyTag>(a) && b == player_) {
                const EnemyTag& enemy = registry.get<EnemyTag>(a);
                AIState* ai = registry.tryGet<AIState>(a);
                if (ai && ai->state == AIState::State::Dead) continue;

                Transform* source = registry.tryGet<Transform>(a);
                applyDamage(registry, b, enemy.contactDamage, source ? source->x : 0.0f,
                            source ? source->y : 0.0f, 220.0f);
                continue;
            }

            // --- Player walking over a pickup ---
            if (a == player_ && registry.has<ItemPickup>(b)) {
                handlePickup(registry, b);
                continue;
            }

            // --- Player standing on the stairs ---
            if (a == player_ && registry.has<StairsTrigger>(b)) {
                requestStairs();
                continue;
            }
        }
    }
}

void CombatSystem::applyDamage(Registry& registry, Entity target, int damage, float fromX,
                               float fromY, float knockback) {
    Health* health = registry.tryGet<Health>(target);
    if (!health || !health->alive()) return;
    if (health->invulnerableFor > 0.0f) return;

    health->current -= damage;
    health->invulnerableFor =
        (target == player_) ? kPlayerInvulnerability : kEnemyInvulnerability;

    if (DamageFeedback* feedback = registry.tryGet<DamageFeedback>(target)) {
        feedback->flashTimer = kHitFlashDuration;
        feedback->stunTimer = kKnockbackStun;
    }

    // Knockback, pushed away from whatever dealt the damage.
    Transform* transform = registry.tryGet<Transform>(target);
    Velocity* velocity = registry.tryGet<Velocity>(target);
    if (transform && velocity && knockback > 0.0f) {
        float dx = transform->x - fromX;
        float dy = transform->y - fromY;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 0.001f) {
            velocity->x = dx / length * knockback;
            velocity->y = dy / length * knockback;
        } else {
            // Perfectly overlapping. Picking a fixed direction beats dividing by
            // something near zero and launching the target across the map.
            velocity->y = -knockback;
        }
    }

    const float x = transform ? transform->x : 0.0f;
    const float y = transform ? transform->y : 0.0f;

    if (target == player_) {
        GameEvents::raise(events_.onPlayerHurt, x, y);
    } else {
        GameEvents::raise(events_.onDamageDealt, x, y, damage);
    }

    if (!health->alive()) killEntity(registry, target);
}

void CombatSystem::handlePickup(Registry& registry, Entity pickup) {
    ItemPickup* item = registry.tryGet<ItemPickup>(pickup);
    PlayerTag* tag = registry.tryGet<PlayerTag>(player_);
    Health* health = registry.tryGet<Health>(player_);
    if (!item || !tag || !health) return;

    switch (item->type) {
        case ItemType::HealthPotion:
            // Heal immediately when hurt, otherwise bank it. Forcing the player
            // to open an inventory mid-fight to use the potion they just walked
            // over is friction with no payoff.
            if (health->current < health->max) {
                health->current = std::min(health->max, health->current + item->amount * 3);
            } else {
                for (InventorySlot& slot : tag->inventory) {
                    if (slot.empty() || slot.type == ItemType::HealthPotion) {
                        slot.type = ItemType::HealthPotion;
                        slot.count += item->amount;
                        break;
                    }
                }
            }
            break;

        case ItemType::WeaponUpgrade:
            tag->attackDamage += item->amount;
            break;

        case ItemType::ArmourScrap:
            health->max += item->amount;
            health->current += item->amount;
            break;

        case ItemType::Coin:
            tag->coins += item->amount;
            break;
    }

    Transform* transform = registry.tryGet<Transform>(pickup);
    GameEvents::raise(events_.onPickup, transform ? transform->x : 0.0f,
                      transform ? transform->y : 0.0f, item->type);
    registry.destroy(pickup);
}

void CombatSystem::killEntity(Registry& registry, Entity entity) {
    Transform* transform = registry.tryGet<Transform>(entity);
    const float x = transform ? transform->x : 0.0f;
    const float y = transform ? transform->y : 0.0f;

    if (entity == player_) {
        GameEvents::raise(events_.onPlayerDeath);
        // The player entity is left alive so the death animation can play and
        // the camera keeps something to follow. The state machine takes over.
        if (AIState* ai = registry.tryGet<AIState>(entity)) ai->state = AIState::State::Dead;
        if (Velocity* velocity = registry.tryGet<Velocity>(entity)) {
            velocity->x = 0.0f;
            velocity->y = 0.0f;
        }
        return;
    }

    if (EnemyTag* enemy = registry.tryGet<EnemyTag>(entity)) {
        GameEvents::raise(events_.onEnemyKilled, x, y, enemy->kind);

        // Stop the corpse from fighting back or blocking the doorway it died in,
        // then let it fade rather than vanishing on the frame it dies.
        if (AIState* ai = registry.tryGet<AIState>(entity)) ai->state = AIState::State::Dead;
        registry.remove<EnemyTag>(entity);
        registry.remove<Collider>(entity);
        registry.remove<Velocity>(entity);

        if (SpriteComponent* sprite = registry.tryGet<SpriteComponent>(entity)) {
            sprite->layer = engine::kLayerDecal;  // corpses lie under the living
            sprite->tint = engine::Color{140, 140, 150, 200};
            if (assets_.hasAtlas() && assets_.enemyDeath) {
                if (engine::Animator* animator = registry.tryGet<engine::Animator>(entity)) {
                    engine::playClip(*animator, assets_.enemyDeath);
                }
            }
        }
        registry.add<Lifetime>(entity, Lifetime{2.5f});
        return;
    }

    registry.destroy(entity);
}

}  
