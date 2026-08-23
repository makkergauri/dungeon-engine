#include "DungeonGame.h"


#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include "ProceduralArt.h"
#include "ProceduralAudio.h"

namespace game {

using engine::Collider;
using engine::Entity;
using engine::Health;
using engine::Transform;

namespace {
constexpr const char* kAtlasPath = "assets/sprites/atlas.png";
constexpr const char* kFontPath = "assets/fonts/dungeon.ttf";
constexpr float kTileSize = 32.0f;

} 

// Setup

bool DungeonGame::onInit() {
    bindInput();
    loadAssets();
    wireEvents();

   
    physics_ = &systems().add<engine::PhysicsSystem>();
    playerSystem_ = &systems().add<PlayerSystem>(input(), assets_, events_);
    enemySystem_ = &systems().add<EnemyAISystem>(assets_, events_);
    combatSystem_ = &systems().add<CombatSystem>(*physics_, assets_, events_);
    systems().add<ItemSystem>();
    systems().add<engine::AnimationSystem>();
    systems().add<engine::LifetimeSystem>();

    setScreen(Screen::MainMenu);
    audio().playMusic(true);
    return true;
}

void DungeonGame::bindInput() {
    // The only function in the entire game that mentions a physical key.
    // Rebinding means editing this table and nothing else.
    input().bindKey(kMoveLeft, sf::Keyboard::A);
    input().bindKey(kMoveLeft, sf::Keyboard::Left);
    input().bindKey(kMoveRight, sf::Keyboard::D);
    input().bindKey(kMoveRight, sf::Keyboard::Right);
    input().bindKey(kMoveUp, sf::Keyboard::W);
    input().bindKey(kMoveUp, sf::Keyboard::Up);
    input().bindKey(kMoveDown, sf::Keyboard::S);
    input().bindKey(kMoveDown, sf::Keyboard::Down);
    input().bindKey(kAttack, sf::Keyboard::Space);
    input().bindKey(kInteract, sf::Keyboard::E);
    input().bindKey(kPause, sf::Keyboard::Escape);
    input().bindKey(kConfirm, sf::Keyboard::Enter);
    input().bindKey(kCancel, sf::Keyboard::Escape);
    input().bindKey(kToggleDebug, sf::Keyboard::F1);

    // Gamepad, if one is plugged in. Same actions, no gameplay code changes.
    input().bindGamepadAxis(kMoveLeft, sf::Joystick::X, false);
    input().bindGamepadAxis(kMoveRight, sf::Joystick::X, true);
    input().bindGamepadAxis(kMoveUp, sf::Joystick::Y, false);
    input().bindGamepadAxis(kMoveDown, sf::Joystick::Y, true);
    input().bindGamepadButton(kAttack, 0);
    input().bindGamepadButton(kInteract, 2);
    input().bindGamepadButton(kConfirm, 0);
    input().bindGamepadButton(kPause, 7);
}

void DungeonGame::loadAssets() {

    assets_.atlas = renderer().loadTexture(kAtlasPath);
    if (!assets_.hasAtlas()) {
        assets_.atlas = renderer().createTexture(generateAtlas());
        std::cout << "[assets] using generated placeholder art\n";
    }
    renderer().loadFont(kFontPath);

    if (assets_.hasAtlas()) {
        static engine::AnimationLibrary library;
        constexpr float kF = static_cast<float>(kFrame);

        assets_.playerIdle = library.addStrip("player_idle", 0, 0, kF, kF, 2, 0.45f);
        assets_.playerWalk = library.addStrip("player_walk", 0, kF, kF, kF, 4, 0.11f);
        assets_.playerAttack = library.addStrip("player_attack", 0, kF * 2, kF, kF, 3, 0.06f, false);
        assets_.playerDeath = library.addStrip("player_death", 0, kF * 3, kF, kF, 4, 0.14f, false);
        assets_.shamblerWalk = library.addStrip("shambler", 0, kF * 4, kF, kF, 4, 0.18f);
        assets_.sentinelIdle = library.addStrip("sentinel", 0, kF * 5, kF, kF, 2, 0.5f);
        assets_.skittererWalk = library.addStrip("skitterer", 0, kF * 6, kF, kF, 4, 0.07f);
        assets_.enemyDeath = library.addStrip("enemy_death", 0, kF * 7, kF, kF, 4, 0.12f, false);
        assets_.coinSpin = library.addStrip("coin_spin", 0, kF * 11, kF, kF, 4, 0.1f);
        assets_.torchFlicker = library.addStrip("torch", 0, kF * 13, kF, kF, 2, 0.16f);

        for (int i = 0; i < 4; ++i) {
            assets_.tileFloor[i] = {kF * i, kF * 8, kF, kF};
        }
        assets_.tileStairs = {kF * 4, kF * 8, kF, kF};
        assets_.tileWall = {0, kF * 9, kF, kF};
        assets_.tileWallFace = {kF, kF * 9, kF, kF};
        assets_.tileWallFaceLit = {kF * 2, kF * 9, kF, kF};

        assets_.itemPotion = {0, kF * 10, kF, kF};
        assets_.itemWeapon = {kF, kF * 10, kF, kF};
        assets_.itemShield = {kF * 2, kF * 10, kF, kF};
        assets_.itemArmour = {kF * 2, kF * 10, kF, kF};
        assets_.itemCoin = {kF * 3, kF * 10, kF, kF};
        assets_.projectile = {kF * 4, kF * 10, kF, kF};

        for (int i = 0; i < 5; ++i) {
            assets_.decor[i] = {kF * i, kF * 12, kF, kF};
        }
        assets_.merchant = {kF * 2, kF * 13, kF, kF};
    }

 

    if (!audio().loadSound("swing", "assets/audio/swing.wav")) {
        const GeneratedSound s = makeSwing();
        audio().createSound("swing", s.samples, s.sampleRate);
    }
    if (!audio().loadSound("hit", "assets/audio/hit.wav")) {
        const GeneratedSound s = makeHit();
        audio().createSound("hit", s.samples, s.sampleRate);
    }
    if (!audio().loadSound("hurt", "assets/audio/hurt.wav")) {
        const GeneratedSound s = makeHurt();
        audio().createSound("hurt", s.samples, s.sampleRate);
    }
    if (!audio().loadSound("death", "assets/audio/death.wav")) {
        const GeneratedSound s = makeDeath();
        audio().createSound("death", s.samples, s.sampleRate);
    }
    if (!audio().loadSound("pickup", "assets/audio/pickup.wav")) {
        const GeneratedSound s = makePickup();
        audio().createSound("pickup", s.samples, s.sampleRate);
    }
    if (!audio().loadSound("stairs", "assets/audio/stairs.wav")) {
        const GeneratedSound s = makeStairs();
        audio().createSound("stairs", s.samples, s.sampleRate);
    }
    if (!audio().loadMusic("assets/audio/ambient.ogg")) {
        const GeneratedSound s = makeAmbientLoop();
        audio().createMusic(s.samples, s.sampleRate);
        std::cout << "[assets] using generated audio\n";
    }
}

void DungeonGame::wireEvents() {

    events_.onPlayerAttack = [this](float x, float y, Facing facing) {
        audio().playSound("swing", 55.0f);
        float dirX = 0.0f;
        float dirY = 0.0f;
        facingVector(facing, dirX, dirY);

        engine::ParticleSystem::Emit emit;
        emit.x = x + dirX * 22.0f;
        emit.y = y + dirY * 22.0f;
        emit.count = 6;
        emit.speedMin = 30.0f;
        emit.speedMax = 90.0f;
        emit.lifeMin = 0.08f;
        emit.lifeMax = 0.18f;
        emit.color = engine::Color{230, 230, 240, 255};
        emit.directionDegrees = std::atan2(dirY, dirX) * 180.0f / 3.14159265f;
        emit.spreadDegrees = 70.0f;
        particles().emit(emit);
    };

    events_.onDamageDealt = [this](float x, float y, int damage) {
        audio().playSound("hit", 80.0f);
    
        addFloatingText(x, y - 14.0f, "-" + std::to_string(damage),
                        engine::Color{255, 226, 140, 255});
        // Shake scales with damage but is capped by addTrauma's clamp, so a big
        // hit is felt without the screen becoming unreadable.
        camera().addTrauma(0.14f + 0.02f * static_cast<float>(damage));

        engine::ParticleSystem::Emit emit;
        emit.x = x;
        emit.y = y;
        emit.count = 8 + damage * 2;
        emit.color = palette::kBlood;
        emit.gravity = 240.0f;
        particles().emit(emit);
    };

    events_.onEnemyKilled = [this](float x, float y, EnemyKind kind) {
        audio().playSound("death", 75.0f);
        camera().addTrauma(0.22f);


        int gold = 2;
        if (kind == EnemyKind::Sentinel) gold = 4;
        if (kind == EnemyKind::Skitterer) gold = 3;
        gold += floorNumber_ / 2;
        spawnItem(registry(), assets_, ItemType::Coin, x, y, gold);

        engine::ParticleSystem::Emit emit;
        emit.x = x;
        emit.y = y;
        emit.count = 26;
        emit.speedMin = 60.0f;
        emit.speedMax = 220.0f;
        emit.lifeMin = 0.3f;
        emit.lifeMax = 0.75f;
        emit.color = palette::kBlood;
        emit.gravity = 320.0f;
        particles().emit(emit);
    };

    events_.onPlayerHurt = [this](float x, float y) {
        audio().playSound("hurt", 90.0f);
        camera().addTrauma(0.42f);

        engine::ParticleSystem::Emit emit;
        emit.x = x;
        emit.y = y;
        emit.count = 14;
        emit.color = engine::Color{220, 80, 80, 255};
        emit.gravity = 200.0f;
        particles().emit(emit);
    };

    events_.onPickup = [this](float x, float y, ItemType type) {
        audio().playSound("pickup", 70.0f);
        addFloatingText(x, y - 12.0f, "+" + itemName(type), itemColor(type));

        engine::ParticleSystem::Emit emit;
        emit.x = x;
        emit.y = y;
        emit.count = 14;
        emit.speedMin = 20.0f;
        emit.speedMax = 90.0f;
        emit.color = itemColor(type);
        emit.gravity = -60.0f;  // pickups drift upward: reads as "gained"
        particles().emit(emit);
    };

    events_.onPlayerDeath = [this]() {
        camera().addTrauma(0.9f);
        deathTimer_ = 1.4f;
        playerSystem_->setControlEnabled(false);
    };
}


// Run and floor management

void DungeonGame::startNewRun() {
    runSeed_ = static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    floorNumber_ = 1;
    generateFloor(floorNumber_);
    setScreen(Screen::Playing);
}

void DungeonGame::generateFloor(int floorNumber) {
    registry().clear();
    particles().clear();

    GenerationParams params;
    params.width = 96;
    params.height = 64;
    params.tileSize = kTileSize;
    params.style = style_;
    params.floorNumber = floorNumber;
    params.seed = runSeed_ + static_cast<std::uint32_t>(floorNumber) * 7919u;

    dungeon_ = generator_.generate(params);

    physics_->setSolidGrid(&dungeon_);
    enemySystem_->setLevel(&dungeon_, dungeon_.buildNavGrid());

    player_ = spawnPlayer(registry(), assets_, dungeon_.worldX(dungeon_.entrance().x),
                          dungeon_.worldY(dungeon_.entrance().y), 12 + (floorNumber - 1));
    playerSystem_->setPlayer(player_);
    playerSystem_->setControlEnabled(true);
    combatSystem_->setPlayer(player_);
    enemySystem_->setPlayer(player_);

    populateFloor(floorNumber);
    const Entity stairs = registry().create();
    registry().add<Transform>(stairs, Transform{dungeon_.worldX(dungeon_.exit().x),
                                                dungeon_.worldY(dungeon_.exit().y)});
    Collider stairsCollider;
    stairsCollider.width = kTileSize * 0.9f;
    stairsCollider.height = kTileSize * 0.9f;
    stairsCollider.isTrigger = true;
    stairsCollider.isStatic = true;
    stairsCollider.layer = layers::kPickup;
    stairsCollider.mask = layers::kPlayer;
    registry().add<Collider>(stairs, stairsCollider);
    registry().add<StairsTrigger>(stairs, StairsTrigger{});

    // Snap the camera rather than letting it glide in from the last floor.
    camera().setBounds(0.0f, 0.0f, dungeon_.widthInTiles() * kTileSize,
                       dungeon_.heightInTiles() * kTileSize);
    camera().setPosition(dungeon_.worldX(dungeon_.entrance().x),
                         dungeon_.worldY(dungeon_.entrance().y));

    explored_.assign(static_cast<std::size_t>(dungeon_.widthInTiles()) *
                         dungeon_.heightInTiles(), 0);
    floatingText_.clear();

    floorBannerTimer_ = 2.2f;
    descendTimer_ = 0.0f;
    deathTimer_ = 0.0f;
    time().discardLostTime();
}

void DungeonGame::populateFloor(int floorNumber) {
    std::vector<GridPoint> candidates = generator_.spawnCandidates(dungeon_, 8);
    if (candidates.empty()) return;

    std::mt19937 rng(runSeed_ + static_cast<std::uint32_t>(floorNumber) * 104729u);
    std::shuffle(candidates.begin(), candidates.end(), rng);

    const int enemyCount = std::min(static_cast<int>(candidates.size()),
                                    6 + floorNumber * 3);
    std::size_t cursor = 0;

    for (int i = 0; i < enemyCount && cursor < candidates.size(); ++i, ++cursor) {
        std::uniform_int_distribution<int> roll(0, 99);
        const int value = roll(rng);
        EnemyKind kind = EnemyKind::Shambler;
        if (value < std::min(35, 8 + floorNumber * 5)) {
            kind = EnemyKind::Skitterer;
        } else if (value < std::min(60, 20 + floorNumber * 6)) {
            kind = EnemyKind::Sentinel;
        }

        const GridPoint p = candidates[cursor];
        spawnEnemy(registry(), assets_, kind, dungeon_.worldX(p.x), dungeon_.worldY(p.y),
                   floorNumber);
    }

    const int potionCount = 2 + floorNumber / 2;
    for (int i = 0; i < potionCount && cursor < candidates.size(); ++i, ++cursor) {
        const GridPoint p = candidates[cursor];
        spawnItem(registry(), assets_, ItemType::HealthPotion, dungeon_.worldX(p.x),
                  dungeon_.worldY(p.y));
    }

    std::uniform_int_distribution<int> upgradeRoll(0, 2);
    if (cursor < candidates.size()) {
        const GridPoint p = candidates[cursor++];
        const ItemType type = upgradeRoll(rng) == 0 ? ItemType::ArmourScrap
                                                    : ItemType::WeaponUpgrade;
        spawnItem(registry(), assets_, type, dungeon_.worldX(p.x), dungeon_.worldY(p.y));
    }

    const int coinCount = 4 + floorNumber;
    for (int i = 0; i < coinCount && cursor < candidates.size(); ++i, ++cursor) {
        const GridPoint p = candidates[cursor];
        spawnItem(registry(), assets_, ItemType::Coin, dungeon_.worldX(p.x),
                  dungeon_.worldY(p.y), 1 + floorNumber / 2);
    }

    // --- Merchant ---
    
    merchant_ = engine::kNullEntity;
    if (candidates.size() > 12) {
        const GridPoint p = candidates[candidates.size() * 2 / 3];
        merchant_ = registry().create();
        registry().add<Transform>(merchant_, Transform{dungeon_.worldX(p.x),
                                                       dungeon_.worldY(p.y)});
        registry().add<MerchantTag>(merchant_, MerchantTag{});

        engine::SpriteComponent sprite;
        sprite.texture = assets_.atlas;
        sprite.source = assets_.hasAtlas() ? assets_.merchant : engine::TextureRect{};
        sprite.width = 34.0f;
        sprite.height = 34.0f;
        sprite.layer = engine::kLayerActor;
        sprite.tint = assets_.hasAtlas() ? engine::colors::kWhite : palette::kMerchant;
        sprite.offsetY = -6.0f;
        registry().add<engine::SpriteComponent>(merchant_, sprite);
    }
    shop_.restock(floorNumber);

    scatterDecor(floorNumber);
}

void DungeonGame::scatterDecor(int floorNumber) {
    if (!assets_.hasAtlas()) return;  // nothing sensible to draw without sprites

    std::mt19937 rng(runSeed_ + static_cast<std::uint32_t>(floorNumber) * 31337u);
    std::uniform_int_distribution<int> pick(0, 4);

    
    for (int y = 1; y < dungeon_.heightInTiles() - 1; ++y) {
        for (int x = 1; x < dungeon_.widthInTiles() - 1; ++x) {
            if (dungeon_.at(x, y) != Tile::Floor) continue;

            const std::uint32_t h = static_cast<std::uint32_t>((x * 92837111) ^ (y * 689287499));
            if ((h % 23u) != 0) continue;  // roughly 4% of floor tiles

            // Keep the entrance clear so the player never spawns inside scenery.
            const int dx = x - dungeon_.entrance().x;
            const int dy = y - dungeon_.entrance().y;
            if (dx * dx + dy * dy < 25) continue;

            const int kind = pick(rng);
            const engine::Entity e = registry().create();
            registry().add<Transform>(e, Transform{dungeon_.worldX(x), dungeon_.worldY(y)});
            registry().add<DecorTag>(e, DecorTag{});

            engine::SpriteComponent sprite;
            sprite.texture = assets_.atlas;
            sprite.source = assets_.decor[kind];
            sprite.width = 26.0f;
            sprite.height = 26.0f;
            sprite.layer = engine::kLayerDecal;
            sprite.offsetY = -3.0f;
            registry().add<engine::SpriteComponent>(e, sprite);
        }
    }
}

void DungeonGame::confirmMenuChoice() {
    switch (menuSelection_) {
        case 0:
            style_ = GenerationStyle::BSP;
            startNewRun();
            break;
        case 1:
            style_ = GenerationStyle::CellularAutomata;
            startNewRun();
            break;
        case 2:
            legendReturn_ = Screen::MainMenu;
            setScreen(Screen::Legend);
            break;
        default:
            requestQuit();
            break;
    }
}

void DungeonGame::openShop() {
    audio().playSound("pickup", 60.0f);
    setScreen(Screen::Shop);
}

void DungeonGame::tryPurchase(std::size_t slot) {
    if (!registry().alive(player_)) return;
    PlayerTag* tag = registry().tryGet<PlayerTag>(player_);
    Health* health = registry().tryGet<Health>(player_);
    if (!tag || !health) return;

    if (shop_.purchase(slot, *tag, *health)) {
        audio().playSound("pickup", 90.0f);
        updateHudStats();
    } else {
        // Distinct, quieter sound for a refused sale. Silence would leave the
        // player unsure whether the key even registered.
        audio().playSound("swing", 35.0f);
    }
}

void DungeonGame::descend() {
    audio().playSound("stairs", 85.0f);
    PlayerTag carriedTag;
    Health carriedHealth{12, 12, 0.0f};
    if (registry().alive(player_)) {
        if (PlayerTag* tag = registry().tryGet<PlayerTag>(player_)) carriedTag = *tag;
        if (Health* health = registry().tryGet<Health>(player_)) carriedHealth = *health;
    }

    ++floorNumber_;
    if (floorNumber_ > kFinalFloor) {
        setScreen(Screen::Victory);
        return;
    }

    generateFloor(floorNumber_);

    if (registry().alive(player_)) {
        if (PlayerTag* tag = registry().tryGet<PlayerTag>(player_)) {
            const PlayerTag fresh = *tag;
            *tag = carriedTag;
            tag->attackCooldown = 0.0f;
            tag->attackTimer = 0.0f;
            tag->facing = fresh.facing;
        }
        if (Health* health = registry().tryGet<Health>(player_)) {
            health->max = std::max(health->max, carriedHealth.max);
            // A small heal on descending. Arriving on a harder floor at one hit
            // point is a death sentence the player had no way to avoid.
            health->current = std::min(health->max, carriedHealth.current + 3);
            health->invulnerableFor = 1.0f;
        }
    }
}


// Frame

void DungeonGame::onFixedUpdate(float dt) {
    if (screen_ != Screen::Playing) return;

    if (deathTimer_ > 0.0f) {
        deathTimer_ -= dt;
        if (deathTimer_ <= 0.0f) setScreen(Screen::GameOver);
        return;
    }

    if (descendTimer_ > 0.0f) {
        descendTimer_ -= dt;
        if (descendTimer_ <= 0.0f) descend();
        return;
    }

    const bool onStairs = combatSystem_->consumeStairsRequest();
    hudStats_.onStairs = onStairs;

    nearMerchant_ = false;
    if (merchant_ != engine::kNullEntity && registry().alive(merchant_) &&
        registry().alive(player_)) {
        Transform* m = registry().tryGet<Transform>(merchant_);
        Transform* p = registry().tryGet<Transform>(player_);
        if (m && p) {
            const float dx = m->x - p->x;
            const float dy = m->y - p->y;
            nearMerchant_ = (dx * dx + dy * dy) < (52.0f * 52.0f);
        }
    }
    hudStats_.nearMerchant = nearMerchant_ && !onStairs;

    if (onStairs && input().isDown(kInteract)) {
        descendTimer_ = 0.35f;
        playerSystem_->setControlEnabled(false);
    } else if (nearMerchant_ && input().wasPressed(kInteract)) {
        openShop();
    }
}

void DungeonGame::onUpdate(float dt) {
    floorBannerTimer_ = std::max(0.0f, floorBannerTimer_ - dt);

    if (screen_ == Screen::Playing && registry().alive(player_)) {
        if (Transform* transform = registry().tryGet<Transform>(player_)) {
            camera().follow(transform->x, transform->y, dt);
        }
    }

    // Idle bob on the merchant, so he reads as a character rather than a statue.
    if (merchant_ != engine::kNullEntity && registry().alive(merchant_)) {
        if (engine::SpriteComponent* sprite =
                registry().tryGet<engine::SpriteComponent>(merchant_)) {
            sprite->offsetY = -6.0f + std::sin(static_cast<float>(time().totalElapsed()) * 2.4f) * 2.0f;
            // Highlight when in range: the clearest possible "you can interact".
            sprite->tint = nearMerchant_ ? engine::Color{255, 255, 255, 255}
                                         : engine::Color{215, 210, 230, 255};
        }
    }

    if (screen_ == Screen::Playing) {
        updateExplored();

        // Age out the damage numbers. Render-rate rather than fixed-step,
        // because they are pure presentation.
        for (FloatingText& item : floatingText_) item.life -= dt;
        floatingText_.erase(
            std::remove_if(floatingText_.begin(), floatingText_.end(),
                           [](const FloatingText& item) { return item.life <= 0.0f; }),
            floatingText_.end());
    }

    if (screen_ == Screen::Playing || screen_ == Screen::Shop) updateHudStats();
}

void DungeonGame::onEvent(const sf::Event& event) {
    if (event.type != sf::Event::KeyPressed) return;

    if (input().wasPressed(kToggleDebug)) showDebug_ = !showDebug_;

    switch (screen_) {
        case Screen::MainMenu: {
            const int itemCount = static_cast<int>(Hud::mainMenuItems().size());
            if (event.key.code == sf::Keyboard::Up || event.key.code == sf::Keyboard::W) {
                // Wrap with a modulo rather than clamping: a menu that stops
                // dead at the top feels broken when you are holding a key.
                menuSelection_ = (menuSelection_ + itemCount - 1) % itemCount;
            } else if (event.key.code == sf::Keyboard::Down ||
                       event.key.code == sf::Keyboard::S) {
                menuSelection_ = (menuSelection_ + 1) % itemCount;
            } else if (event.key.code == sf::Keyboard::Enter ||
                       event.key.code == sf::Keyboard::Space) {
                confirmMenuChoice();
            } else if (event.key.code == sf::Keyboard::H) {
                legendReturn_ = Screen::MainMenu;
                setScreen(Screen::Legend);
            } else if (event.key.code == sf::Keyboard::Escape) {
                requestQuit();
            }
            break;
        }

        case Screen::Playing:
            if (event.key.code == sf::Keyboard::Escape) {
                setScreen(Screen::Paused);
            } else if (event.key.code == sf::Keyboard::H) {
                legendReturn_ = Screen::Playing;
                setScreen(Screen::Legend);
            }
            break;

        case Screen::Legend:
            if (event.key.code == sf::Keyboard::H || event.key.code == sf::Keyboard::Escape) {
                setScreen(legendReturn_);
            }
            break;

        case Screen::Paused:
            if (event.key.code == sf::Keyboard::Escape) {
                setScreen(Screen::Playing);
            } else if (event.key.code == sf::Keyboard::H) {
                legendReturn_ = Screen::Paused;
                setScreen(Screen::Legend);
            } else if (event.key.code == sf::Keyboard::Q) {
                setScreen(Screen::MainMenu);
            }
            break;

        case Screen::Shop:
            if (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::E) {
                setScreen(Screen::Playing);
            } else if (event.key.code >= sf::Keyboard::Num1 &&
                       event.key.code <= sf::Keyboard::Num4) {
                tryPurchase(static_cast<std::size_t>(event.key.code - sf::Keyboard::Num1));
            }
            break;

        case Screen::GameOver:
        case Screen::Victory:
            if (event.key.code == sf::Keyboard::Enter) {
                startNewRun();
            } else if (event.key.code == sf::Keyboard::Escape) {
                setScreen(Screen::MainMenu);
            }
            break;
    }
}

void DungeonGame::setScreen(Screen screen) {
    screen_ = screen;

    // Freeze simulation outside gameplay by switching the systems off rather
    // than by scattering `if (paused) return` through every one of them.
    const bool playing = screen == Screen::Playing;
    // Shop and help freeze the world exactly like pause does -- being hit while
    // reading an inventory screen is the kind of thing players never forgive.
    if (playerSystem_) playerSystem_->setControlEnabled(playing && deathTimer_ <= 0.0f);
    if (enemySystem_) enemySystem_->setEnabled(playing);
    if (physics_) physics_->enabled = playing;
    if (combatSystem_) combatSystem_->enabled = playing;

    audio().pauseMusic(screen == Screen::Paused);

    // Clear held inputs across a screen change, or the key that unpaused the
    // game also swings the sword on the first frame back.
    input().reset();

    if (playing) time().discardLostTime();
}

// Rendering

void DungeonGame::addFloatingText(float x, float y, const std::string& text,
                                  engine::Color color) {
    // Hard cap. A crowded fight can raise dozens of these per second and they
    // stop being readable long before they become a performance problem.
    if (floatingText_.size() > 24) floatingText_.erase(floatingText_.begin());
    floatingText_.push_back(FloatingText{x, y, 0.9f, text, color});
}

void DungeonGame::updateExplored() {
    if (!registry().alive(player_) || explored_.empty()) return;
    Transform* transform = registry().tryGet<Transform>(player_);
    if (!transform) return;

    const int cx = dungeon_.tileXAt(transform->x);
    const int cy = dungeon_.tileYAt(transform->y);
    constexpr int kSightRadius = 9;

    // Square reveal rather than a circle: the difference is invisible at two
    // pixels per tile on the minimap, and this is one comparison per cell.
    for (int y = cy - kSightRadius; y <= cy + kSightRadius; ++y) {
        for (int x = cx - kSightRadius; x <= cx + kSightRadius; ++x) {
            if (!dungeon_.inBounds(x, y)) continue;
            explored_[static_cast<std::size_t>(y) * dungeon_.widthInTiles() + x] = 1;
        }
    }
}

void DungeonGame::renderWorldOverlays() {
    registry().each<EnemyTag, Health, Transform>(
        [&](Entity, EnemyTag&, Health& health, Transform& transform) {
            if (!health.alive() || health.current >= health.max) return;

            const float width = 26.0f;
            const float y = transform.y - 22.0f;
            const float fraction =
                std::clamp(static_cast<float>(health.current) / health.max, 0.0f, 1.0f);

            renderer().drawQuad(transform.x, y, width + 2.0f, 6.0f,
                                engine::Color{16, 14, 20, 220}, engine::kLayerParticle);
            renderer().drawQuad(transform.x - width * 0.5f + width * fraction * 0.5f, y,
                                width * fraction, 4.0f,
                                engine::Color{206, 72, 72, 255}, engine::kLayerParticle);
        });


    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(time().totalElapsed()) * 3.0f);
    renderer().drawQuad(dungeon_.worldX(dungeon_.exit().x),
                        dungeon_.worldY(dungeon_.exit().y) - 26.0f, 8.0f + pulse * 4.0f,
                        8.0f + pulse * 4.0f,
                        engine::Color{120, 220, 150,
                                      static_cast<std::uint8_t>(140 + pulse * 100)},
                        engine::kLayerParticle);
    renderer().flush();

    for (const FloatingText& item : floatingText_) {
        engine::Color color = item.color;
        color.a = static_cast<std::uint8_t>(std::clamp(item.life * 280.0f, 0.0f, 255.0f));
        // Rise as they fade, which reads as the number leaving the world.
        renderer().drawText(item.text, item.x, item.y - (0.9f - item.life) * 26.0f, 15, color,
                            true);
    }
}

void DungeonGame::renderTiles() {
    const float halfW = camera().viewportWidth() * 0.5f;
    const float halfH = camera().viewportHeight() * 0.5f;

    const int minX = std::max(0, dungeon_.tileXAt(camera().x() - halfW) - 1);
    const int maxX = std::min(dungeon_.widthInTiles() - 1,
                              dungeon_.tileXAt(camera().x() + halfW) + 1);
    const int minY = std::max(0, dungeon_.tileYAt(camera().y() - halfH) - 1);
    const int maxY = std::min(dungeon_.heightInTiles() - 1,
                              dungeon_.tileYAt(camera().y() + halfH) + 2);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const Tile tile = dungeon_.at(x, y);

            if (tile == Tile::Wall) {
                // Interior walls with no exposed face are never visible. On a
                // cave map that is most of the grid.
                bool exposed = false;
                for (int dy = -1; dy <= 1 && !exposed; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dungeon_.isFloor(x + dx, y + dy)) { exposed = true; break; }
                    }
                }
                if (!exposed) continue;
                const bool facing = dungeon_.isFloor(x, y + 1);
                engine::TextureRect source = assets_.tileWall;
                engine::Color tint = palette::kWall;

                if (facing) {
                    // Alternate lit and unlit faces from a coordinate hash so a
                    // long wall does not read as one flat band.
                    const bool lit = ((x * 7 + y * 13) & 3) != 0;
                    source = lit ? assets_.tileWallFaceLit : assets_.tileWallFace;
                    tint = engine::Color{225, 222, 235, 255};
                }
                if (!assets_.hasAtlas()) source = engine::TextureRect{};

                renderer().drawQuad(dungeon_.worldX(x), dungeon_.worldY(y), kTileSize,
                                    kTileSize, assets_.hasAtlas() ? engine::colors::kWhite : tint,
                                    facing ? engine::kLayerDecal : engine::kLayerFloor,
                                    assets_.atlas, source);
                continue;
            }

            // --- Floor and stairs ---
            engine::TextureRect source;
            engine::Color tint = engine::colors::kWhite;

            if (tile == Tile::StairsDown) {
                source = assets_.tileStairs;
                if (!assets_.hasAtlas()) tint = palette::kStairs;
            } else {
                // Hash the coordinates to pick one of four variants. Deterministic,
                // so the same tile always looks the same across frames.
                const int variant = static_cast<int>(
                    ((x * 73856093) ^ (y * 19349663)) & 3);
                source = assets_.tileFloor[variant];
                if (!assets_.hasAtlas()) {
                    tint = palette::kFloor;
                    const int noise = ((x * 73856093) ^ (y * 19349663)) & 7;
                    tint.r = static_cast<std::uint8_t>(tint.r + noise);
                    tint.g = static_cast<std::uint8_t>(tint.g + noise);
                    tint.b = static_cast<std::uint8_t>(tint.b + noise);
                }
            }
            if (!assets_.hasAtlas()) source = engine::TextureRect{};

            renderer().drawQuad(dungeon_.worldX(x), dungeon_.worldY(y), kTileSize, kTileSize,
                                tint, engine::kLayerFloor, assets_.atlas, source);

            // Contact shadow under a wall above. Cheap, and it stops floor tiles
            // and wall bases from blending into one another.
            if (tile != Tile::Wall && dungeon_.isSolid(x, y - 1)) {
                renderer().drawQuad(dungeon_.worldX(x), dungeon_.worldY(y) - kTileSize * 0.36f,
                                    kTileSize, kTileSize * 0.28f,
                                    engine::Color{0, 0, 0, 70}, engine::kLayerFloor);
            }
        }
    }
}

void DungeonGame::renderWorld() {
    renderer().beginWorld(camera());
    renderTiles();
    renderer().submitSprites(registry());
    particles().submit(renderer());
    renderWorldOverlays();
    renderer().flush();
}

void DungeonGame::onRender() {
    const float screenWidth = static_cast<float>(window().width());
    const float screenHeight = static_cast<float>(window().height());
    const float elapsed = static_cast<float>(time().totalElapsed());

    // The world keeps rendering behind pause, the shop and the help screen, so
    // those read as overlays on a running game rather than as separate places.
    if (screen_ != Screen::MainMenu) renderWorld();

    renderer().beginUI();

    switch (screen_) {
        case Screen::MainMenu:
            hud_.drawMainMenu(renderer(), assets_, menuSelection_, elapsed, screenWidth,
                              screenHeight);
            break;

        case Screen::Playing:
        case Screen::Paused:
        case Screen::Shop: {
            hud_.drawGameplay(renderer(), hudStats_, screenWidth, screenHeight);

            // Minimap blips, cheapest possible answer to "where is everything".
            std::vector<MapBlip> blips;
            registry().each<EnemyTag, Transform>([&](Entity, EnemyTag& enemy, Transform& t) {
                // Only show enemies on explored ground, or the map would give
                // away the whole floor before the player has walked it.
                const int tx = dungeon_.tileXAt(t.x);
                const int ty = dungeon_.tileYAt(t.y);
                if (!dungeon_.inBounds(tx, ty)) return;
                const std::size_t index =
                    static_cast<std::size_t>(ty) * dungeon_.widthInTiles() + tx;
                if (index >= explored_.size() || !explored_[index]) return;

                engine::Color color = palette::kShambler;
                if (enemy.kind == EnemyKind::Sentinel) color = palette::kSentinel;
                if (enemy.kind == EnemyKind::Skitterer) color = palette::kSkitterer;
                blips.push_back(MapBlip{t.x, t.y, color, false});
            });
            if (merchant_ != engine::kNullEntity && registry().alive(merchant_)) {
                if (Transform* t = registry().tryGet<Transform>(merchant_)) {
                    blips.push_back(MapBlip{t->x, t->y, palette::kMerchant, true});
                }
            }
            if (registry().alive(player_)) {
                if (Transform* t = registry().tryGet<Transform>(player_)) {
                    // Player last, so the blip draws over anything sharing a tile.
                    blips.push_back(MapBlip{t->x, t->y,
                                            engine::Color{255, 255, 255, 255}, true});
                }
            }
            hud_.drawMinimap(renderer(), dungeon_, explored_, blips, screenWidth);

            if (screen_ == Screen::Playing && floorBannerTimer_ > 0.0f) {
                engine::Color color = palette::kUiText;
                color.a = static_cast<std::uint8_t>(
                    std::clamp(floorBannerTimer_ * 255.0f, 0.0f, 255.0f));
                renderer().drawText("Floor " + std::to_string(floorNumber_),
                                    screenWidth * 0.5f, screenHeight * 0.20f, 40, color, true);
            }

            if (screen_ == Screen::Paused) {
                hud_.drawMenu(renderer(), "PAUSED",
                              {"Esc  -  resume", "H  -  how to play", "Q  -  abandon run"},
                              screenWidth, screenHeight);
            } else if (screen_ == Screen::Shop) {
                hud_.drawShop(renderer(), shop_.offers(), hudStats_.coins, screenWidth,
                              screenHeight);
            }
            break;
        }

        case Screen::Legend:
            hud_.drawLegend(renderer(), assets_, screenWidth, screenHeight);
            break;

        case Screen::GameOver:
            hud_.drawMenu(renderer(), "YOU DIED",
                          {"You fell on floor " + std::to_string(floorNumber_),
                           "Gold collected: " + std::to_string(hudStats_.coins), "",
                           "Enter  -  try again", "Esc  -  main menu"},
                          screenWidth, screenHeight, engine::Color{200, 70, 70, 255});
            break;

        case Screen::Victory:
            hud_.drawMenu(renderer(), "ESCAPED",
                          {"You cleared all " + std::to_string(kFinalFloor) + " floors",
                           "Gold collected: " + std::to_string(hudStats_.coins), "",
                           "Enter  -  play again", "Esc  -  main menu"},
                          screenWidth, screenHeight, palette::kStairs);
            break;
    }

    if (showDebug_) {
        hud_.drawDebug(renderer(), time().fps(), static_cast<int>(registry().entityCount()),
                       renderer().lastDrawCallCount(), renderer().lastQuadCount(),
                       enemySystem_ ? enemySystem_->lastFrameSearches() : 0,
                       particles().liveCount(), screenWidth);
    }
}

void DungeonGame::updateHudStats() {
    hudStats_.floorNumber = floorNumber_;
    hudStats_.enemiesRemaining = enemiesAlive();

    if (!registry().alive(player_)) return;
    if (Health* health = registry().tryGet<Health>(player_)) {
        hudStats_.health = health->current;
        hudStats_.maxHealth = health->max;
    }
    if (PlayerTag* tag = registry().tryGet<PlayerTag>(player_)) {
        hudStats_.coins = tag->coins;
        hudStats_.attackDamage = tag->attackDamage;
        hudStats_.inventory = tag->inventory;
    }
}

int DungeonGame::enemiesAlive() {
    int count = 0;
    registry().each<EnemyTag>([&](Entity, EnemyTag&) { ++count; });
    return count;
}

bool DungeonGame::playerAlive() {
    if (!registry().alive(player_)) return false;
    Health* health = registry().tryGet<Health>(player_);
    return health && health->alive();
}

void DungeonGame::onShutdown() { audio().stopMusic(); }

} 