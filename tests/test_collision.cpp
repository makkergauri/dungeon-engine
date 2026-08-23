#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/physics/Collision.h"
#include "engine/physics/Physics.h"

using Catch::Approx;
using namespace engine;

namespace {

/// Minimal grid backing for the physics tests: a rectangular room with a solid
/// border, plus whatever extra walls a test asks for.
class TestGrid final : public SolidGrid {
public:
    TestGrid(int w, int h, float tile) : w_(w), h_(h), tile_(tile), solid_(w * h, 0) {
        for (int x = 0; x < w; ++x) { set(x, 0, true); set(x, h - 1, true); }
        for (int y = 0; y < h; ++y) { set(0, y, true); set(w - 1, y, true); }
    }

    void set(int x, int y, bool solid) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        solid_[y * w_ + x] = solid ? 1 : 0;
    }

    bool isSolid(int x, int y) const override {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return true;
        return solid_[y * w_ + x] != 0;
    }
    float tileSize() const override { return tile_; }
    int widthInTiles() const override { return w_; }
    int heightInTiles() const override { return h_; }

private:
    int w_, h_;
    float tile_;
    std::vector<unsigned char> solid_;
};

}  // namespace

TEST_CASE("AABB overlap is exclusive at the edges", "[collision]") {
    const AABB a{0.0f, 0.0f, 10.0f, 10.0f};

    SECTION("clearly overlapping") {
        REQUIRE(overlaps(a, AABB{5.0f, 5.0f, 10.0f, 10.0f}));
    }
    SECTION("clearly separate") {
        REQUIRE_FALSE(overlaps(a, AABB{20.0f, 0.0f, 10.0f, 10.0f}));
    }
    SECTION("exactly touching edges does not count") {
        // This is the case that matters. If touching counted as overlapping, an
        // entity resting flush against a wall would re-trigger a collision every
        // frame and jitter forever.
        REQUIRE_FALSE(overlaps(a, AABB{10.0f, 0.0f, 10.0f, 10.0f}));
        REQUIRE_FALSE(overlaps(a, AABB{0.0f, 10.0f, 10.0f, 10.0f}));
    }
    SECTION("fully contained") {
        REQUIRE(overlaps(a, AABB{2.0f, 2.0f, 3.0f, 3.0f}));
    }
}

TEST_CASE("resolve returns the minimum translation vector", "[collision]") {
    const AABB a{0.0f, 0.0f, 10.0f, 10.0f};

    SECTION("shallow horizontal overlap pushes horizontally") {
        const Penetration p = resolve(a, AABB{8.0f, 2.0f, 10.0f, 10.0f});
        REQUIRE(p.hit);
        REQUIRE(p.normalX == Approx(-1.0f));
        REQUIRE(p.normalY == Approx(0.0f));
        REQUIRE(p.depth == Approx(2.0f));
    }

    SECTION("shallow vertical overlap pushes vertically") {
        const Penetration p = resolve(a, AABB{2.0f, 8.0f, 10.0f, 10.0f});
        REQUIRE(p.hit);
        REQUIRE(p.normalX == Approx(0.0f));
        REQUIRE(p.normalY == Approx(-1.0f));
        REQUIRE(p.depth == Approx(2.0f));
    }

    SECTION("resolves along the shallower axis, not the deeper one") {
        // Overlapping 8 on X and 2 on Y. Choosing the deeper axis here is the
        // classic bug that teleports an entity through a wall on a corner graze.
        const Penetration p = resolve(a, AABB{2.0f, 8.0f, 20.0f, 10.0f});
        REQUIRE(p.hit);
        REQUIRE(p.normalY == Approx(-1.0f));
        REQUIRE(p.depth == Approx(2.0f));
    }

    SECTION("no overlap reports no hit") {
        REQUIRE_FALSE(resolve(a, AABB{50.0f, 50.0f, 10.0f, 10.0f}).hit);
    }
}

TEST_CASE("swept AABB finds the impact time", "[collision]") {
    const AABB mover{0.0f, 0.0f, 4.0f, 4.0f};

    SECTION("moving right into a wall") {
        const Sweep s = sweep(mover, 100.0f, 0.0f, AABB{50.0f, 0.0f, 10.0f, 10.0f});
        REQUIRE(s.hit);
        REQUIRE(s.time == Approx(0.46f));  // (50 - 4) / 100
        REQUIRE(s.normalX == Approx(-1.0f));
    }

    SECTION("motion that stops short misses") {
        REQUIRE_FALSE(sweep(mover, 10.0f, 0.0f, AABB{50.0f, 0.0f, 10.0f, 10.0f}).hit);
    }

    SECTION("passing above the obstacle misses") {
        REQUIRE_FALSE(sweep(mover, 100.0f, 0.0f, AABB{50.0f, 40.0f, 10.0f, 10.0f}).hit);
    }

    SECTION("already overlapping reports an immediate hit") {
        // Without this special case the slab maths yields a negative entry time
        // and the mover slides further into the wall instead of being pushed out.
        const Sweep s = sweep(mover, 10.0f, 0.0f, AABB{2.0f, 0.0f, 10.0f, 10.0f});
        REQUIRE(s.hit);
        REQUIRE(s.time == Approx(0.0f));
    }

    SECTION("stationary box never hits") {
        REQUIRE_FALSE(sweep(mover, 0.0f, 0.0f, AABB{50.0f, 0.0f, 10.0f, 10.0f}).hit);
    }
}

TEST_CASE("makeAABB centres the box on the transform", "[collision]") {
    Transform transform;
    transform.x = 100.0f;
    transform.y = 50.0f;

    Collider collider;
    collider.width = 20.0f;
    collider.height = 10.0f;

    const AABB box = makeAABB(transform, collider);
    REQUIRE(box.centerX() == Approx(100.0f));
    REQUIRE(box.centerY() == Approx(50.0f));
    REQUIRE(box.left() == Approx(90.0f));
    REQUIRE(box.top() == Approx(45.0f));
}

TEST_CASE("physics keeps bodies out of walls", "[physics]") {
    constexpr float kTile = 32.0f;
    TestGrid grid(20, 20, kTile);

    Registry registry;
    PhysicsSystem physics;
    physics.setSolidGrid(&grid);

    const Entity e = registry.create();
    registry.add<Transform>(e, Transform{kTile * 5.5f, kTile * 5.5f});
    registry.add<Velocity>(e, Velocity{});
    Collider collider;
    collider.width = 20.0f;
    collider.height = 20.0f;
    registry.add<Collider>(e, collider);

    SECTION("a fast body cannot tunnel through the border wall") {
        // 4000 px/s at a 60Hz step is 66px per step -- more than two tiles. The
        // substepping in moveAndCollide is the only thing stopping this.
        registry.get<Velocity>(e) = Velocity{4000.0f, 0.0f};
        for (int i = 0; i < 120; ++i) physics.fixedUpdate(registry, 1.0f / 60.0f);

        const Transform& t = registry.get<Transform>(e);
        const int tx = static_cast<int>(t.x / kTile);
        const int ty = static_cast<int>(t.y / kTile);
        REQUIRE_FALSE(grid.isSolid(tx, ty));
        REQUIRE(t.x < kTile * 19.0f);
    }

    SECTION("blocking one axis leaves the other free (wall sliding)") {
        // A vertical wall to the right. Moving diagonally into it should cancel
        // X and preserve the full Y speed, which is what makes movement feel
        // smooth instead of sticky.
        for (int y = 0; y < 20; ++y) grid.set(7, y, true);
        registry.get<Transform>(e) = Transform{kTile * 6.5f, kTile * 3.0f};
        registry.get<Velocity>(e) = Velocity{200.0f, 200.0f};

        const float startY = registry.get<Transform>(e).y;
        for (int i = 0; i < 30; ++i) {
            registry.get<Velocity>(e) = Velocity{200.0f, 200.0f};
            physics.fixedUpdate(registry, 1.0f / 60.0f);
        }

        const Transform& t = registry.get<Transform>(e);
        REQUIRE(t.x < kTile * 7.0f);          // stopped by the wall
        REQUIRE(t.y > startY + 90.0f);        // still moving freely downward
    }

    SECTION("triggers pass through walls") {
        Collider& c = registry.get<Collider>(e);
        c.isTrigger = true;
        registry.get<Transform>(e) = Transform{kTile * 5.5f, kTile * 5.5f};
        registry.get<Velocity>(e) = Velocity{600.0f, 0.0f};

        for (int i = 0; i < 60; ++i) physics.fixedUpdate(registry, 1.0f / 60.0f);
        // A sword swing must not be stopped by the wall behind its target.
        REQUIRE(registry.get<Transform>(e).x > kTile * 19.0f);
    }
}

TEST_CASE("physics reports entity overlaps subject to layer masks", "[physics]") {
    Registry registry;
    PhysicsSystem physics;  // no grid: only entity-vs-entity matters here

    auto makeBody = [&](float x, std::uint32_t layer, std::uint32_t mask) {
        const Entity e = registry.create();
        registry.add<Transform>(e, Transform{x, 0.0f});
        Collider collider;
        collider.width = 10.0f;
        collider.height = 10.0f;
        collider.layer = layer;
        collider.mask = mask;
        registry.add<Collider>(e, collider);
        return e;
    };

    SECTION("overlapping bodies on matching layers are reported once") {
        makeBody(0.0f, 0x1, 0x2);
        makeBody(5.0f, 0x2, 0x1);
        physics.fixedUpdate(registry, 1.0f / 60.0f);
        REQUIRE(physics.overlapEvents().size() == 1);
    }

    SECTION("non-matching masks are filtered out") {
        makeBody(0.0f, 0x1, 0x4);
        makeBody(5.0f, 0x2, 0x8);
        physics.fixedUpdate(registry, 1.0f / 60.0f);
        REQUIRE(physics.overlapEvents().empty());
    }

    SECTION("separated bodies are not reported") {
        makeBody(0.0f, 0x1, 0xFFFFFFFF);
        makeBody(100.0f, 0x2, 0xFFFFFFFF);
        physics.fixedUpdate(registry, 1.0f / 60.0f);
        REQUIRE(physics.overlapEvents().empty());
    }
}
