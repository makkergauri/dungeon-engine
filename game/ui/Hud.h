#pragma once

#include <array>
#include <string>
#include <vector>

#include "../../engine/rendering/Renderer.h"
#include "../GameCommon.h"
#include "../generation/DungeonGenerator.h"
#include "../shop/Shop.h"

namespace game {
struct MapBlip {
    float x = 0.0f;
    float y = 0.0f;
    engine::Color color;
    bool large = false;
};

class Hud {
public:
    struct Stats {
        int health = 0;
        int maxHealth = 0;
        int floorNumber = 1;
        int enemiesRemaining = 0;
        int coins = 0;
        int attackDamage = 1;
        std::array<InventorySlot, 4> inventory{};
        bool onStairs = false;
        bool nearMerchant = false;
        bool stairsFound = false;
    };

    void drawGameplay(engine::Renderer& renderer, const Stats& stats, float screenWidth,
                      float screenHeight);

    /// Explored-only minimap. Fog of war is not decoration here: a fully
    /// revealed map removes any reason to explore, and a map with no marks at
    /// all leaves the player wandering with no idea where the exit is.
    void drawMinimap(engine::Renderer& renderer, const Dungeon& dungeon,
                     const std::vector<std::uint8_t>& explored,
                     const std::vector<MapBlip>& blips, float screenWidth);

    /// Animated title screen with a keyboard-driven selector.
    void drawMainMenu(engine::Renderer& renderer, const GameAssets& assets, int selected,
                      float elapsed, float screenWidth, float screenHeight);

    /// The how-to-play screen: every sprite in the game, drawn next to what it
    /// is and what it does. This is the single most useful screen in the whole
    /// project for a new player -- without it, a top-down pixel game is a set of
    /// coloured shapes with no legend.
    void drawLegend(engine::Renderer& renderer, const GameAssets& assets, float screenWidth,
                    float screenHeight);

    /// Shared chrome for the simple full-screen menus.
    void drawMenu(engine::Renderer& renderer, const std::string& title,
                  const std::vector<std::string>& lines, float screenWidth,
                  float screenHeight, engine::Color titleColor = palette::kUiText);

    void drawShop(engine::Renderer& renderer, const std::array<Offer, 4>& offers, int coins,
                  float screenWidth, float screenHeight);

    void drawDebug(engine::Renderer& renderer, float fps, int entityCount, int drawCalls,
                   int quads, int searches, std::size_t particles, float screenWidth);

    static const std::vector<std::string>& mainMenuItems();

private:
    void drawHealthBar(engine::Renderer& renderer, int current, int max, float x, float y,
                       float width, float height);
    /// One row of the legend: a sprite, a name, and a line of explanation.
    void drawLegendRow(engine::Renderer& renderer, const GameAssets& assets,
                       engine::TextureRect source, engine::Color fallback,
                       const std::string& name, const std::string& detail, float x, float y);
};

} 