#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace engine {

void Camera::setViewportSize(float width, float height) {
    viewWidth_ = width;
    viewHeight_ = height;
    clampToBounds();
}

void Camera::setPosition(float x, float y) {
    x_ = x;
    y_ = y;
    clampToBounds();
}

void Camera::setBounds(float minX, float minY, float maxX, float maxY) {
    hasBounds_ = true;
    minX_ = minX;
    minY_ = minY;
    maxX_ = maxX;
    maxY_ = maxY;
    clampToBounds();
}

void Camera::follow(float targetX, float targetY, float dt) {
    // Frame-rate independent exponential smoothing.
    //
    // The tempting version is `pos += (target - pos) * 0.1f`, but that constant
    // is secretly "per frame", so the camera lags twice as far behind at 30fps
    // as at 60. Going through exp() makes the rate genuinely per-second.
    const float t = 1.0f - std::exp(-followSpeed * dt);
    x_ += (targetX - x_) * t;
    y_ += (targetY - y_) * t;
    clampToBounds();
}

void Camera::addTrauma(float amount) {
    trauma_ = std::min(1.0f, trauma_ + amount);
}

void Camera::update(float dt) {
    shakeTime_ += dt;

    if (trauma_ > 0.0f) {
        trauma_ = std::max(0.0f, trauma_ - traumaDecayPerSecond * dt);
        const float magnitude = trauma_ * trauma_ * maxShakePixels;

        // Two sine waves at unrelated frequencies. Cheaper than noise and, more
        // importantly, continuous -- random offsets per frame produce a buzz
        // that reads as a rendering glitch rather than an impact.
        shakeX_ = magnitude * std::sin(shakeTime_ * 47.0f);
        shakeY_ = magnitude * std::sin(shakeTime_ * 59.0f + 1.7f);
    } else {
        shakeX_ = 0.0f;
        shakeY_ = 0.0f;
    }
}

void Camera::clampToBounds() {
    if (!hasBounds_) return;

    const float halfW = viewWidth_ * 0.5f;
    const float halfH = viewHeight_ * 0.5f;
    const float levelWidth = maxX_ - minX_;
    const float levelHeight = maxY_ - minY_;

    if (levelWidth <= viewWidth_) {
        x_ = minX_ + levelWidth * 0.5f;
    } else {
        x_ = std::clamp(x_, minX_ + halfW, maxX_ - halfW);
    }

    if (levelHeight <= viewHeight_) {
        y_ = minY_ + levelHeight * 0.5f;
    } else {
        y_ = std::clamp(y_, minY_ + halfH, maxY_ - halfH);
    }
}

}  // namespace engine
