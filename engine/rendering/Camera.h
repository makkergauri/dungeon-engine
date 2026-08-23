#pragma once

namespace engine {

/// 2D camera: position, viewport size, follow smoothing, bounds clamping and
/// screen shake.
///
/// Deliberately holds no SFML types. It produces plain numbers; the renderer is
/// the only thing that knows how to turn those into an sf::View. That keeps the
/// follow and clamp behaviour testable without opening a window.
class Camera {
public:
    void setViewportSize(float width, float height);
    void setPosition(float x, float y);

    /// Restrict the camera so the viewport never shows past the level edges.
    /// When the level is smaller than the viewport on an axis, the camera locks
    /// to the centre of that axis instead of jittering between two clamps.
    void setBounds(float minX, float minY, float maxX, float maxY);
    void clearBounds() { hasBounds_ = false; }

    void follow(float targetX, float targetY, float dt);

    /// Add trauma, in [0, 1]. Trauma decays on its own and shake offset is
    /// proportional to its square, so small hits are barely felt while a big one
    /// is unmistakable -- a linear response makes every hit feel the same.
    void addTrauma(float amount);

    void update(float dt);

    /// Final camera centre including the shake offset.
    float x() const { return x_ + shakeX_; }
    float y() const { return y_ + shakeY_; }
    float viewportWidth() const { return viewWidth_; }
    float viewportHeight() const { return viewHeight_; }

    /// How quickly the camera closes the gap to its target, per second.
    /// Higher is snappier; around 8 feels responsive without being rigid.
    float followSpeed = 8.0f;
    float maxShakePixels = 18.0f;
    float traumaDecayPerSecond = 1.6f;

private:
    void clampToBounds();

    float x_ = 0.0f;
    float y_ = 0.0f;
    float viewWidth_ = 960.0f;
    float viewHeight_ = 540.0f;

    bool hasBounds_ = false;
    float minX_ = 0.0f, minY_ = 0.0f, maxX_ = 0.0f, maxY_ = 0.0f;

    float trauma_ = 0.0f;
    float shakeX_ = 0.0f;
    float shakeY_ = 0.0f;
    float shakeTime_ = 0.0f;
};

}  // namespace engine
