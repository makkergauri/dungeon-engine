#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine {

/// An entity is nothing but an ID. All of its data lives in component pools,
/// which means "what an entity is" is decided at runtime by which components it
/// happens to own, rather than at compile time by a class hierarchy.
using Entity = std::uint32_t;

/// 0 is reserved so that a default-constructed Entity is obviously invalid.
inline constexpr Entity kNullEntity = 0;

// -----------------------------------------------------------------------------
// Component storage
// -----------------------------------------------------------------------------

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
    virtual std::size_t size() const = 0;
};

/// Components of one type live packed together in a single contiguous vector.
/// That packing is the whole point: a system that walks every Velocity touches
/// one cache-friendly array instead of chasing pointers all over the heap.
///
/// The Entity -> index map is a hash map here rather than a sparse array. A
/// sparse array would be faster, but it costs memory proportional to the highest
/// ID ever handed out, and this game never has more than a few hundred entities
/// alive at once. Noted as a deliberate tradeoff in docs/design-decisions.md.
template <typename T>
class ComponentPool final : public IComponentPool {
public:
    T& add(Entity e, T value) {
        auto it = lookup_.find(e);
        if (it != lookup_.end()) {
            components_[it->second] = std::move(value);
            return components_[it->second];
        }
        lookup_.emplace(e, components_.size());
        entities_.push_back(e);
        components_.push_back(std::move(value));
        return components_.back();
    }

    T* tryGet(Entity e) {
        auto it = lookup_.find(e);
        return it == lookup_.end() ? nullptr : &components_[it->second];
    }

    const T* tryGet(Entity e) const {
        auto it = lookup_.find(e);
        return it == lookup_.end() ? nullptr : &components_[it->second];
    }

    T& get(Entity e) {
        T* c = tryGet(e);
        assert(c && "get() on an entity that does not own this component");
        return *c;
    }

    bool has(Entity e) const override { return lookup_.count(e) != 0; }

    /// Swap-and-pop removal. Order is never meaningful inside a pool, so we can
    /// keep the array dense by moving the last element into the hole.
    void remove(Entity e) override {
        auto it = lookup_.find(e);
        if (it == lookup_.end()) return;

        const std::size_t idx = it->second;
        const std::size_t last = components_.size() - 1;
        if (idx != last) {
            components_[idx] = std::move(components_[last]);
            entities_[idx] = entities_[last];
            lookup_[entities_[idx]] = idx;
        }
        components_.pop_back();
        entities_.pop_back();
        lookup_.erase(it);
    }

    std::size_t size() const override { return components_.size(); }

    // Direct access for systems that want to walk the packed array themselves.
    std::vector<T>& data() { return components_; }
    const std::vector<Entity>& entities() const { return entities_; }

private:
    std::vector<Entity> entities_;   // parallel to components_
    std::vector<T> components_;
    std::unordered_map<Entity, std::size_t> lookup_;
};

// -----------------------------------------------------------------------------
// Registry
// -----------------------------------------------------------------------------

/// Owns every entity and every component pool.
///
/// IDs are handed out monotonically and never recycled. Recycling would need
/// generation counters to catch stale handles, and at 32 bits we would have to
/// spawn an entity every frame for two years to run out. Simpler is better here.
class Registry {
public:
    Entity create() {
        const Entity e = nextId_++;
        alive_.push_back(e);
        aliveSet_.insert(e);
        return e;
    }

    bool alive(Entity e) const { return aliveSet_.count(e) != 0; }

    /// Queue an entity for removal. Systems destroy things mid-iteration all the
    /// time (an enemy dies while the combat system is walking enemies), so actual
    /// deletion is deferred to a single flush point at the end of the frame.
    void destroy(Entity e) {
        if (alive(e)) pendingDestroy_.push_back(e);
    }

    void flushDestroyed() {
        if (pendingDestroy_.empty()) return;
        for (Entity e : pendingDestroy_) {
            if (!alive(e)) continue;
            for (auto& [type, pool] : pools_) pool->remove(e);
            aliveSet_.erase(e);
            alive_.erase(std::remove(alive_.begin(), alive_.end(), e), alive_.end());
        }
        pendingDestroy_.clear();
    }

    /// Wipe everything. Used when regenerating a floor or restarting a run.
    void clear() {
        pools_.clear();
        alive_.clear();
        aliveSet_.clear();
        pendingDestroy_.clear();
        nextId_ = kNullEntity + 1;
    }

    template <typename T>
    T& add(Entity e, T value = T{}) {
        return pool<T>().add(e, std::move(value));
    }

    template <typename T>
    T* tryGet(Entity e) {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) return nullptr;
        return static_cast<ComponentPool<T>*>(it->second.get())->tryGet(e);
    }

    template <typename T>
    T& get(Entity e) {
        T* c = tryGet<T>(e);
        assert(c && "get() on an entity that does not own this component");
        return *c;
    }

    template <typename T>
    bool has(Entity e) {
        return tryGet<T>(e) != nullptr;
    }

    template <typename T>
    void remove(Entity e) {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it != pools_.end()) it->second->remove(e);
    }

    template <typename T>
    ComponentPool<T>& pool() {
        const std::type_index key(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            it = pools_.emplace(key, std::make_unique<ComponentPool<T>>()).first;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    /// Iterate every entity owning all of Ts, calling fn(Entity, T&...).
    ///
    /// The *first* type drives the iteration, so call sites should list the
    /// rarest component first: each<Health, Transform> walks the handful of
    /// entities that can be damaged, while each<Transform, Health> would walk
    /// every entity in the world and reject most of them. A heavier ECS picks
    /// the smallest pool automatically; doing that generically needs runtime
    /// dispatch over the type list, which is not worth the complexity here.
    template <typename First, typename... Rest, typename Fn>
    void each(Fn&& fn) {
        auto& primary = pool<First>();
        // Snapshot the ID list: the callback is allowed to add components or
        // spawn entities, either of which can reallocate the pool underneath us.
        scratch_ = primary.entities();
        for (Entity e : scratch_) {
            if (!alive(e)) continue;
            First* first = primary.tryGet(e);
            if (!first) continue;  // removed by an earlier callback
            if (!(has<Rest>(e) && ...)) continue;
            fn(e, *first, get<Rest>(e)...);
        }
    }

    const std::vector<Entity>& entities() const { return alive_; }
    std::size_t entityCount() const { return alive_.size(); }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
    std::vector<Entity> alive_;
    std::unordered_set<Entity> aliveSet_;
    std::vector<Entity> pendingDestroy_;
    std::vector<Entity> scratch_;
    Entity nextId_ = kNullEntity + 1;
};

}  // namespace engine
