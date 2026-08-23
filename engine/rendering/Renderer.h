#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../ecs/System.h"
#include "Camera.h"
#include "Sprite.h"

namespace engine {

class Window;

/// One quad queued for drawing. Collecting these and sorting before touching the
/// GPU is what makes batching possible.
struct DrawCommand {
    TextureId texture = kNoTexture;
    float x = 0.0f, y = 0.0f;   // centre, world space
    float w = 0.0f, h = 0.0f;
    TextureRect source;
    Color tint;
    int layer = 0;
    bool flipX = false;
    float sortY = 0.0f;
};

/// Immediate-mode sprite renderer with sorting and batching.
///
/// The naive approach -- one window.draw() per sprite -- costs a state change
/// and a draw call each time, and a busy floor has a few hundred sprites. Here
/// every quad is appended to a vertex array and flushed only when the texture
/// changes, so a floor drawn from a single atlas costs a handful of draw calls
/// regardless of how many sprites are on it.
class Renderer {
public:
    bool init(sf::RenderTarget& target);

    /// Returns kNoTexture if the file is missing. Callers are expected to carry
    /// on: an art-free checkout should still be playable in flat colours rather
    /// than crashing on startup.
    TextureId loadTexture(const std::string& path);
    /// Register a texture built at runtime rather than loaded from disk.
    /// Used by the procedural art generator; keeps the renderer unaware of
    /// where pixels come from.
    TextureId createTexture(const sf::Image& image);

    bool loadFont(const std::string& path);
    bool hasFont() const { return fontLoaded_; }

    /// Switch to world space through the camera. Everything submitted after this
    /// is positioned in world units.
    void beginWorld(const Camera& camera);
    /// Switch to screen space for HUD and menus, where (0,0) is the top-left
    /// corner of the window no matter where the camera is.
    void beginUI();

    void submit(const DrawCommand& command);

    /// Convenience wrappers used by the tile and HUD drawing code.
    void drawQuad(float centerX, float centerY, float w, float h, Color tint,
                  int layer = kLayerDecal, TextureId texture = kNoTexture,
                  TextureRect source = {});
    void drawRectOutline(float x, float y, float w, float h, float thickness, Color tint,
                         int layer = kLayerUI);

    /// Walks the sprite pool and queues every visible sprite.
    void submitSprites(Registry& registry);

    /// Sort, batch and issue the draw calls.
    void flush();

    void drawText(const std::string& text, float x, float y, unsigned int size, Color tint,
                  bool centered = false);

    /// Draw-call count for the last flush. Worth putting on the debug overlay --
    /// it is the single number that tells you whether batching is working.
    int lastDrawCallCount() const { return lastDrawCalls_; }
    int lastQuadCount() const { return lastQuads_; }

private:
    void appendQuad(sf::VertexArray& vertices, const DrawCommand& command,
                    const sf::Texture* texture) const;

    sf::RenderTarget* target_ = nullptr;
    std::vector<DrawCommand> queue_;
    std::vector<std::unique_ptr<sf::Texture>> textures_;  // index 0 is the 1x1 white pixel
    std::unordered_map<std::string, TextureId> textureLookup_;
    sf::VertexArray batch_;
    sf::Font font_;
    sf::Text text_;
    bool fontLoaded_ = false;
    int lastDrawCalls_ = 0;
    int lastQuads_ = 0;
};

}  // namespace engine