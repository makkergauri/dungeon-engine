#pragma once

#include <chrono>

namespace engine {

/// Frame timing and the fixed-timestep accumulator.
///
/// Uses std::chrono rather than SFML's clock purely so this class -- and the
/// timestep logic, which is the part worth testing -- has no dependency on the
/// windowing library.
class Time {
public:
    explicit Time(double fixedStep = 1.0 / 60.0);

    /// Sample the wall clock and bank the elapsed time. Call once at the top of
    /// every frame.
    void beginFrame();

    /// Consume one fixed step if enough time has accumulated. Drive the physics
    /// loop with `while (time.consumeFixedStep()) { ... }`.
    bool consumeFixedStep();

    float fixedDelta() const { return static_cast<float>(fixedStep_); }
    float frameDelta() const { return static_cast<float>(frameDelta_); }

    /// How far we are between the last completed fixed step and the next one,
    /// in [0, 1). Render interpolation uses this to draw entities between
    /// simulation states instead of snapping to the last one.
    float alpha() const { return static_cast<float>(accumulator_ / fixedStep_); }

    /// Smoothed frames per second, for the debug overlay.
    float fps() const { return smoothedFps_; }

    double totalElapsed() const { return elapsed_; }

    /// Ignore the time spent in the last frame. Call after loading a level or
    /// unpausing, otherwise the accumulator holds several seconds of debt and
    /// the game fast-forwards to catch up.
    void discardLostTime();

private:
    using Clock = std::chrono::steady_clock;

    /// Ceiling on how much time one frame may contribute.
    ///
    /// Without this, a long stall banks (say) two seconds, the fixed loop runs
    /// 120 steps to catch up, that takes longer than a frame, which banks even
    /// more time. The simulation never catches up and the game locks solid --
    /// the "spiral of death". Clamping means a stalled frame drops simulation
    /// time instead, which players read as a brief hitch rather than a freeze.
    static constexpr double kMaxFrameTime = 0.25;

    double fixedStep_;
    double accumulator_ = 0.0;
    double frameDelta_ = 0.0;
    double elapsed_ = 0.0;
    float smoothedFps_ = 0.0f;
    Clock::time_point lastTick_;
};

}  // namespace engine
