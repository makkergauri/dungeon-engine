#pragma once

#include <SFML/Graphics.hpp>

#include <string>

namespace engine {

/// Thin wrapper over sf::RenderWindow.
///
/// It is deliberately thin. The value is not in hiding SFML -- it is in having
/// exactly one file that mentions window creation, so swapping the backend later
/// touches one file instead of every system that draws something.
class Window {
public:
    struct Config {
        unsigned int width = 1280;
        unsigned int height = 720;
        std::string title = "Untitled";
        bool vsync = true;
        /// Only used when vsync is off. Leaving both unbounded pins a core at
        /// 100% redrawing frames nobody sees.
        unsigned int frameLimit = 0;
    };

    bool create(const Config& config);
    void close();
    bool isOpen() const;

    /// Pump the OS event queue, forwarding each event to `handler`. Window-level
    /// concerns (close, resize) are dealt with here; everything else is passed on.
    template <typename Handler>
    void pollEvents(Handler&& handler) {
        sf::Event event;
        while (window_.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window_.close();
            } else if (event.type == sf::Event::Resized) {
                lastResize_ = {event.size.width, event.size.height};
                resized_ = true;
            }
            handler(event);
        }
    }

    void clear(sf::Color color = sf::Color(18, 16, 24));
    void display();

    sf::RenderWindow& handle() { return window_; }
    unsigned int width() const { return window_.getSize().x; }
    unsigned int height() const { return window_.getSize().y; }

    /// True once after the window has been resized, so the camera can resize its
    /// viewport without polling the window size every frame.
    bool consumeResized(unsigned int& outWidth, unsigned int& outHeight);

private:
    sf::RenderWindow window_;
    sf::Vector2u lastResize_;
    bool resized_ = false;
};

}  // namespace engine
