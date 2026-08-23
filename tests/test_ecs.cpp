#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include "engine/core/Time.h"
#include "engine/ecs/System.h"
#include "engine/rendering/Camera.h"

using Catch::Approx;
using namespace engine;

TEST_CASE("entities and components have independent lifetimes", "[ecs]") {
    Registry registry;

    const Entity a = registry.create();
    const Entity b = registry.create();

    REQUIRE(a != kNullEntity);
    REQUIRE(a != b);
    REQUIRE(registry.alive(a));
    REQUIRE(registry.entityCount() == 2);

    SECTION("components can be added, read and removed") {
        registry.add<Transform>(a, Transform{10.0f, 20.0f});
        REQUIRE(registry.has<Transform>(a));
        REQUIRE_FALSE(registry.has<Velocity>(a));
        REQUIRE(registry.get<Transform>(a).x == Approx(10.0f));

        registry.get<Transform>(a).x = 99.0f;
        REQUIRE(registry.get<Transform>(a).x == Approx(99.0f));

        registry.remove<Transform>(a);
        REQUIRE_FALSE(registry.has<Transform>(a));
        REQUIRE(registry.tryGet<Transform>(a) == nullptr);
    }

    SECTION("adding the same component twice overwrites rather than duplicating") {
        registry.add<Health>(a, Health{5, 5, 0.0f});
        registry.add<Health>(a, Health{9, 9, 0.0f});
        REQUIRE(registry.get<Health>(a).current == 9);
        REQUIRE(registry.pool<Health>().size() == 1);
    }

    SECTION("tryGet on an unknown component type is safe") {
        // No pool for Lifetime has ever been created here.
        REQUIRE(registry.tryGet<Lifetime>(a) == nullptr);
    }
}

TEST_CASE("destruction is deferred until the flush point", "[ecs]") {
    Registry registry;
    const Entity a = registry.create();
    registry.add<Transform>(a, Transform{});

    registry.destroy(a);
    // Still alive: systems destroy entities mid-iteration constantly, and
    // yanking the components out immediately would invalidate the pool the
    // caller is walking.
    REQUIRE(registry.alive(a));
    REQUIRE(registry.has<Transform>(a));

    registry.flushDestroyed();
    REQUIRE_FALSE(registry.alive(a));
    REQUIRE_FALSE(registry.has<Transform>(a));
    REQUIRE(registry.entityCount() == 0);

    SECTION("destroying twice is harmless") {
        const Entity b = registry.create();
        registry.destroy(b);
        registry.destroy(b);
        registry.flushDestroyed();
        REQUIRE_FALSE(registry.alive(b));
    }
}

TEST_CASE("each() visits exactly the entities owning every listed component", "[ecs]") {
    Registry registry;

    const Entity moving = registry.create();
    registry.add<Transform>(moving, Transform{});
    registry.add<Velocity>(moving, Velocity{1.0f, 0.0f});

    const Entity stationary = registry.create();
    registry.add<Transform>(stationary, Transform{});

    const Entity ghost = registry.create();
    registry.add<Velocity>(ghost, Velocity{5.0f, 5.0f});

    int visited = 0;
    registry.each<Velocity, Transform>([&](Entity e, Velocity&, Transform&) {
        REQUIRE(e == moving);
        ++visited;
    });
    REQUIRE(visited == 1);

    int transforms = 0;
    registry.each<Transform>([&](Entity, Transform&) { ++transforms; });
    REQUIRE(transforms == 2);

    SECTION("mutating the world inside the callback does not corrupt iteration") {
        // The snapshot in each() exists for exactly this: spawning during
        // iteration reallocates the pool the loop would otherwise be holding.
        int seen = 0;
        registry.each<Velocity>([&](Entity, Velocity&) {
            ++seen;
            const Entity spawned = registry.create();
            registry.add<Velocity>(spawned, Velocity{});
        });
        REQUIRE(seen == 2);  // only the two that existed when the loop started
    }

    SECTION("entities destroyed by an earlier callback are skipped") {
        registry.destroy(moving);
        registry.flushDestroyed();
        int remaining = 0;
        registry.each<Velocity>([&](Entity, Velocity&) { ++remaining; });
        REQUIRE(remaining == 1);
    }
}

TEST_CASE("swap-and-pop removal keeps other components intact", "[ecs]") {
    // Removal moves the last element into the hole and rewrites its index. If
    // that bookkeeping is wrong, an unrelated entity silently starts reading
    // another entity's data -- the nastiest possible class of bug here.
    Registry registry;
    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i) {
        const Entity e = registry.create();
        registry.add<Health>(e, Health{i, i, 0.0f});
        entities.push_back(e);
    }

    registry.remove<Health>(entities[3]);
    registry.remove<Health>(entities[0]);
    registry.remove<Health>(entities[9]);

    REQUIRE(registry.pool<Health>().size() == 7);
    for (int i = 0; i < 10; ++i) {
        if (i == 0 || i == 3 || i == 9) {
            REQUIRE_FALSE(registry.has<Health>(entities[i]));
        } else {
            REQUIRE(registry.get<Health>(entities[i]).current == i);
        }
    }
}

TEST_CASE("clear() resets the world", "[ecs]") {
    Registry registry;
    for (int i = 0; i < 5; ++i) registry.add<Transform>(registry.create(), Transform{});
    REQUIRE(registry.entityCount() == 5);

    registry.clear();
    REQUIRE(registry.entityCount() == 0);

    const Entity fresh = registry.create();
    REQUIRE(fresh != kNullEntity);
    REQUIRE(registry.alive(fresh));
}

TEST_CASE("LifetimeSystem ages entities out", "[ecs]") {
    Registry registry;
    const Entity e = registry.create();
    registry.add<Lifetime>(e, Lifetime{0.1f});

    LifetimeSystem system;
    system.fixedUpdate(registry, 1.0f / 60.0f);
    registry.flushDestroyed();
    REQUIRE(registry.alive(e));

    for (int i = 0; i < 10; ++i) system.fixedUpdate(registry, 1.0f / 60.0f);
    registry.flushDestroyed();
    REQUIRE_FALSE(registry.alive(e));
}

TEST_CASE("the fixed timestep accumulator is stable", "[time]") {
    Time time(1.0 / 60.0);
    REQUIRE(time.fixedDelta() == Approx(1.0f / 60.0f));

    SECTION("no steps are available before any time has passed") {
        time.discardLostTime();
        REQUIRE_FALSE(time.consumeFixedStep());
    }

    SECTION("alpha stays within one step") {
        time.beginFrame();
        while (time.consumeFixedStep()) {}
        REQUIRE(time.alpha() >= 0.0f);
        REQUIRE(time.alpha() < 1.0f);
    }
}

TEST_CASE("camera clamps to level bounds", "[camera]") {
    Camera camera;
    camera.setViewportSize(800.0f, 600.0f);
    camera.setBounds(0.0f, 0.0f, 3200.0f, 2400.0f);

    SECTION("cannot show past the left or top edge") {
        camera.setPosition(-500.0f, -500.0f);
        REQUIRE(camera.x() == Approx(400.0f));  // half the viewport width
        REQUIRE(camera.y() == Approx(300.0f));
    }

    SECTION("cannot show past the right or bottom edge") {
        camera.setPosition(99999.0f, 99999.0f);
        REQUIRE(camera.x() == Approx(3200.0f - 400.0f));
        REQUIRE(camera.y() == Approx(2400.0f - 300.0f));
    }

    SECTION("a level smaller than the viewport is centred, not clamped") {
        // Clamping here would fight itself between the two limits and jitter.
        camera.setBounds(0.0f, 0.0f, 400.0f, 300.0f);
        camera.setPosition(0.0f, 0.0f);
        REQUIRE(camera.x() == Approx(200.0f));
        REQUIRE(camera.y() == Approx(150.0f));
    }
}

TEST_CASE("camera follow converges and is frame-rate independent", "[camera]") {
    Camera slow;
    slow.setViewportSize(800.0f, 600.0f);
    slow.setPosition(0.0f, 0.0f);

    Camera fast;
    fast.setViewportSize(800.0f, 600.0f);
    fast.setPosition(0.0f, 0.0f);

    // One second of following, at 30fps and at 120fps. If the smoothing used a
    // naive per-frame constant these would end up in very different places.
    for (int i = 0; i < 30; ++i) slow.follow(1000.0f, 0.0f, 1.0f / 30.0f);
    for (int i = 0; i < 120; ++i) fast.follow(1000.0f, 0.0f, 1.0f / 120.0f);

    REQUIRE(slow.x() == Approx(fast.x()).margin(1.0f));
    REQUIRE(slow.x() > 990.0f);
}

TEST_CASE("screen shake decays back to zero", "[camera]") {
    Camera camera;
    camera.setViewportSize(800.0f, 600.0f);
    camera.setPosition(500.0f, 500.0f);

    camera.addTrauma(1.0f);
    camera.update(0.016f);
    const float displaced = std::abs(camera.x() - 500.0f) + std::abs(camera.y() - 500.0f);
    REQUIRE(displaced > 0.0f);

    for (int i = 0; i < 200; ++i) camera.update(0.016f);
    REQUIRE(camera.x() == Approx(500.0f));
    REQUIRE(camera.y() == Approx(500.0f));
}
