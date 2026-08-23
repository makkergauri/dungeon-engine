#pragma once

#include <cstddef>
#include <vector>

#include "Renderer.h"
#include "Sprite.h"

namespace engine {

/// Fixed-capacity particle pool.
///
/// Particles are deliberately *not* ECS entities. A single death burst spawns
/// thirty of them, they live for half a second, and they have no behaviour worth
/// composing. Routing that churn through entity creation and component pools
/// would cost far more than it buys; a flat pre-allocated array with a live count
/// is the right shape for this data. Choosing not to use the architecture
/// everywhere is part of understanding it.
class ParticleSystem {
public:
    struct Emit {
        float x = 0.0f, y = 0.0f;
        int count = 12;
        float speedMin = 40.0f;
        float speedMax = 140.0f;
        float lifeMin = 0.25f;
        float lifeMax = 0.6f;
        float sizeMin = 2.0f;
        float sizeMax = 5.0f;
        Color color = colors::kWhite;
        /// Restrict the burst to a cone, in degrees. 360 gives an even spray;
        /// narrow angles are for directional effects like blood from a hit.
        float directionDegrees = 0.0f;
        float spreadDegrees = 360.0f;
        float gravity = 0.0f;
        float drag = 2.5f;
    };

    explicit ParticleSystem(std::size_t capacity = 2048);

    void emit(const Emit& params);
    void update(float dt);
    void submit(Renderer& renderer, int layer = kLayerParticle) const;
    void clear() { live_ = 0; }

    std::size_t liveCount() const { return live_; }

private:
    struct Particle {
        float x, y, vx, vy;
        float life, maxLife;
        float size;
        Color color;
        float gravity;
        float drag;
    };

    std::vector<Particle> particles_;
    /// Live particles occupy [0, live_). Killing one swaps in the last live
    /// particle, so the update loop never branches on a dead flag.
    std::size_t live_ = 0;
    unsigned int rngState_ = 0x9E3779B9u;

    float randomFloat(float lo, float hi);
};

}  // namespace engine
