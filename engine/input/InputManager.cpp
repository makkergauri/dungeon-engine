#include "InputManager.h"

#include <cmath>

namespace engine {

void InputManager::bindKey(ActionId action, sf::Keyboard::Key key) {
    Binding binding;
    binding.kind = Binding::Kind::Key;
    binding.key = key;
    bindings_[action].push_back(binding);
    current_.emplace(action, false);
    previous_.emplace(action, false);
}

void InputManager::bindGamepadButton(ActionId action, unsigned int button) {
    Binding binding;
    binding.kind = Binding::Kind::Button;
    binding.button = button;
    bindings_[action].push_back(binding);
    current_.emplace(action, false);
    previous_.emplace(action, false);
}

void InputManager::bindGamepadAxis(ActionId action, sf::Joystick::Axis axis, bool positive) {
    Binding binding;
    binding.kind = Binding::Kind::Axis;
    binding.axis = axis;
    binding.positive = positive;
    bindings_[action].push_back(binding);
    current_.emplace(action, false);
    previous_.emplace(action, false);
}

void InputManager::clearBindings(ActionId action) { bindings_[action].clear(); }

void InputManager::beginFrame() {
    previous_ = current_;
    lastKey_ = sf::Keyboard::Unknown;
}

void InputManager::setState(ActionId action, bool down) { current_[action] = down; }

void InputManager::handleEvent(const sf::Event& event) {
    if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased) {
        const bool down = event.type == sf::Event::KeyPressed;
        if (down) lastKey_ = event.key.code;

        for (const auto& [action, list] : bindings_) {
            for (const Binding& binding : list) {
                if (binding.kind == Binding::Kind::Key && binding.key == event.key.code) {
                    setState(action, down);
                }
            }
        }
    } else if (event.type == sf::Event::JoystickButtonPressed ||
               event.type == sf::Event::JoystickButtonReleased) {
        const bool down = event.type == sf::Event::JoystickButtonPressed;
        for (const auto& [action, list] : bindings_) {
            for (const Binding& binding : list) {
                if (binding.kind == Binding::Kind::Button &&
                    binding.button == event.joystickButton.button) {
                    setState(action, down);
                }
            }
        }
    } else if (event.type == sf::Event::LostFocus) {
        // Drop every held input when the window loses focus. Otherwise alt-tab
        // while walking leaves the player sprinting into a wall on return,
        // because the key-up event went to a different window.
        for (auto& [action, down] : current_) down = false;
    }
}

void InputManager::pollGamepad() {
    if (!sf::Joystick::isConnected(0)) return;

    for (const auto& [action, list] : bindings_) {
        for (const Binding& binding : list) {
            if (binding.kind != Binding::Kind::Axis) continue;
            const float value = sf::Joystick::getAxisPosition(0, binding.axis);
            const bool active = binding.positive ? value > kAxisDeadzone
                                                 : value < -kAxisDeadzone;
            // Only ever turn an axis binding *on* here. Turning it off would
            // stomp a keyboard binding for the same action that is currently
            // held, and the player would see their movement stutter.
            if (active) setState(action, true);
        }
    }
}

bool InputManager::isDown(ActionId action) const {
    auto it = current_.find(action);
    return it != current_.end() && it->second;
}

bool InputManager::wasPressed(ActionId action) const {
    auto now = current_.find(action);
    auto before = previous_.find(action);
    const bool down = now != current_.end() && now->second;
    const bool wasDown = before != previous_.end() && before->second;
    return down && !wasDown;
}

bool InputManager::wasReleased(ActionId action) const {
    auto now = current_.find(action);
    auto before = previous_.find(action);
    const bool down = now != current_.end() && now->second;
    const bool wasDown = before != previous_.end() && before->second;
    return !down && wasDown;
}

float InputManager::axis(ActionId negative, ActionId positive) const {
    float value = 0.0f;
    if (isDown(negative)) value -= 1.0f;
    if (isDown(positive)) value += 1.0f;
    return value;
}

void InputManager::reset() {
    for (auto& [action, down] : current_) down = false;
    previous_ = current_;
}

}  // namespace engine
