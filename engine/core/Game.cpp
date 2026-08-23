#include "Game.h"

#include <iostream>

namespace engine {

int Game::run(const Window::Config& config) {
    if (!window_.create(config)) {
        std::cerr << "[engine] failed to create window\n";
        return 1;
    }
    if (!renderer_.init(window_.handle())) {
        std::cerr << "[engine] failed to initialise renderer\n";
        return 1;
    }

    camera_.setViewportSize(static_cast<float>(config.width),
                            static_cast<float>(config.height));

    if (!onInit()) {
        std::cerr << "[engine] game initialisation failed\n";
        return 1;
    }

    running_ = true;
    // Discard everything spent loading assets, or the accumulator opens with a
    // multi-second debt and the first frame simulates a minute of gameplay.
    time_.discardLostTime();

    while (running_ && window_.isOpen()) {
        time_.beginFrame();

        // --- Input ---
        input_.beginFrame();
        window_.pollEvents([this](const sf::Event& event) {
            input_.handleEvent(event);
            onEvent(event);
        });
        input_.pollGamepad();

        unsigned int newWidth = 0;
        unsigned int newHeight = 0;
        if (window_.consumeResized(newWidth, newHeight)) {
            camera_.setViewportSize(static_cast<float>(newWidth),
                                    static_cast<float>(newHeight));
        }

        // --- Fixed-rate simulation ---
        //
        // Gameplay advances in constant-size steps regardless of frame rate.
        // The alternative -- feeding the raw frame delta straight into movement
        // and collision -- means a machine that stutters gets different physics
        // from one that does not: entities move further per step and start
        // clipping through walls the collision code would otherwise have caught.
        // Constant steps also make a seeded run reproducible, which is the only
        // reason a movement bug is ever debuggable.
        //
        // The loop is bounded by Time's frame-time clamp, so a stall costs a
        // hitch rather than an ever-growing catch-up backlog.
        while (time_.consumeFixedStep()) {
            const float dt = time_.fixedDelta();
            systems_.fixedUpdate(registry_, dt);
            onFixedUpdate(dt);
            registry_.flushDestroyed();
        }

        // --- Variable-rate presentation ---
        // Animation, particles and camera smoothing run at the display rate so
        // they stay smooth on a 144Hz monitor while physics stays at 60.
        const float frameDelta = time_.frameDelta();
        systems_.update(registry_, frameDelta);
        particles_.update(frameDelta);
        camera_.update(frameDelta);
        onUpdate(frameDelta);

        // --- Render ---
        window_.clear();
        onRender();
        renderer_.flush();
        window_.display();
    }

    onShutdown();
    window_.close();
    return 0;
}

}  // namespace engine
