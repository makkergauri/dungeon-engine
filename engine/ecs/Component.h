#pragma once

#include <cstdint>

namespace engine {

/// Components are plain data. No virtuals, no constructors that do work, no
/// pointers back to the world. Keeping them trivially copyable is what lets the
/// pools stay packed and lets tests build a component by hand in one line.
///
/// Only genuinely reusable components live here. Anything that mentions the
/// words "dungeon", "player" or "loot" belongs in game/GameCommon.h -- the
/// engine folder must stay usable for a completely different 2D game.

struct Transform {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;  // degrees; only used by particles and effects
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
};

/// Axis-aligned box used for both solid collision and trigger overlaps.
/// The offset lets the collider sit somewhere other than the sprite origin --
/// most characters want a box around their feet, not their whole body.
struct Collider {
    float width = 16.0f;
    float height = 16.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    /// Triggers report overlaps but never block movement (pickups, hitboxes).
    bool isTrigger = false;
    /// Static bodies are never integrated or pushed out of walls.
    bool isStatic = false;

    /// Bitmask filtering, so the physics system can skip pairs that could never
    /// interact (enemy hitboxes vs other enemy hitboxes, for instance).
    std::uint32_t layer = 0x1;
    std::uint32_t mask = 0xFFFFFFFF;
};

struct Health {
    int current = 1;
    int max = 1;
    float invulnerableFor = 0.0f;  // seconds of i-frames remaining

    bool alive() const { return current > 0; }
};

/// Entities that should disappear on their own: corpses, hit sparks, floating
/// damage numbers. The lifetime system decrements and destroys.
struct Lifetime {
    float remaining = 1.0f;
};

}  // namespace engine
