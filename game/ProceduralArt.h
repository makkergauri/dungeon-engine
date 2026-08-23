#pragma once

#include <SFML/Graphics/Image.hpp>

namespace game {

sf::Image generateAtlas();

/// Size of one frame, in pixels. Everything downstream derives from this.
inline constexpr int kFrame = 16;
inline constexpr int kAtlasCols = 8;
inline constexpr int kAtlasRows = 14;

}  // namespace game