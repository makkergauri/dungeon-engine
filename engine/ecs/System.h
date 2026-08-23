#pragma once

#include <memory>
#include <vector>

#include "Component.h"
#include "Entity.h"

namespace engine {

/// Base class for anything that transforms world state over time.
///
/// Systems get the registry as a parameter rather than storing a reference to
/// it. That sounds like a small thing, but it means a system can be constructed
/// in a unit test and pointed at a throwaway registry with no setup at all.
class System {
public:
    virtual ~System() = default;

    /// Called at the fixed timestep. Anything that affects gameplay -- movement,
    /// collision, AI decisions, damage -- belongs here so behaviour is identical
    /// on a 60Hz laptop and a 240Hz desktop.
    virtual void fixedUpdate(Registry& registry, float dt) { (void)registry; (void)dt; }

    /// Called once per rendered frame. Cosmetic only: animation timers, particle
    /// fade, camera smoothing. Never move anything gameplay-relevant here.
    virtual void update(Registry& registry, float dt) { (void)registry; (void)dt; }

    bool enabled = true;
};

/// Owns a list of systems and runs them in insertion order. Explicit ordering
/// matters more than it looks: input must run before movement, movement before
/// collision resolution, collision before combat reacts to the overlaps.
class SystemScheduler {
public:
    template <typename T, typename... Args>
    T& add(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        systems_.push_back(std::move(system));
        return ref;
    }

    void fixedUpdate(Registry& registry, float dt) {
        for (auto& s : systems_) {
            if (s->enabled) s->fixedUpdate(registry, dt);
        }
    }

    void update(Registry& registry, float dt) {
        for (auto& s : systems_) {
            if (s->enabled) s->update(registry, dt);
        }
    }

    void clear() { systems_.clear(); }

private:
    std::vector<std::unique_ptr<System>> systems_;
};

/// Ages out Lifetime components. Small enough to live with the interface it
/// implements, and every project ends up needing it.
class LifetimeSystem final : public System {
public:
    void fixedUpdate(Registry& registry, float dt) override {
        registry.each<Lifetime>([&](Entity e, Lifetime& life) {
            life.remaining -= dt;
            if (life.remaining <= 0.0f) registry.destroy(e);
        });
    }
};

}  // namespace engine
