#pragma once

#include <random>

#include "../../engine/ecs/System.h"
#include "../GameCommon.h"
#include "../GameEvents.h"
#include "../ai/Pathfinding.h"
#include "../generation/DungeonGenerator.h"

namespace game {

engine::Entity spawnEnemy(engine::Registry& registry, const GameAssets& assets,
                          EnemyKind kind, float x, float y, int floorNumber);

/// Enemy AI: a finite state machine per enemy, with A* underneath the chase.
class EnemyAISystem final : public engine::System {
public:
    EnemyAISystem(const GameAssets& assets, GameEvents& events);

    void fixedUpdate(engine::Registry& registry, float dt) override;
    void update(engine::Registry& registry, float dt) override;
    void setLevel(const Dungeon* dungeon, NavGrid navGrid);
    void setPlayer(engine::Entity player) { player_ = player; }
    void setEnabled(bool enabled) { aiEnabled_ = enabled; }
    int lastFrameSearches() const { return searchesThisFrame_; }

private:
    void updateShambler(engine::Registry& registry, engine::Entity self, AIState& ai,
                        EnemyTag& enemy, float dt);
    void updateSentinel(engine::Registry& registry, engine::Entity self, AIState& ai,
                        EnemyTag& enemy, float dt);
    void updateSkitterer(engine::Registry& registry, engine::Entity self, AIState& ai,
                         EnemyTag& enemy, float dt);
    void steerTowardsPlayer(engine::Registry& registry, engine::Entity self, AIState& ai,
                            float dt, float& outX, float& outY);

    bool playerPosition(engine::Registry& registry, float& outX, float& outY) const;
    bool hasLineOfSight(float ax, float ay, float bx, float by) const;

    const GameAssets& assets_;
    GameEvents& events_;
    const Dungeon* dungeon_ = nullptr;
    NavGrid navGrid_;
    Pathfinder pathfinder_;
    engine::Entity player_ = engine::kNullEntity;
    bool aiEnabled_ = true;
    std::mt19937 rng_{0xC0FFEE};

    int searchesThisFrame_ = 0;
    static constexpr int kMaxSearchesPerStep = 4;
};

} 
