#pragma once

#include <SFML/Window.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace engine {

/// Actions are integers owned by the game, not the engine.
///
/// The engine has no idea what "attack" means; it only knows that action 4 is
/// bound to two keys and a gamepad button. The game defines its own enum and
/// casts. That is what keeps the input system reusable and makes remapping a
/// matter of rewriting a bindings table rather than hunting for key checks.
using ActionId = std::uint32_t;

class InputManager {
public:
    void bindKey(ActionId action, sf::Keyboard::Key key);
    void bindGamepadButton(ActionId action, unsigned int button);
    /// Bind an action to one direction of a gamepad axis. `positive` picks which
    /// half of the axis counts.
    void bindGamepadAxis(ActionId action, sf::Joystick::Axis axis, bool positive);
    void clearBindings(ActionId action);

    /// Call once per frame *before* pumping events: it rolls this frame's state
    /// into last frame's, which is what makes the edge queries below work.
    void beginFrame();
    void handleEvent(const sf::Event& event);
    /// Poll gamepad axes, which SFML only reports as events on large changes.
    void pollGamepad();

    bool isDown(ActionId action) const;
    /// True only on the frame the action went down. Use for anything that should
    /// happen once per press -- attacking, confirming a menu -- so holding the
    /// key does not repeat it every frame.
    bool wasPressed(ActionId action) const;
    bool wasReleased(ActionId action) const;

    /// Combined -1..1 axis from two opposing actions, so movement code does not
    /// care whether it came from a key or a stick.
    float axis(ActionId negative, ActionId positive) const;

    /// The most recent key pressed this frame, or Unknown. Used by the rebinding
    /// screen; nothing in gameplay should ever call this.
    sf::Keyboard::Key lastKeyPressed() const { return lastKey_; }

    void reset();

private:
    struct Binding {
        enum class Kind { Key, Button, Axis } kind = Kind::Key;
        sf::Keyboard::Key key = sf::Keyboard::Unknown;
        unsigned int button = 0;
        sf::Joystick::Axis axis = sf::Joystick::X;
        bool positive = true;
    };

    void setState(ActionId action, bool down);

    std::unordered_map<ActionId, std::vector<Binding>> bindings_;
    std::unordered_map<ActionId, bool> current_;
    std::unordered_map<ActionId, bool> previous_;
    sf::Keyboard::Key lastKey_ = sf::Keyboard::Unknown;

    /// Sticks rest slightly off centre, so a raw reading of anything non-zero
    /// would have the player drifting forever.
    static constexpr float kAxisDeadzone = 35.0f;  // SFML reports axes as -100..100
};

}  // namespace engine
