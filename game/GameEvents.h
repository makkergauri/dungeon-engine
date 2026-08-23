#pragma once

#include <functional>

#include "GameCommon.h"

namespace game {

struct GameEvents {
    std::function<void(float x, float y, int damage)> onDamageDealt;
    std::function<void(float x, float y, EnemyKind kind)> onEnemyKilled;
    std::function<void(float x, float y, Facing facing)> onPlayerAttack;
    std::function<void(float x, float y)> onPlayerHurt;
    std::function<void(float x, float y, ItemType type)> onPickup;
    std::function<void()> onPlayerDeath;
    std::function<void()> onStairsReached;


    template <typename Fn, typename... Args>
    static void raise(const Fn& callback, Args&&... args) {
        if (callback) callback(std::forward<Args>(args)...);
    }
};

}  
