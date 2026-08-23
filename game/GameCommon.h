#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "../engine/ecs/Component.h"
#include "../engine/ecs/Entity.h"
#include "../engine/input/InputManager.h"
#include "../engine/rendering/Sprite.h"
#include "ai/Pathfinding.h"

namespace game {
enum Action : engine::ActionId {
    kMoveLeft,
    kMoveRight,
    kMoveUp,
    kMoveDown,
    kAttack,
    kInteract,
    kPause,
    kConfirm,
    kCancel,
    kToggleDebug,
};

namespace layers {
inline constexpr std::uint32_t kPlayer = 1u << 0;
inline constexpr std::uint32_t kEnemy = 1u << 1;
inline constexpr std::uint32_t kPlayerAttack = 1u << 2;
inline constexpr std::uint32_t kEnemyAttack = 1u << 3;
inline constexpr std::uint32_t kPickup = 1u << 4;
}  // namespace layers

enum class Facing { Down, Up, Left, Right };

/// Unit direction vector for a facing, used by attacks and sprite selection.
void facingVector(Facing facing, float& outX, float& outY);
Facing facingFromVector(float x, float y, Facing fallback);


// Game components


enum class ItemType { HealthPotion, WeaponUpgrade, ArmourScrap, Coin };

struct ItemPickup {
    ItemType type = ItemType::HealthPotion;
    int amount = 1;
    float bobPhase = 0.0f;  // so a row of pickups does not bob in lockstep
};

struct InventorySlot {
    ItemType type = ItemType::HealthPotion;
    int count = 0;
    bool empty() const { return count <= 0; }
};

struct PlayerTag {
    Facing facing = Facing::Down;
    float moveSpeed = 175.0f;
    float attackCooldown = 0.0f;
    /// How long the current swing has left. Non-zero means the player is locked
    /// into the attack animation.
    float attackTimer = 0.0f;
    int attackDamage = 3;
    int coins = 0;
    std::array<InventorySlot, 4> inventory{};
};

enum class EnemyKind {
    /// Slow, tanky, walks straight at you. The baseline threat.
    Shambler,
    /// Stationary, fires projectiles when it has line of sight. Punishes
    /// standing still in the open.
    Sentinel,
    /// Fast and erratic, closes distance in bursts. Forces movement.
    Skitterer,
};

struct EnemyTag {
    EnemyKind kind = EnemyKind::Shambler;
    float moveSpeed = 60.0f;
    int contactDamage = 1;
    float attackRange = 28.0f;
    float attackCooldown = 0.0f;
    float attackInterval = 1.2f;
    int scoreValue = 10;
};

/// Finite state machine plus its pathfinding scratch space.
struct AIState {
    enum class State { Idle, Chase, Attack, Dead };

    State state = State::Idle;
    float stateTimer = 0.0f;

    /// Distance at which an idle enemy notices the player.
    float aggroRange = 260.0f;
    /// Once aggroed, enemies keep chasing past this to avoid the classic
    /// "enemy loses interest one pixel outside its radius" flip-flopping.
    float loseInterestRange = 420.0f;

    std::vector<GridPoint> path;
    std::size_t pathIndex = 0;
    float repathTimer = 0.0f;
    /// Staggered per enemy so twenty of them do not all run A* on the same frame.
    float repathInterval = 0.45f;

    /// Erratic movement for the Skitterer: a wander offset refreshed on a timer.
    float wanderX = 0.0f;
    float wanderY = 0.0f;
    float wanderTimer = 0.0f;
};

/// A short-lived damage volume: a sword swing, a projectile's tip.
struct Hitbox {
    engine::Entity owner = engine::kNullEntity;
    int damage = 1;
    float knockback = 180.0f;
    /// Hitboxes persist for a few frames so they cannot be dodged by frame
    /// timing, which means they would otherwise apply damage every step.
    bool consumed = false;
};

/// Marks an entity as a projectile so the AI system can despawn it on wall hit.
struct Projectile {
    float lifetime = 3.0f;
};

/// Drives the hit flash and knockback recovery.
struct DamageFeedback {
    float flashTimer = 0.0f;
    float stunTimer = 0.0f;
};

/// Attached to the stairs so the interact check knows what it is standing on.
struct StairsTrigger {};

/// The shopkeeper. Has no collider -- proximity is checked directly, because a
/// trigger volume would also have to be excluded from every combat query.
struct MerchantTag {
    float bobPhase = 0.0f;
};

/// Purely decorative scenery: mushrooms, rubble, stalagmites, wall torches.
/// Carries no collider and no behaviour, so it costs a sprite and nothing else.
struct DecorTag {
    float flickerPhase = 0.0f;
};


// Assets

struct GameAssets {
    engine::TextureId atlas = engine::kNoTexture;
    bool hasAtlas() const { return atlas != engine::kNoTexture; }

    const engine::AnimationClip* playerIdle = nullptr;
    const engine::AnimationClip* playerWalk = nullptr;
    const engine::AnimationClip* playerAttack = nullptr;
    const engine::AnimationClip* playerDeath = nullptr;

    const engine::AnimationClip* shamblerWalk = nullptr;
    const engine::AnimationClip* sentinelIdle = nullptr;
    const engine::AnimationClip* skittererWalk = nullptr;
    const engine::AnimationClip* enemyDeath = nullptr;
    const engine::AnimationClip* coinSpin = nullptr;
    const engine::AnimationClip* torchFlicker = nullptr;

    /// Four floor variants, chosen per tile by a coordinate hash so the ground
    /// does not read as graph paper.
    engine::TextureRect tileFloor[4]{};
    engine::TextureRect tileWall{};
    engine::TextureRect tileWallFace{};
    engine::TextureRect tileWallFaceLit{};
    engine::TextureRect tileStairs{};
    /// Mushroom, rock, bones, stalagmite, crystal.
    engine::TextureRect decor[5]{};
    engine::TextureRect merchant{};
    engine::TextureRect itemShield{};
    engine::TextureRect itemPotion{};
    engine::TextureRect itemWeapon{};
    engine::TextureRect itemArmour{};
    engine::TextureRect itemCoin{};
    engine::TextureRect projectile{};
};

/// Flat colours used when the atlas is absent. Also used for particles and the
/// HUD regardless, so they live here rather than being buried in the fallback.
namespace palette {
inline constexpr engine::Color kPlayer{95, 190, 235, 255};
inline constexpr engine::Color kShambler{170, 90, 90, 255};
inline constexpr engine::Color kSentinel{200, 150, 60, 255};
inline constexpr engine::Color kSkitterer{190, 90, 190, 255};
inline constexpr engine::Color kProjectile{240, 190, 90, 255};
inline constexpr engine::Color kFloor{58, 54, 68, 255};
inline constexpr engine::Color kWall{32, 30, 40, 255};
inline constexpr engine::Color kStairs{120, 200, 140, 255};
inline constexpr engine::Color kBlood{160, 40, 45, 255};
inline constexpr engine::Color kUiPanel{20, 18, 26, 220};
inline constexpr engine::Color kUiText{225, 220, 210, 255};
inline constexpr engine::Color kUiDim{140, 134, 128, 255};
inline constexpr engine::Color kGold{235, 196, 92, 255};
inline constexpr engine::Color kMerchant{160, 138, 220, 255};
}  

std::string itemName(ItemType type);
engine::Color itemColor(ItemType type);

}  