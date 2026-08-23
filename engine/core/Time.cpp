#include "Time.h"

#include <algorithm>

namespace engine {

Time::Time(double fixedStep) : fixedStep_(fixedStep), lastTick_(Clock::now()) {}

void Time::beginFrame() {
    const Clock::time_point now = Clock::now();
    const std::chrono::duration<double> delta = now - lastTick_;
    lastTick_ = now;

    frameDelta_ = std::min(delta.count(), kMaxFrameTime);
    elapsed_ += frameDelta_;
    accumulator_ += frameDelta_;

    // Exponential smoothing on the FPS readout. The raw value flickers by tens
    // of frames between samples and is unreadable on screen.
    if (frameDelta_ > 0.0) {
        const float instant = static_cast<float>(1.0 / frameDelta_);
        smoothedFps_ = (smoothedFps_ == 0.0f) ? instant
                                              : smoothedFps_ * 0.92f + instant * 0.08f;
    }
}

bool Time::consumeFixedStep() {
    if (accumulator_ < fixedStep_) return false;
    accumulator_ -= fixedStep_;
    return true;
}

void Time::discardLostTime() {
    lastTick_ = Clock::now();
    accumulator_ = 0.0;
}

}  // namespace engine
