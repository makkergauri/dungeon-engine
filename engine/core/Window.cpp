#include "Window.h"

namespace engine {

bool Window::create(const Config& config) {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 0;  // pixel art; AA would blur the edges

    window_.create(sf::VideoMode(config.width, config.height), config.title,
                   sf::Style::Default, settings);
    if (!window_.isOpen()) return false;

    window_.setVerticalSyncEnabled(config.vsync);
    if (!config.vsync && config.frameLimit > 0) {
        window_.setFramerateLimit(config.frameLimit);
    }
    // Keeping the cursor visible: this is a keyboard game and hiding the cursor
    // in a windowed build just annoys people trying to alt-tab.
    return true;
}

void Window::close() { window_.close(); }

bool Window::isOpen() const { return window_.isOpen(); }

void Window::clear(sf::Color color) { window_.clear(color); }

void Window::display() { window_.display(); }

bool Window::consumeResized(unsigned int& outWidth, unsigned int& outHeight) {
    if (!resized_) return false;
    resized_ = false;
    outWidth = lastResize_.x;
    outHeight = lastResize_.y;
    return true;
}

}  // namespace engine
