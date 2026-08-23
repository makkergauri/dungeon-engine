#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../ecs/System.h"

namespace engine {

/// Handle into the renderer's texture cache. Components store this rather than
/// an sf::Texture* so that nothing in the ECS has to include SFML -- the whole
/// component set stays copyable, comparable, and testable without a GPU.
using TextureId = std::uint32_t;
inline constexpr TextureId kNoTexture = 0;

struct Color {
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
};

namespace colors {
inline constexpr Color kWhite{255, 255, 255, 255};
inline constexpr Color kBlack{0, 0, 0, 255};
inline constexpr Color kRed{220, 70, 70, 255};
inline constexpr Color kGreen{110, 200, 110, 255};
inline constexpr Color kGold{230, 190, 90, 255};
}  // namespace colors

/// Sub-rectangle of a texture, in pixels.
struct TextureRect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

/// Draw layers. Explicit constants beat magic numbers scattered through the
/// spawn code, and keeping them ordered here makes the sort key obvious.
enum Layer : int {
    kLayerFloor = 0,
    kLayerDecal = 10,
    kLayerItem = 20,
    kLayerActor = 30,
    kLayerParticle = 40,
    kLayerUI = 100,
};

struct SpriteComponent {
    TextureId texture = kNoTexture;
    TextureRect source;      // zero width/height means "use the whole texture"
    float width = 32.0f;     // world-space size, independent of source pixels
    float height = 32.0f;
    Color tint = colors::kWhite;
    int layer = kLayerActor;
    bool flipX = false;
    bool visible = true;
    /// Vertical draw offset, so a sprite can stand taller than its collider.
    float offsetY = 0.0f;
};

/// A named run of frames. Owned by AnimationLibrary and referenced (never
/// copied) by animators, because a clip is shared by every entity of a type.
struct AnimationClip {
    std::vector<TextureRect> frames;
    float frameDuration = 0.12f;
    bool loop = true;
};

struct Animator {
    const AnimationClip* clip = nullptr;
    float timer = 0.0f;
    int frame = 0;
    bool finished = false;
    float speed = 1.0f;
};

/// Owns every clip. Handing out raw pointers is safe here because the library
/// outlives every entity that references it -- it is loaded once at startup and
/// never mutated afterwards. Clips are stored in a deque-like stable container
/// so that adding one never invalidates pointers already handed out.
class AnimationLibrary {
public:
    /// Build a clip from a horizontal strip of frames in an atlas.
    const AnimationClip* addStrip(const std::string& name, float startX, float startY,
                                  float frameWidth, float frameHeight, int frameCount,
                                  float frameDuration, bool loop = true);

    const AnimationClip* find(const std::string& name) const;

private:
    // std::vector would reallocate and dangle every pointer already handed out.
    std::vector<std::unique_ptr<AnimationClip>> storage_;
    std::vector<std::string> names_;
};

/// Advances animators and writes the current frame into the sprite.
///
/// This runs on the render update, not the fixed update: animation is purely
/// cosmetic, and stepping it at the fixed rate would make it stutter on
/// high-refresh displays for no gameplay benefit.
class AnimationSystem final : public System {
public:
    void update(Registry& registry, float dt) override;
};

/// Swap an animator to a different clip, restarting only if it actually changed.
/// Without the guard, calling this every frame (which the state machines do)
/// would pin every animation to frame zero.
void playClip(Animator& animator, const AnimationClip* clip);

}  // namespace engine
