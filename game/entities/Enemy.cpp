#include "Enemy.h"

#include <algorithm>
#include <cmath>

#include "../combat/CombatSystem.h"

namespace game {

using engine::Animator;
using engine::Collider;
using engine::Entity;
using engine::Health;
using engine::Lifetime;
using engine::Registry;
using engine::SpriteComponent;
using engine::Transform;
using engine::Velocity;

namespace {

struct EnemyProfile {
    float speed;
    int health;
    int contactDamage;
    float attackRange;
    float attackInterval;
    float aggroRange;
    float colliderSize;
    int score;
    engine::Color color;
};

EnemyProfile profileFor(EnemyKind kind) {
    switch (kind) {
        case EnemyKind::Shambler:
            return {58.0f, 8, 2, 26.0f, 1.0f, 280.0f, 22.0f, 10, palette::kShambler};
        case EnemyKind::Sentinel:
            // Stationary, so it can afford far more range and a lot less health.
            return {0.0f, 5, 1, 340.0f, 1.6f, 340.0f, 22.0f, 15, palette::kSentinel};
        case EnemyKind::Skitterer:
            return {138.0f, 4, 1, 24.0f, 0.7f, 240.0f, 16.0f, 12, palette::kSkitterer};
    }
    return {60.0f, 5, 1, 26.0f, 1.0f, 260.0f, 20.0f, 10, palette::kShambler};
}

float distance(float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
}

}  

engine::Entity spawnEnemy(Registry& registry, const GameAssets& assets, EnemyKind kind,
                          float x, float y, int floorNumber) {
    const EnemyProfile profile = profileFor(kind);
    const Entity e = registry.create();

    registry.add<Transform>(e, Transform{x, y});
    registry.add<Velocity>(e, Velocity{});

    Collider collider;
    collider.width = profile.colliderSize;
    collider.height = profile.colliderSize;
    collider.layer = layers::kEnemy;
    collider.mask = layers::kPlayer | layers::kPlayerAttack;
    registry.add<Collider>(e, collider);

    const int bonusHealth = std::max(0, (floorNumber - 1) * 2);
    registry.add<Health>(e, Health{profile.health + bonusHealth,
                                   profile.health + bonusHealth, 0.0f});

    EnemyTag tag;
    tag.kind = kind;
    tag.moveSpeed = profile.speed;
    tag.contactDamage = profile.contactDamage;
    tag.attackRange = profile.attackRange;
    tag.attackInterval = profile.attackInterval;
    tag.attackCooldown = 0.0f;
    tag.scoreValue = profile.score;
    registry.add<EnemyTag>(e, tag);

    AIState ai;
    ai.aggroRange = profile.aggroRange;
    ai.loseInterestRange = profile.aggroRange * 1.6f;
    
    ai.repathTimer = static_cast<float>(e % 27) * 0.017f;
    ai.repathInterval = (kind == EnemyKind::Skitterer) ? 0.3f : 0.5f;
    registry.add<AIState>(e, ai);

    registry.add<DamageFeedback>(e, DamageFeedback{});

    SpriteComponent sprite;
    sprite.texture = assets.atlas;
    sprite.width = profile.colliderSize + 8.0f;
    sprite.height = profile.colliderSize + 8.0f;
    sprite.layer = engine::kLayerActor;
    sprite.tint = assets.hasAtlas() ? engine::colors::kWhite : profile.color;
    registry.add<SpriteComponent>(e, sprite);

    if (assets.hasAtlas()) {
        const engine::AnimationClip* clip = nullptr;
        switch (kind) {
            case EnemyKind::Shambler:  clip = assets.shamblerWalk; break;
            case EnemyKind::Sentinel:  clip = assets.sentinelIdle; break;
            case EnemyKind::Skitterer: clip = assets.skittererWalk; break;
        }
        if (clip) {
            Animator animator;
            animator.clip = clip;
            // Random starting frame so a pack of identical enemies does not
            // animate as one organism.
            animator.frame = static_cast<int>(e % std::max<std::size_t>(1, clip->frames.size()));
            registry.add<Animator>(e, animator);
        }
    }
    return e;
}


// EnemyAISystem


EnemyAISystem::EnemyAISystem(const GameAssets& assets, GameEvents& events)
    : assets_(assets), events_(events) {}

void EnemyAISystem::setLevel(const Dungeon* dungeon, NavGrid navGrid) {
    dungeon_ = dungeon;
    navGrid_ = std::move(navGrid);
}

bool EnemyAISystem::playerPosition(Registry& registry, float& outX, float& outY) const {
    if (player_ == engine::kNullEntity || !registry.alive(player_)) return false;
    Transform* transform = registry.tryGet<Transform>(player_);
    Health* health = registry.tryGet<Health>(player_);
    if (!transform || !health || !health->alive()) return false;
    outX = transform->x;
    outY = transform->y;
    return true;
}

bool EnemyAISystem::hasLineOfSight(float ax, float ay, float bx, float by) const {
    if (!dungeon_) return true;
    const GridPoint from{dungeon_->tileXAt(ax), dungeon_->tileYAt(ay)};
    const GridPoint to{dungeon_->tileXAt(bx), dungeon_->tileYAt(by)};
    return Pathfinder::lineOfSight(navGrid_, from, to);
}

void EnemyAISystem::steerTowardsPlayer(Registry& registry, Entity self, AIState& ai, float dt,
                                       float& outX, float& outY) {
    outX = 0.0f;
    outY = 0.0f;

    float playerX = 0.0f;
    float playerY = 0.0f;
    if (!dungeon_ || !playerPosition(registry, playerX, playerY)) return;

    Transform* transform = registry.tryGet<Transform>(self);
    if (!transform) return;

    // Straight line first. If nothing is in the way there is no reason to run a
    // graph search, and this is by far the most common case in an open room.
    if (hasLineOfSight(transform->x, transform->y, playerX, playerY)) {
        ai.path.clear();
        const float dx = playerX - transform->x;
        const float dy = playerY - transform->y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length > 0.001f) {
            outX = dx / length;
            outY = dy / length;
        }
        return;
    }

    ai.repathTimer -= dt;
    const bool needsPath = ai.path.empty() || ai.pathIndex >= ai.path.size();
    if ((ai.repathTimer <= 0.0f || needsPath) && searchesThisFrame_ < kMaxSearchesPerStep) {
        const GridPoint start{dungeon_->tileXAt(transform->x), dungeon_->tileYAt(transform->y)};
        const GridPoint goal{dungeon_->tileXAt(playerX), dungeon_->tileYAt(playerY)};

        std::vector<GridPoint> path;
        if (pathfinder_.findPath(navGrid_, start, goal, path)) {
            // Trim the staircase out of the raw grid path so the enemy walks a
            // natural line instead of visibly stepping cell to cell.
            Pathfinder::simplify(navGrid_, path);
            ai.path = std::move(path);
            // Skip the tile we are standing on, otherwise the first waypoint is
            // reached instantly and the enemy jitters in place.
            ai.pathIndex = ai.path.size() > 1 ? 1 : 0;
        } else {
            ai.path.clear();
            ai.pathIndex = 0;
        }
        ai.repathTimer = ai.repathInterval;
        ++searchesThisFrame_;
    }

    if (ai.path.empty() || ai.pathIndex >= ai.path.size()) return;

    const GridPoint waypoint = ai.path[ai.pathIndex];
    const float targetX = dungeon_->worldX(waypoint.x);
    const float targetY = dungeon_->worldY(waypoint.y);
    const float dx = targetX - transform->x;
    const float dy = targetY - transform->y;
    const float length = std::sqrt(dx * dx + dy * dy);

    // Advance when close enough. The threshold is deliberately larger than the
    // step size so an enemy that overshoots slightly still counts as arrived.
    if (length < dungeon_->tileSize() * 0.4f) {
        ++ai.pathIndex;
        return;
    }
    if (length > 0.001f) {
        outX = dx / length;
        outY = dy / length;
    }
}

void EnemyAISystem::updateShambler(Registry& registry, Entity self, AIState& ai,
                                   EnemyTag& enemy, float dt) {
    Velocity* velocity = registry.tryGet<Velocity>(self);
    if (!velocity) return;

    float dirX = 0.0f;
    float dirY = 0.0f;
    steerTowardsPlayer(registry, self, ai, dt, dirX, dirY);
    velocity->x = dirX * enemy.moveSpeed;
    velocity->y = dirY * enemy.moveSpeed;
}

void EnemyAISystem::updateSentinel(Registry& registry, Entity self, AIState& ai,
                                   EnemyTag& enemy, float dt) {
    (void)ai;
    Transform* transform = registry.tryGet<Transform>(self);
    if (!transform) return;

    float playerX = 0.0f;
    float playerY = 0.0f;
    if (!playerPosition(registry, playerX, playerY)) return;

    // Only fires when it can actually see the player. Shooting through walls
    // would be unreadable and deeply unfair; needing line of sight makes cover
    // a real tactic.
    if (!hasLineOfSight(transform->x, transform->y, playerX, playerY)) return;
    if (distance(transform->x, transform->y, playerX, playerY) > enemy.attackRange) return;

    enemy.attackCooldown -= dt;
    if (enemy.attackCooldown > 0.0f) return;
    enemy.attackCooldown = enemy.attackInterval;

    const float dx = playerX - transform->x;
    const float dy = playerY - transform->y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f) return;

    constexpr float kProjectileSpeed = 230.0f;
    const Entity shot = spawnHitbox(registry, self, transform->x, transform->y, 12.0f, 12.0f,
                                    enemy.contactDamage, layers::kEnemyAttack,
                                    layers::kPlayer, 4.0f, 140.0f);
    registry.add<Velocity>(shot, Velocity{dx / length * kProjectileSpeed,
                                          dy / length * kProjectileSpeed});
    registry.add<Projectile>(shot, Projectile{4.0f});

    SpriteComponent sprite;
    sprite.texture = assets_.atlas;
    sprite.source = assets_.hasAtlas() ? assets_.projectile : engine::TextureRect{};
    sprite.width = 12.0f;
    sprite.height = 12.0f;
    sprite.layer = engine::kLayerParticle;
    sprite.tint = assets_.hasAtlas() ? engine::colors::kWhite : palette::kProjectile;
    registry.add<SpriteComponent>(shot, sprite);
}

void EnemyAISystem::updateSkitterer(Registry& registry, Entity self, AIState& ai,
                                    EnemyTag& enemy, float dt) {
    Velocity* velocity = registry.tryGet<Velocity>(self);
    if (!velocity) return;

    float dirX = 0.0f;
    float dirY = 0.0f;
    steerTowardsPlayer(registry, self, ai, dt, dirX, dirY);

    // Erratic movement, done as a slowly-rotating offset rather than fresh
    // randomness per frame. Per-frame noise averages out to nothing and just
    // makes the sprite vibrate; a wander vector that persists for a while
    // actually curves the approach.
    ai.wanderTimer -= dt;
    if (ai.wanderTimer <= 0.0f) {
        std::uniform_real_distribution<float> spread(-0.85f, 0.85f);
        ai.wanderX = spread(rng_);
        ai.wanderY = spread(rng_);
        std::uniform_real_distribution<float> interval(0.18f, 0.45f);
        ai.wanderTimer = interval(rng_);
    }

    float vx = dirX + ai.wanderX * 0.6f;
    float vy = dirY + ai.wanderY * 0.6f;
    const float length = std::sqrt(vx * vx + vy * vy);
    if (length > 0.001f) {
        vx /= length;
        vy /= length;
    }

    // Move in bursts: a sine on the state timer gives bursts of speed with brief
    // pauses, which is what makes it feel like a different creature rather than
    // a fast Shambler.
    const float burst = 0.55f + 0.45f * std::sin(ai.stateTimer * 6.0f);
    velocity->x = vx * enemy.moveSpeed * burst;
    velocity->y = vy * enemy.moveSpeed * burst;
}

void EnemyAISystem::fixedUpdate(Registry& registry, float dt) {
    searchesThisFrame_ = 0;

    // Projectiles are triggers, so physics lets them pass through walls. Cull
    // them here where the level geometry is available.
    registry.each<Projectile, Transform>([&](Entity e, Projectile& projectile, Transform& t) {
        projectile.lifetime -= dt;
        const bool hitWall =
            dungeon_ && dungeon_->isSolid(dungeon_->tileXAt(t.x), dungeon_->tileYAt(t.y));
        if (projectile.lifetime <= 0.0f || hitWall) registry.destroy(e);
    });

    if (!aiEnabled_) return;

    float playerX = 0.0f;
    float playerY = 0.0f;
    const bool playerAlive = playerPosition(registry, playerX, playerY);

    registry.each<AIState, EnemyTag, Transform>(
        [&](Entity self, AIState& ai, EnemyTag& enemy, Transform& transform) {
            if (ai.state == AIState::State::Dead) return;
            ai.stateTimer += dt;

            Velocity* velocity = registry.tryGet<Velocity>(self);
            DamageFeedback* feedback = registry.tryGet<DamageFeedback>(self);

            // Knockback wins over the AI's intentions for a moment after a hit,
            // so the player can see their attack physically move the enemy.
            if (feedback && feedback->stunTimer > 0.0f) return;

            if (!playerAlive) {
                ai.state = AIState::State::Idle;
                if (velocity) {
                    velocity->x = 0.0f;
                    velocity->y = 0.0f;
                }
                return;
            }

            const float toPlayer = distance(transform.x, transform.y, playerX, playerY);

            // --- State transitions ---
            switch (ai.state) {
                case AIState::State::Idle:
                    if (toPlayer <= ai.aggroRange &&
                        hasLineOfSight(transform.x, transform.y, playerX, playerY)) {
                        ai.state = AIState::State::Chase;
                        ai.stateTimer = 0.0f;
                        ai.repathTimer = 0.0f;  // path immediately on waking up
                    }
                    break;

                case AIState::State::Chase:
                    if (toPlayer > ai.loseInterestRange) {
                        ai.state = AIState::State::Idle;
                        ai.path.clear();
                    } else if (toPlayer <= enemy.attackRange) {
                        ai.state = AIState::State::Attack;
                        ai.stateTimer = 0.0f;
                    }
                    break;

                case AIState::State::Attack:
                    // Hysteresis on the way out: leaving attack range by a
                    // pixel should not immediately flip the state back, or an
                    // enemy hovering at exactly attack range oscillates.
                    if (toPlayer > enemy.attackRange * 1.35f) {
                        ai.state = AIState::State::Chase;
                        ai.stateTimer = 0.0f;
                    }
                    break;

                case AIState::State::Dead:
                    return;
            }

            // --- Per-type behaviour ---
            if (ai.state == AIState::State::Idle && enemy.kind != EnemyKind::Sentinel) {
                if (velocity) {
                    velocity->x = 0.0f;
                    velocity->y = 0.0f;
                }
                return;
            }

            switch (enemy.kind) {
                case EnemyKind::Shambler:  updateShambler(registry, self, ai, enemy, dt); break;
                case EnemyKind::Sentinel:  updateSentinel(registry, self, ai, enemy, dt); break;
                case EnemyKind::Skitterer: updateSkitterer(registry, self, ai, enemy, dt); break;
            }

            // Contact damage is resolved by CombatSystem from the physics
            // overlap; the Attack state exists so animation and the melee types'
            // approach behaviour can differ, not to apply damage here.
        });
}

void EnemyAISystem::update(Registry& registry, float) {
    // Face the direction of travel and clear the hit flash.
    registry.each<EnemyTag, SpriteComponent, Velocity>(
        [&](Entity e, EnemyTag& enemy, SpriteComponent& sprite, Velocity& velocity) {
            if (std::fabs(velocity.x) > 1.0f) sprite.flipX = velocity.x < 0.0f;

            DamageFeedback* feedback = registry.tryGet<DamageFeedback>(e);
            if (feedback && feedback->flashTimer > 0.0f) return;  // combat owns the tint

            if (assets_.hasAtlas()) {
                sprite.tint = engine::colors::kWhite;
            } else {
                sprite.tint = profileFor(enemy.kind).color;
            }
        });
}

}  // namespace game
