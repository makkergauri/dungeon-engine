#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {
constexpr float kPi = 3.14159265f;
}

ParticleSystem::ParticleSystem(std::size_t capacity) { particles_.resize(capacity); }

float ParticleSystem::randomFloat(float lo, float hi) {
    // xorshift32: fast, deterministic, and self-contained. Using std::mt19937
    // here would be heavier than the particle update itself, and particles do
    // not need statistical quality.
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    const float unit = static_cast<float>(rngState_ & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
    return lo + unit * (hi - lo);
}

void ParticleSystem::emit(const Emit& params) {
    for (int i = 0; i < params.count; ++i) {
        // Silently drop the overflow rather than growing the pool. A frame that
        // wants more than 2048 particles has a bug in it, and reallocating
        // mid-frame would turn that bug into a stutter.
        if (live_ >= particles_.size()) return;

        Particle& p = particles_[live_++];
        const float halfSpread = params.spreadDegrees * 0.5f;
        const float angle = (params.directionDegrees +
                             randomFloat(-halfSpread, halfSpread)) * kPi / 180.0f;
        const float speed = randomFloat(params.speedMin, params.speedMax);

        p.x = params.x;
        p.y = params.y;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.maxLife = randomFloat(params.lifeMin, params.lifeMax);
        p.life = p.maxLife;
        p.size = randomFloat(params.sizeMin, params.sizeMax);
        p.color = params.color;
        p.gravity = params.gravity;
        p.drag = params.drag;
    }
}

void ParticleSystem::update(float dt) {
    std::size_t i = 0;
    while (i < live_) {
        Particle& p = particles_[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            p = particles_[--live_];
            continue;  // do not advance i: a fresh particle now sits at this slot
        }

        // Exponential drag, integrated the frame-rate independent way for the
        // same reason the camera smoothing is.
        const float damping = std::exp(-p.drag * dt);
        p.vx *= damping;
        p.vy = p.vy * damping + p.gravity * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        ++i;
    }
}

void ParticleSystem::submit(Renderer& renderer, int layer) const {
    for (std::size_t i = 0; i < live_; ++i) {
        const Particle& p = particles_[i];
        const float t = p.life / p.maxLife;

        Color color = p.color;
        color.a = static_cast<std::uint8_t>(std::clamp(t * 255.0f, 0.0f, 255.0f));
        // Shrink as well as fade. Fading alone leaves a ghostly square hanging
        // in the air; shrinking sells it as the particle dissipating.
        const float size = p.size * (0.4f + 0.6f * t);

        renderer.drawQuad(p.x, p.y, size, size, color, layer);
    }
}

}  // namespace engine
