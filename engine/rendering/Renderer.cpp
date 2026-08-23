#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace engine {
namespace {

sf::Color toSf(const Color& c) { return sf::Color(c.r, c.g, c.b, c.a); }

}  // namespace

bool Renderer::init(sf::RenderTarget& target) {
    target_ = &target;
    batch_.setPrimitiveType(sf::Triangles);

    // Slot 0 is a single white pixel. Untextured quads sample it and get their
    // colour entirely from the vertex tint, which means solid rectangles and
    // textured sprites share one code path -- and, more usefully, one batch.
    sf::Image white;
    white.create(1, 1, sf::Color::White);
    auto texture = std::make_unique<sf::Texture>();
    if (!texture->loadFromImage(white)) return false;
    textures_.push_back(std::move(texture));

    return true;
}

TextureId Renderer::loadTexture(const std::string& path) {
    auto cached = textureLookup_.find(path);
    if (cached != textureLookup_.end()) return cached->second;

    auto texture = std::make_unique<sf::Texture>();
    if (!texture->loadFromFile(path)) {
        std::cerr << "[renderer] missing texture: " << path
                  << " (falling back to flat colour)\n";
        textureLookup_[path] = kNoTexture;
        return kNoTexture;
    }
    // Pixel art only looks right unfiltered; smoothing turns crisp 16x16 sprites
    // into mush the moment they are scaled up.
    texture->setSmooth(false);

    const TextureId id = static_cast<TextureId>(textures_.size());
    textures_.push_back(std::move(texture));
    textureLookup_[path] = id;
    return id;
}

TextureId Renderer::createTexture(const sf::Image& image) {
    auto texture = std::make_unique<sf::Texture>();
    if (!texture->loadFromImage(image)) return kNoTexture;
    texture->setSmooth(false);

    const TextureId id = static_cast<TextureId>(textures_.size());
    textures_.push_back(std::move(texture));
    return id;
}

bool Renderer::loadFont(const std::string& path) {
    fontLoaded_ = font_.loadFromFile(path);
    if (fontLoaded_) {
        text_.setFont(font_);
    } else {
        std::cerr << "[renderer] missing font: " << path << " (UI text disabled)\n";
    }
    return fontLoaded_;
}

void Renderer::beginWorld(const Camera& camera) {
    flush();
    sf::View view;
    view.setSize(camera.viewportWidth(), camera.viewportHeight());
    // Rounding the centre to whole pixels stops pixel-art tiles shimmering as
    // the camera slides between texel boundaries.
    view.setCenter(std::round(camera.x()), std::round(camera.y()));
    target_->setView(view);
}

void Renderer::beginUI() {
    flush();
    const sf::Vector2u size = target_->getSize();
    sf::View view(sf::FloatRect(0.0f, 0.0f, static_cast<float>(size.x),
                                static_cast<float>(size.y)));
    target_->setView(view);
}

void Renderer::submit(const DrawCommand& command) { queue_.push_back(command); }

void Renderer::drawQuad(float centerX, float centerY, float w, float h, Color tint,
                        int layer, TextureId texture, TextureRect source) {
    DrawCommand command;
    command.texture = texture;
    command.x = centerX;
    command.y = centerY;
    command.w = w;
    command.h = h;
    command.tint = tint;
    command.layer = layer;
    command.source = source;
    command.sortY = centerY;
    queue_.push_back(command);
}

void Renderer::drawRectOutline(float x, float y, float w, float h, float thickness,
                               Color tint, int layer) {
    drawQuad(x + w * 0.5f, y + thickness * 0.5f, w, thickness, tint, layer);
    drawQuad(x + w * 0.5f, y + h - thickness * 0.5f, w, thickness, tint, layer);
    drawQuad(x + thickness * 0.5f, y + h * 0.5f, thickness, h, tint, layer);
    drawQuad(x + w - thickness * 0.5f, y + h * 0.5f, thickness, h, tint, layer);
}

void Renderer::submitSprites(Registry& registry) {
    registry.each<SpriteComponent, Transform>(
        [&](Entity, SpriteComponent& sprite, Transform& transform) {
            if (!sprite.visible) return;
            DrawCommand command;
            command.texture = sprite.texture;
            command.x = transform.x;
            command.y = transform.y + sprite.offsetY;
            command.w = sprite.width * transform.scaleX;
            command.h = sprite.height * transform.scaleY;
            command.source = sprite.source;
            command.tint = sprite.tint;
            command.layer = sprite.layer;
            command.flipX = sprite.flipX;
            // Sort by feet, not centre, so a tall sprite standing behind a short
            // one still resolves correctly.
            command.sortY = transform.y + sprite.height * 0.5f;
            queue_.push_back(command);
        });
}

void Renderer::appendQuad(sf::VertexArray& vertices, const DrawCommand& command,
                          const sf::Texture* texture) const {
    const float halfW = command.w * 0.5f;
    const float halfH = command.h * 0.5f;
    const float left = command.x - halfW;
    const float right = command.x + halfW;
    const float top = command.y - halfH;
    const float bottom = command.y + halfH;

    // Default to the whole texture when no source rect was given.
    TextureRect src = command.source;
    if (src.w <= 0.0f || src.h <= 0.0f) {
        const sf::Vector2u size = texture->getSize();
        src = TextureRect{0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y)};
    }

    float u0 = src.x;
    float u1 = src.x + src.w;
    if (command.flipX) std::swap(u0, u1);  // mirroring is a UV swap, not a second sprite
    const float v0 = src.y;
    const float v1 = src.y + src.h;

    const sf::Color tint = toSf(command.tint);

    // Two triangles. sf::Quads exists in SFML 2 but was removed in SFML 3, and
    // triangles cost nothing extra here.
    const sf::Vertex topLeft(sf::Vector2f(left, top), tint, sf::Vector2f(u0, v0));
    const sf::Vertex topRight(sf::Vector2f(right, top), tint, sf::Vector2f(u1, v0));
    const sf::Vertex bottomRight(sf::Vector2f(right, bottom), tint, sf::Vector2f(u1, v1));
    const sf::Vertex bottomLeft(sf::Vector2f(left, bottom), tint, sf::Vector2f(u0, v1));

    vertices.append(topLeft);
    vertices.append(topRight);
    vertices.append(bottomRight);
    vertices.append(topLeft);
    vertices.append(bottomRight);
    vertices.append(bottomLeft);
}

void Renderer::flush() {
    if (!target_ || queue_.empty()) return;

    // Sort by layer, then by depth, then by texture.
    //
    // Texture is last on purpose. Putting it first would give perfect batching
    // but wrong overlap order, and a player notices a goblin drawn on top of a
    // wall long before they notice a few extra draw calls. With a single atlas
    // -- which is how the art is authored -- the texture key is constant anyway
    // and this degenerates into one batch per layer.
    std::sort(queue_.begin(), queue_.end(), [](const DrawCommand& a, const DrawCommand& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        if (a.sortY != b.sortY) return a.sortY < b.sortY;
        return a.texture < b.texture;
    });

    lastDrawCalls_ = 0;
    lastQuads_ = static_cast<int>(queue_.size());

    sf::RenderStates states;
    TextureId currentTexture = queue_.front().texture;
    batch_.clear();

    auto drawBatch = [&]() {
        if (batch_.getVertexCount() == 0) return;
        const std::size_t index = currentTexture < textures_.size() ? currentTexture : 0;
        states.texture = textures_[index].get();
        target_->draw(batch_, states);
        batch_.clear();
        ++lastDrawCalls_;
    };

    for (const DrawCommand& command : queue_) {
        if (command.texture != currentTexture) {
            drawBatch();
            currentTexture = command.texture;
        }
        const std::size_t index = currentTexture < textures_.size() ? currentTexture : 0;
        appendQuad(batch_, command, textures_[index].get());
    }
    drawBatch();

    queue_.clear();
}

void Renderer::drawText(const std::string& string, float x, float y, unsigned int size,
                        Color tint, bool centered) {
    if (!fontLoaded_ || !target_) return;

    // Text goes straight to the target rather than through the batch: it needs
    // the font's own texture and its own transform, so it could never share a
    // batch with sprite quads anyway.
    flush();

    text_.setString(string);
    text_.setCharacterSize(size);
    text_.setFillColor(toSf(tint));

    if (centered) {
        const sf::FloatRect bounds = text_.getLocalBounds();
        // getLocalBounds() includes the glyph's own left/top bearing, so it has
        // to be subtracted or the centring drifts with the first character.
        text_.setOrigin(bounds.left + bounds.width * 0.5f, bounds.top + bounds.height * 0.5f);
    } else {
        text_.setOrigin(0.0f, 0.0f);
    }

    text_.setPosition(std::round(x), std::round(y));
    target_->draw(text_);
}

}  // namespace engine