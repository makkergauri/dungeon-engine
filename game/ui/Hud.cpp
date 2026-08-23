#include "Hud.h"

#include <algorithm>
#include <cmath>

namespace game {

using engine::Color;
using engine::kLayerUI;

namespace {

/// Panel background plus border, used by every framed box on screen.
void panel(engine::Renderer& renderer, float x, float y, float w, float h, Color fill,
           Color border) {
    renderer.drawQuad(x + w * 0.5f, y + h * 0.5f, w, h, fill, kLayerUI);
    renderer.drawRectOutline(x, y, w, h, 2.0f, border, kLayerUI);
}

const Color kPanelFill{24, 21, 31, 238};
const Color kPanelEdge{62, 56, 78, 255};

}  // namespace

const std::vector<std::string>& Hud::mainMenuItems() {
    static const std::vector<std::string> items = {
        "Descend  (rooms and corridors)",
        "Descend  (natural caves)",
        "How to play",
        "Quit",
    };
    return items;
}


// Gameplay HUD

void Hud::drawHealthBar(engine::Renderer& renderer, int current, int max, float x, float y,
                        float width, float height) {
    const float fraction = max > 0 ? std::clamp(static_cast<float>(current) / max, 0.0f, 1.0f)
                                   : 0.0f;

    renderer.drawQuad(x + width * 0.5f, y + height * 0.5f, width, height,
                      Color{40, 20, 24, 230}, kLayerUI);

    const Color fill{static_cast<std::uint8_t>(200 - static_cast<int>(60 * fraction)),
                     static_cast<std::uint8_t>(60 + static_cast<int>(140 * fraction)),
                     static_cast<std::uint8_t>(60 + static_cast<int>(40 * fraction)), 255};

    if (fraction > 0.0f) {
        const float fillWidth = width * fraction;
        renderer.drawQuad(x + fillWidth * 0.5f, y + height * 0.5f, fillWidth, height, fill,
                          kLayerUI);
    }
    renderer.drawRectOutline(x, y, width, height, 2.0f, Color{15, 13, 18, 255}, kLayerUI);
}

void Hud::drawGameplay(engine::Renderer& renderer, const Stats& stats, float screenWidth,
                       float screenHeight) {
    constexpr float kPad = 16.0f;

    panel(renderer, kPad, kPad, 300.0f, 74.0f, kPanelFill, kPanelEdge);
    drawHealthBar(renderer, stats.health, stats.maxHealth, kPad + 12.0f, kPad + 12.0f, 200.0f,
                  18.0f);

    // Inventory strip, bottom left.
    const float slotSize = 34.0f;
    const float slotY = screenHeight - kPad - slotSize;
    for (std::size_t i = 0; i < stats.inventory.size(); ++i) {
        const float slotX = kPad + i * (slotSize + 6.0f);
        const InventorySlot& slot = stats.inventory[i];
        renderer.drawQuad(slotX + slotSize * 0.5f, slotY + slotSize * 0.5f, slotSize, slotSize,
                          kPanelFill, kLayerUI);
        if (!slot.empty()) {
            renderer.drawQuad(slotX + slotSize * 0.5f, slotY + slotSize * 0.5f, 16.0f, 16.0f,
                              itemColor(slot.type), kLayerUI);
        }
        renderer.drawRectOutline(slotX, slotY, slotSize, slotSize, 2.0f, kPanelEdge, kLayerUI);
    }

    const std::string objective = stats.enemiesRemaining > 0
        ? ("Find the stairs      Enemies left: " + std::to_string(stats.enemiesRemaining))
        : "Floor cleared -- find the stairs";
    panel(renderer, screenWidth * 0.5f - 210.0f, screenHeight - 48.0f, 420.0f, 30.0f,
          Color{18, 16, 24, 200}, Color{50, 46, 62, 255});

    renderer.flush();  // panels must land before the text that sits on them

    renderer.drawText(std::to_string(std::max(0, stats.health)) + " / " +
                          std::to_string(stats.maxHealth),
                      kPad + 220.0f, kPad + 11.0f, 15, palette::kUiText);
    renderer.drawText("Floor " + std::to_string(stats.floorNumber), kPad + 12.0f,
                      kPad + 38.0f, 18, palette::kUiText);
    renderer.drawText("ATK " + std::to_string(stats.attackDamage), kPad + 108.0f,
                      kPad + 42.0f, 14, palette::kUiDim);
    renderer.drawText(std::to_string(stats.coins) + " gold", kPad + 172.0f, kPad + 42.0f, 14,
                      palette::kGold);

    renderer.drawText(objective, screenWidth * 0.5f, screenHeight - 41.0f, 15,
                      palette::kUiDim, true);
    renderer.drawText("Esc  pause      H  help", kPad + 160.0f, screenHeight - kPad - 22.0f,
                      13, palette::kUiDim);

    for (std::size_t i = 0; i < stats.inventory.size(); ++i) {
        const InventorySlot& slot = stats.inventory[i];
        if (slot.empty()) continue;
        renderer.drawText(std::to_string(slot.count),
                          kPad + i * (slotSize + 6.0f) + slotSize - 12.0f,
                          slotY + slotSize - 16.0f, 12, palette::kUiText);
    }

    // Contextual prompts, only shown when actionable.
    if (stats.onStairs) {
        renderer.drawText("Press E to descend", screenWidth * 0.5f, screenHeight - 96.0f, 22,
                          palette::kStairs, true);
    } else if (stats.nearMerchant) {
        renderer.drawText("Press E to trade", screenWidth * 0.5f, screenHeight - 96.0f, 22,
                          Color{198, 168, 240, 255}, true);
    }
}


// Minimap

void Hud::drawMinimap(engine::Renderer& renderer, const Dungeon& dungeon,
                      const std::vector<std::uint8_t>& explored,
                      const std::vector<MapBlip>& blips, float screenWidth) {
    constexpr float kPad = 16.0f;
    constexpr float kScale = 2.0f;  // pixels per tile

    const float mapW = dungeon.widthInTiles() * kScale;
    const float mapH = dungeon.heightInTiles() * kScale;
    const float originX = screenWidth - kPad - mapW;
    const float originY = kPad;

    panel(renderer, originX - 4.0f, originY - 4.0f, mapW + 8.0f, mapH + 8.0f,
          Color{12, 11, 16, 225}, kPanelEdge);

    for (int y = 0; y < dungeon.heightInTiles(); ++y) {
        for (int x = 0; x < dungeon.widthInTiles(); ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * dungeon.widthInTiles() + x;
            if (index >= explored.size() || !explored[index]) continue;
            if (!dungeon.isFloor(x, y)) continue;

            const Color tint = dungeon.at(x, y) == Tile::StairsDown
                                   ? Color{120, 220, 150, 255}
                                   : Color{92, 88, 108, 255};
            renderer.drawQuad(originX + x * kScale + kScale * 0.5f,
                              originY + y * kScale + kScale * 0.5f, kScale, kScale, tint,
                              kLayerUI);
        }
    }

    for (const MapBlip& blip : blips) {
        const float size = blip.large ? 6.0f : 4.0f;
        renderer.drawQuad(originX + (blip.x / dungeon.tileSize()) * kScale,
                          originY + (blip.y / dungeon.tileSize()) * kScale, size, size,
                          blip.color, kLayerUI);
    }
}


// Menus


void Hud::drawMainMenu(engine::Renderer& renderer, const GameAssets& assets, int selected,
                       float elapsed, float screenWidth, float screenHeight) {
    const float cx = screenWidth * 0.5f;
    const float f = 16.0f;

    // --- Backdrop -----------------------------------------------------------
    const float drift = std::fmod(elapsed * 10.0f, 96.0f);
    const int cols = static_cast<int>(screenWidth / 96.0f) + 3;
    const int rows = static_cast<int>(screenHeight / 96.0f) + 3;
    for (int y = -1; y < rows; ++y) {
        for (int x = -1; x < cols; ++x) {
            const int variant = ((x * 7) ^ (y * 13)) & 3;
            const engine::TextureRect source =
                assets.hasAtlas() ? assets.tileFloor[variant] : engine::TextureRect{};
            renderer.drawQuad(x * 96.0f + drift, y * 96.0f + drift * 0.6f, 96.0f, 96.0f,
                              Color{150, 142, 172, 255}, engine::kLayerFloor, assets.atlas,
                              source);
        }
    }


    const int bands = 14;
    for (int i = 0; i < bands; ++i) {
        const float t = static_cast<float>(i) / bands;
        const std::uint8_t alpha = static_cast<std::uint8_t>(28 + t * 40);
        const float inset = t * screenHeight * 0.55f;
        const float thickness = screenHeight * 0.05f;
        renderer.drawQuad(cx, inset, screenWidth, thickness, Color{8, 7, 12, alpha},
                          engine::kLayerDecal);
        renderer.drawQuad(cx, screenHeight - inset, screenWidth, thickness,
                          Color{8, 7, 12, alpha}, engine::kLayerDecal);
        renderer.drawQuad(inset, screenHeight * 0.5f, thickness, screenHeight,
                          Color{8, 7, 12, alpha}, engine::kLayerDecal);
        renderer.drawQuad(screenWidth - inset, screenHeight * 0.5f, thickness, screenHeight,
                          Color{8, 7, 12, alpha}, engine::kLayerDecal);
    }
    renderer.drawQuad(cx, screenHeight * 0.5f, screenWidth, screenHeight,
                      Color{14, 11, 22, 120}, engine::kLayerDecal);


    for (int i = 0; i < 46; ++i) {
        const float seedX = std::fmod(std::sin(i * 12.9898f) * 43758.5453f, 1.0f);
        const float seedS = 0.35f + std::fmod(std::sin(i * 78.233f) * 12345.678f, 0.65f);
        const float x = std::fabs(seedX) * screenWidth;
        const float y = screenHeight - std::fmod(elapsed * 34.0f * seedS + i * 137.0f,
                                                 screenHeight + 60.0f);
        const float sway = std::sin(elapsed * 1.4f + i) * 14.0f;
        const float size = 2.0f + std::fabs(seedX) * 3.0f;
        const std::uint8_t alpha =
            static_cast<std::uint8_t>(70 + 110 * (y / std::max(1.0f, screenHeight)));
        renderer.drawQuad(x + sway, y, size, size, Color{255, 170, 80, alpha},
                          engine::kLayerDecal);
    }

    // --- Title --------------------------------------------------------------
    const float titleY = screenHeight * 0.16f;
    const float plateW = std::min(760.0f, screenWidth * 0.62f);

    renderer.drawQuad(cx, titleY + 34.0f, plateW, 132.0f, Color{16, 13, 24, 225}, kLayerUI);
    renderer.drawRectOutline(cx - plateW * 0.5f, titleY - 32.0f, plateW, 132.0f, 3.0f,
                             Color{126, 96, 190, 255}, kLayerUI);
    // Inner hairline: two borders a few pixels apart read as "engraved" and cost
    // one extra call.
    renderer.drawRectOutline(cx - plateW * 0.5f + 7.0f, titleY - 25.0f, plateW - 14.0f,
                             118.0f, 1.0f, Color{74, 58, 112, 255}, kLayerUI);

    // Torches flanking the plate, using the animated atlas frames.
    if (assets.hasAtlas()) {
        const int flame = (static_cast<int>(elapsed * 7.0f) % 2);
        const engine::TextureRect torch{f * flame, f * 13, f, f};
        const float glow = 0.6f + 0.4f * std::sin(elapsed * 6.0f);
        for (int side = 0; side < 2; ++side) {
            const float tx = cx + (side == 0 ? -1.0f : 1.0f) * (plateW * 0.5f + 46.0f);

        
            for (int ring = 4; ring >= 1; --ring) {
                const float size = 26.0f + ring * 13.0f;
                const std::uint8_t alpha =
                    static_cast<std::uint8_t>((14 + glow * 10) / static_cast<float>(ring));
                renderer.drawQuad(tx, titleY + 26.0f, size, size,
                                  Color{255, 158, 70, alpha}, kLayerUI);
            }
            renderer.drawQuad(tx, titleY + 26.0f, 58.0f, 58.0f, engine::colors::kWhite,
                              kLayerUI, assets.atlas, torch);
        }
    }
    renderer.flush();

    // Layered text for a cheap glow: dark offset copies first, then bright core.
    renderer.drawText("DUNGEON", cx + 4.0f, titleY + 6.0f, 68, Color{40, 20, 70, 220}, true);
    renderer.drawText("DUNGEON", cx, titleY, 68, Color{236, 214, 255, 255}, true);
    renderer.drawText("Eight floors down.  One way out.", cx, titleY + 56.0f, 17,
                      Color{178, 156, 210, 255}, true);

    // --- Cast row -----------------------------------------------------------
    const float castY = screenHeight * 0.42f;
    if (assets.hasAtlas()) {
        const int walkFrame = static_cast<int>(elapsed * 8.0f) % 4;
        const float spacing = std::min(150.0f, screenWidth * 0.11f);
        struct Cast { engine::TextureRect rect; const char* name; Color color; };
        const Cast cast[4] = {
            {{f * walkFrame, f * 1, f, f}, "You", palette::kPlayer},
            {{f * walkFrame, f * 4, f, f}, "Shambler", palette::kShambler},
            {{f * (walkFrame % 2), f * 5, f, f}, "Sentinel", palette::kSentinel},
            {{f * walkFrame, f * 6, f, f}, "Skitterer", palette::kSkitterer},
        };
        for (int i = 0; i < 4; ++i) {
            const float x = cx + (i - 1.5f) * spacing;
            const float bob = std::sin(elapsed * 3.0f + i * 1.1f) * 4.0f;
            // Plinth, so each figure stands on something instead of floating.
            renderer.drawQuad(x, castY + 40.0f, 62.0f, 10.0f, Color{0, 0, 0, 90}, kLayerUI);
            renderer.drawQuad(x, castY + bob, 68.0f, 68.0f, engine::colors::kWhite, kLayerUI,
                              assets.atlas, cast[i].rect);
        }
        renderer.flush();
        for (int i = 0; i < 4; ++i) {
            renderer.drawText(cast[i].name, cx + (i - 1.5f) * spacing, castY + 52.0f, 14,
                              cast[i].color, true);
        }
    }

    // --- Menu ---------------------------------------------------------------
    const std::vector<std::string>& items = mainMenuItems();
    const float rowStart = screenHeight * 0.60f;
    const float rowStep = 48.0f;
    const float rowW = std::min(520.0f, screenWidth * 0.42f);

    for (std::size_t i = 0; i < items.size(); ++i) {
        const float y = rowStart + i * rowStep;
        const bool active = static_cast<int>(i) == selected;
        renderer.drawQuad(cx, y + 10.0f, rowW, 36.0f,
                          active ? Color{62, 46, 104, 248} : Color{20, 17, 27, 120},
                          kLayerUI);
        if (active) {
            const float pulse = 0.5f + 0.5f * std::sin(elapsed * 5.0f);
            renderer.drawRectOutline(cx - rowW * 0.5f, y - 9.0f, rowW, 38.0f, 2.0f,
                                     Color{170, 130, 240,
                                           static_cast<std::uint8_t>(150 + pulse * 105)},
                                     kLayerUI);
        }
    }
    renderer.flush();

    for (std::size_t i = 0; i < items.size(); ++i) {
        const bool active = static_cast<int>(i) == selected;
        renderer.drawText(items[i], cx, rowStart + i * rowStep, 20,
                          active ? Color{255, 250, 240, 255} : Color{150, 144, 162, 255},
                          true);
    }

    renderer.drawText("Arrow keys to choose        Enter to confirm", cx,
                      screenHeight - 62.0f, 15, Color{140, 130, 160, 255}, true);
}

void Hud::drawLegendRow(engine::Renderer& renderer, const GameAssets& assets,
                        engine::TextureRect source, Color fallback, const std::string& name,
                        const std::string& detail, float x, float y) {
    renderer.drawQuad(x + 18.0f, y + 14.0f, 34.0f, 34.0f,
                      assets.hasAtlas() ? engine::colors::kWhite : fallback, kLayerUI,
                      assets.atlas, assets.hasAtlas() ? source : engine::TextureRect{});
    renderer.flush();
    renderer.drawText(name, x + 46.0f, y, 16, palette::kUiText);
    renderer.drawText(detail, x + 46.0f, y + 19.0f, 12, palette::kUiDim);
}

void Hud::drawLegend(engine::Renderer& renderer, const GameAssets& assets, float screenWidth,
                     float screenHeight) {
    renderer.drawQuad(screenWidth * 0.5f, screenHeight * 0.5f, screenWidth, screenHeight,
                      Color{10, 9, 14, 232}, kLayerUI);
    panel(renderer, 40.0f, 30.0f, screenWidth - 80.0f, screenHeight - 60.0f,
          Color{22, 19, 30, 245}, Color{92, 76, 150, 255});
    renderer.flush();

    renderer.drawText("HOW TO PLAY", screenWidth * 0.5f, 48.0f, 30,
                      Color{206, 180, 250, 255}, true);

    const float f = 16.0f;
    const float leftX = 70.0f;
    const float rightX = screenWidth * 0.5f + 20.0f;
    float y = 96.0f;

    renderer.drawText("CONTROLS", leftX, y, 17, palette::kGold);
    y += 26.0f;
    const char* controls[5] = {
        "WASD / arrows    move",
        "Space            attack in the way you face",
        "E                stairs or merchant",
        "Esc  pause       H  this screen",
        "F1               performance overlay",
    };
    for (const char* line : controls) {
        renderer.drawText(line, leftX, y, 13, palette::kUiDim);
        y += 20.0f;
    }

    y += 12.0f;
    renderer.drawText("ENEMIES", leftX, y, 17, palette::kGold);
    y += 28.0f;
    drawLegendRow(renderer, assets, {0, f * 4, f, f}, palette::kShambler, "Shambler",
                  "Slow and tough. Walks straight at you.", leftX, y);
    y += 46.0f;
    drawLegendRow(renderer, assets, {0, f * 5, f, f}, palette::kSentinel, "Sentinel",
                  "Never moves. Shoots on sight -- use cover.", leftX, y);
    y += 46.0f;
    drawLegendRow(renderer, assets, {0, f * 6, f, f}, palette::kSkitterer, "Skitterer",
                  "Fast and erratic. Keep moving.", leftX, y);

    float ry = 96.0f;
    renderer.drawText("PICK UPS", rightX, ry, 17, palette::kGold);
    ry += 28.0f;
    drawLegendRow(renderer, assets, {f * 3, f * 10, f, f}, palette::kGold, "Gold",
                  "Spend it with the merchant.", rightX, ry);
    ry += 46.0f;
    drawLegendRow(renderer, assets, {0, f * 10, f, f}, itemColor(ItemType::HealthPotion),
                  "Potion", "Heals you, or is stored for later.", rightX, ry);
    ry += 46.0f;
    drawLegendRow(renderer, assets, {f, f * 10, f, f}, itemColor(ItemType::WeaponUpgrade),
                  "Whetstone", "Permanently raises your attack.", rightX, ry);
    ry += 46.0f;
    drawLegendRow(renderer, assets, {f * 2, f * 10, f, f}, itemColor(ItemType::ArmourScrap),
                  "Armour", "Permanently raises your max health.", rightX, ry);

    ry += 50.0f;
    renderer.drawText("PLACES", rightX, ry, 17, palette::kGold);
    ry += 28.0f;
    drawLegendRow(renderer, assets, {f * 4, f * 8, f, f}, palette::kStairs, "Stairs",
                  "Green on the minimap. Press E to descend.", rightX, ry);
    ry += 46.0f;
    drawLegendRow(renderer, assets, {f * 2, f * 13, f, f}, palette::kMerchant, "Merchant",
                  "Purple on the minimap. Press E to trade.", rightX, ry);

    renderer.drawText("Press H or Esc to go back", screenWidth * 0.5f, screenHeight - 52.0f,
                      15, palette::kUiDim, true);
}

void Hud::drawMenu(engine::Renderer& renderer, const std::string& title,
                   const std::vector<std::string>& lines, float screenWidth,
                   float screenHeight, Color titleColor) {
    renderer.drawQuad(screenWidth * 0.5f, screenHeight * 0.5f, screenWidth, screenHeight,
                      Color{10, 9, 14, 215}, kLayerUI);
    panel(renderer, screenWidth * 0.5f - 250.0f, screenHeight * 0.26f, 500.0f,
          screenHeight * 0.44f, Color{22, 19, 30, 245}, kPanelEdge);
    renderer.flush();

    renderer.drawText(title, screenWidth * 0.5f, screenHeight * 0.32f, 44, titleColor, true);

    float y = screenHeight * 0.46f;
    for (const std::string& line : lines) {
        renderer.drawText(line, screenWidth * 0.5f, y, 18, palette::kUiText, true);
        y += 30.0f;
    }
}

void Hud::drawShop(engine::Renderer& renderer, const std::array<Offer, 4>& offers, int coins,
                   float screenWidth, float screenHeight) {
    renderer.drawQuad(screenWidth * 0.5f, screenHeight * 0.5f, screenWidth, screenHeight,
                      Color{10, 9, 14, 215}, kLayerUI);

    const float panelW = 520.0f;
    const float panelH = 340.0f;
    const float panelX = screenWidth * 0.5f - panelW * 0.5f;
    const float panelY = screenHeight * 0.5f - panelH * 0.5f;
    panel(renderer, panelX, panelY, panelW, panelH, Color{26, 23, 34, 250},
          Color{92, 76, 150, 255});

    // Row backgrounds go down before any text, because the renderer batches by
    // layer rather than by call order -- a panel drawn later would cover them.
    for (std::size_t i = 0; i < offers.size(); ++i) {
        const float rowY = panelY + 78.0f + i * 54.0f;
        const bool affordable = !offers[i].soldOut && coins >= offers[i].price;
        renderer.drawQuad(screenWidth * 0.5f, rowY + 20.0f, panelW - 40.0f, 44.0f,
                          affordable ? Color{40, 36, 54, 255} : Color{30, 27, 36, 255},
                          kLayerUI);
    }
    renderer.flush();

    renderer.drawText("MERCHANT", screenWidth * 0.5f, panelY + 26.0f, 30,
                      Color{198, 168, 240, 255}, true);
    renderer.drawText("Gold: " + std::to_string(coins), screenWidth * 0.5f, panelY + 56.0f,
                      17, palette::kGold, true);

    for (std::size_t i = 0; i < offers.size(); ++i) {
        const Offer& offer = offers[i];
        const float y = panelY + 86.0f + i * 54.0f;
        const bool affordable = !offer.soldOut && coins >= offer.price;
        const Color text = affordable ? palette::kUiText : Color{110, 104, 118, 255};

        renderer.drawText(std::to_string(i + 1) + ".", panelX + 32.0f, y, 18, text);
        renderer.drawText(offer.name, panelX + 62.0f, y, 18, text);
        renderer.drawText(offer.detail, panelX + 62.0f, y + 21.0f, 13,
                          affordable ? palette::kUiDim : Color{86, 82, 94, 255});

        const std::string price = offer.soldOut ? "SOLD" : (std::to_string(offer.price) + "g");
        renderer.drawText(price, panelX + panelW - 92.0f, y + 6.0f, 18,
                          offer.soldOut ? Color{110, 104, 118, 255}
                                        : (affordable ? palette::kGold
                                                      : Color{130, 90, 80, 255}));
    }

    renderer.drawText("Press 1-4 to buy    Esc to leave", screenWidth * 0.5f,
                      panelY + panelH - 34.0f, 15, palette::kUiDim, true);
}

void Hud::drawDebug(engine::Renderer& renderer, float fps, int entityCount, int drawCalls,
                    int quads, int searches, std::size_t particles, float screenWidth) {
    (void)screenWidth;
    // Draw calls next to quad count is the pair worth watching: if they start
    // tracking each other, batching has stopped working.
    const std::string line = "fps " + std::to_string(static_cast<int>(fps)) + "  ents " +
                             std::to_string(entityCount) + "  quads " + std::to_string(quads) +
                             "  draws " + std::to_string(drawCalls) + "  A* " +
                             std::to_string(searches) + "  parts " +
                             std::to_string(particles);
    renderer.drawText(line, 20.0f, 110.0f, 13, palette::kUiDim);
}

}  