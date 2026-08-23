#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../engine/core/Game.h"
#include "../engine/physics/Physics.h"
#include "GameCommon.h"
#include "GameEvents.h"
#include "combat/CombatSystem.h"
#include "entities/Enemy.h"
#include "entities/Item.h"
#include "entities/Player.h"
#include "generation/DungeonGenerator.h"
#include "shop/Shop.h"
#include "ui/Hud.h"

namespace game {
class DungeonGame final : public engine::Game {
public:
    enum class Screen { MainMenu, Playing, Paused, Shop, Legend, GameOver, Victory };

protected:
    bool onInit() override;
    void onFixedUpdate(float dt) override;
    void onUpdate(float dt) override;
    void onRender() override;
    void onEvent(const sf::Event& event) override;
    void onShutdown() override;

private:
    void bindInput();
    void loadAssets();
    void wireEvents();

    void startNewRun();
    void generateFloor(int floorNumber);
    void populateFloor(int floorNumber);
    /// Scatter non-interactive scenery. Purely visual, but it is most of what
    /// separates "a grid with things on it" from "a place".
    void scatterDecor(int floorNumber);
    void openShop();
    void tryPurchase(std::size_t slot);
    void confirmMenuChoice();

    /// Mark tiles near the player as seen, for the minimap's fog of war.
    void updateExplored();
    /// Health bars and damage numbers, drawn in world space over the sprites.
    void renderWorldOverlays();
    void addFloatingText(float x, float y, const std::string& text, engine::Color color);

    void descend();

    void renderTiles();
    void renderWorld();
    void updateHudStats();
    void setScreen(Screen screen);

    int enemiesAlive();
    bool playerAlive();

    // --- State ---
    Screen screen_ = Screen::MainMenu;
    GenerationStyle style_ = GenerationStyle::BSP;
    Dungeon dungeon_;
    DungeonGenerator generator_;
    GameAssets assets_;
    GameEvents events_;
    Hud hud_;
    Hud::Stats hudStats_;

    engine::PhysicsSystem* physics_ = nullptr;
    PlayerSystem* playerSystem_ = nullptr;
    EnemyAISystem* enemySystem_ = nullptr;
    CombatSystem* combatSystem_ = nullptr;

    engine::Entity player_ = engine::kNullEntity;
    engine::Entity merchant_ = engine::kNullEntity;
    Shop shop_;
    bool nearMerchant_ = false;

    
    int menuSelection_ = 0;
    Screen legendReturn_ = Screen::MainMenu;

    /// One byte per tile: has the player seen it. Drives the minimap.
    std::vector<std::uint8_t> explored_;

    /// Damage numbers and pickup labels rising off the world.
    struct FloatingText {
        float x = 0.0f, y = 0.0f;
        float life = 0.0f;
        std::string text;
        engine::Color color;
    };
    std::vector<FloatingText> floatingText_;

    int floorNumber_ = 1;
    std::uint32_t runSeed_ = 0;
    bool showDebug_ = false;
    float deathTimer_ = 0.0f;
    float descendTimer_ = 0.0f;
    float floorBannerTimer_ = 0.0f;

    /// Reached after clearing this many floors.
    static constexpr int kFinalFloor = 8;
};

} 