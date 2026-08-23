#include "Player.h"

#include <algorithm>
#include <cmath>

#include "../combat/CombatSystem.h"

namespace game {

using engine::Animator;
using engine::Collider;
using engine::Entity;
using engine::Health;
using engine::Registry;
using engine::SpriteComponent;
using engine::Transform;
using engine::Velocity;

engine::Entity spawnPlayer(Registry& registry, const GameAssets& assets, float x, float y,
                           int maxHealth) {
    const Entity e = registry.create();
    registry.add<Transform>(e, Transform{x, y});
    registry.add<Velocity>(e, Velocity{});

    Collider collider;
    collider.width = 18.0f;
    collider.height = 18.0f;
    collider.layer = layers::kPlayer;
    collider.mask = layers::kEnemy | layers::kEnemyAttack | layers::kPickup;
    registry.add<Collider>(e, collider);

    registry.add<Health>(e, Health{maxHealth, maxHealth, 0.0f});
    registry.add<PlayerTag>(e, PlayerTag{});
    registry.add<DamageFeedback>(e, DamageFeedback{});

    SpriteComponent sprite;
    sprite.texture = assets.atlas;
    sprite.width = 28.0f;
    sprite.height = 28.0f;
    sprite.layer = engine::kLayerActor;
    sprite.tint = assets.hasAtlas() ? engine::colors::kWhite : palette::kPlayer;
    sprite.offsetY = -4.0f;
    registry.add<SpriteComponent>(e, sprite);

    if (assets.hasAtlas() && assets.playerIdle) {
        Animator animator;
        animator.clip = assets.playerIdle;
        registry.add<Animator>(e, animator);
    }
    return e;
}

PlayerSystem::PlayerSystem(const engine::InputManager& input, const GameAssets& assets,
                           GameEvents& events)
    : input_(input), assets_(assets), events_(events) {}

void PlayerSystem::fixedUpdate(Registry& registry, float dt) {
    if (player_ == engine::kNullEntity || !registry.alive(player_)) return;

    PlayerTag* tag = registry.tryGet<PlayerTag>(player_);
    Velocity* velocity = registry.tryGet<Velocity>(player_);
    Transform* transform = registry.tryGet<Transform>(player_);
    Health* health = registry.tryGet<Health>(player_);
    if (!tag || !velocity || !transform || !health) return;

    tag->attackCooldown = std::max(0.0f, tag->attackCooldown - dt);
    tag->attackTimer = std::max(0.0f, tag->attackTimer - dt);

    if (!controlEnabled_ || !health->alive()) {
        velocity->x = 0.0f;
        velocity->y = 0.0f;
        return;
    }
    DamageFeedback* feedback = registry.tryGet<DamageFeedback>(player_);
    if (feedback && feedback->stunTimer > 0.0f) return;

    float moveX = input_.axis(kMoveLeft, kMoveRight);
    float moveY = input_.axis(kMoveUp, kMoveDown);
    const float length = std::sqrt(moveX * moveX + moveY * moveY);
    if (length > 0.001f) {
        moveX /= length;
        moveY /= length;
        tag->facing = facingFromVector(moveX, moveY, tag->facing);
    }
    const float speed = (tag->attackTimer > 0.0f) ? 0.0f : tag->moveSpeed;
    velocity->x = moveX * speed;
    velocity->y = moveY * speed;

    if (input_.wasPressed(kAttack) && tag->attackCooldown <= 0.0f) {
        tag->attackTimer = kAttackDuration;
        tag->attackCooldown = kAttackCooldown;

        float dirX = 0.0f;
        float dirY = 0.0f;
        facingVector(tag->facing, dirX, dirY);

        // The swing volume is wider across the facing axis than along it, which
        // gives a slight arc feel and makes hitting a moving target fair.
        const float alongX = std::fabs(dirX) > 0.0f ? kAttackReach : 30.0f;
        const float alongY = std::fabs(dirY) > 0.0f ? kAttackReach : 30.0f;

        spawnHitbox(registry, player_, transform->x + dirX * kAttackReach * 0.6f,
                    transform->y + dirY * kAttackReach * 0.6f, alongX, alongY,
                    tag->attackDamage, layers::kPlayerAttack, layers::kEnemy,
                    kAttackDuration, 200.0f);

        GameEvents::raise(events_.onPlayerAttack, transform->x, transform->y, tag->facing);
    }

    if (input_.wasPressed(kInteract)) {
    }
}

void PlayerSystem::update(Registry& registry, float) { chooseAnimation(registry); }

void PlayerSystem::chooseAnimation(Registry& registry) {
    if (player_ == engine::kNullEntity || !registry.alive(player_)) return;

    PlayerTag* tag = registry.tryGet<PlayerTag>(player_);
    Velocity* velocity = registry.tryGet<Velocity>(player_);
    SpriteComponent* sprite = registry.tryGet<SpriteComponent>(player_);
    Health* health = registry.tryGet<Health>(player_);
    if (!tag || !velocity || !sprite || !health) return;

    // Mirror rather than authoring separate left and right art. Halves the
    // sprite count for every character in the game.
    if (tag->facing == Facing::Left) sprite->flipX = true;
    if (tag->facing == Facing::Right) sprite->flipX = false;

    DamageFeedback* feedback = registry.tryGet<DamageFeedback>(player_);
    const bool flashing = feedback && feedback->flashTimer > 0.0f;
    if (!flashing) {
        // Blink while invulnerable so the player can see the i-frames they were
        // given. Silent invulnerability just looks like inconsistent damage.
        const bool invulnerable = health->invulnerableFor > 0.0f;
        const bool blinkOff = invulnerable && std::fmod(health->invulnerableFor, 0.16f) > 0.08f;
        sprite->tint = assets_.hasAtlas() ? engine::colors::kWhite : palette::kPlayer;
        sprite->tint.a = blinkOff ? 90 : 255;
    }

    Animator* animator = registry.tryGet<Animator>(player_);
    if (!animator || !assets_.hasAtlas()) return;

    const bool moving = std::fabs(velocity->x) > 1.0f || std::fabs(velocity->y) > 1.0f;
    if (!health->alive()) {
        engine::playClip(*animator, assets_.playerDeath);
    } else if (tag->attackTimer > 0.0f) {
        engine::playClip(*animator, assets_.playerAttack);
    } else if (moving) {
        engine::playClip(*animator, assets_.playerWalk);
    } else {
        engine::playClip(*animator, assets_.playerIdle);
    }
}

} 
