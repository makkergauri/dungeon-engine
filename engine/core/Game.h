#pragma once

#include "../audio/AudioManager.h"
#include "../ecs/System.h"
#include "../input/InputManager.h"
#include "../rendering/Camera.h"
#include "../rendering/ParticleSystem.h"
#include "../rendering/Renderer.h"
#include "Time.h"
#include "Window.h"

namespace engine {

/// Application skeleton: owns the subsystems and runs the loop.
///
/// Games subclass this and override the lifecycle hooks. Everything the engine
/// provides is reachable through the protected accessors, and nothing in here
/// knows what game is being built on top of it.
class Game {
public:
    virtual ~Game() = default;

    /// Boot the subsystems, run until the window closes, then tear down.
    /// Returns a process exit code.
    int run(const Window::Config& config);

    void requestQuit() { running_ = false; }

protected:
    // --- Lifecycle hooks ---
    /// Load assets and build the world. Return false to abort startup.
    virtual bool onInit() { return true; }
    /// Gameplay. Called zero or more times per frame at a constant `dt`.
    virtual void onFixedUpdate(float dt) { (void)dt; }
    /// Presentation. Called exactly once per frame with the real frame time.
    virtual void onUpdate(float dt) { (void)dt; }
    /// Issue draw commands. The window has already been cleared.
    virtual void onRender() {}
    /// Raw events, for anything the InputManager does not cover (text entry,
    /// window-specific handling).
    virtual void onEvent(const sf::Event& event) { (void)event; }
    virtual void onShutdown() {}

    Window& window() { return window_; }
    Renderer& renderer() { return renderer_; }
    InputManager& input() { return input_; }
    AudioManager& audio() { return audio_; }
    ParticleSystem& particles() { return particles_; }
    Registry& registry() { return registry_; }
    SystemScheduler& systems() { return systems_; }
    Camera& camera() { return camera_; }
    const Time& time() const { return time_; }
    Time& time() { return time_; }

private:
    Window window_;
    Renderer renderer_;
    InputManager input_;
    AudioManager audio_;
    ParticleSystem particles_;
    Registry registry_;
    SystemScheduler systems_;
    Camera camera_;
    Time time_{1.0 / 60.0};
    bool running_ = false;
};

}  // namespace engine
