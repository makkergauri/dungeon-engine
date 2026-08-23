#include "GameCommon.h"

#include <cmath>

namespace game {

void facingVector(Facing facing, float& outX, float& outY) {
    switch (facing) {
        case Facing::Up:    outX = 0.0f;  outY = -1.0f; break;
        case Facing::Down:  outX = 0.0f;  outY = 1.0f;  break;
        case Facing::Left:  outX = -1.0f; outY = 0.0f;  break;
        case Facing::Right: outX = 1.0f;  outY = 0.0f;  break;
    }
}

Facing facingFromVector(float x, float y, Facing fallback) {
    if (std::fabs(x) < 0.01f && std::fabs(y) < 0.01f) return fallback;
    if (std::fabs(x) >= std::fabs(y)) {
        return x > 0.0f ? Facing::Right : Facing::Left;
    }
    return y > 0.0f ? Facing::Down : Facing::Up;
}

std::string itemName(ItemType type) {
    switch (type) {
        case ItemType::HealthPotion:  return "Health Potion";
        case ItemType::WeaponUpgrade: return "Whetstone";
        case ItemType::ArmourScrap:   return "Armour Scrap";
        case ItemType::Coin:          return "Coin";
    }
    return "Unknown";
}

engine::Color itemColor(ItemType type) {
    switch (type) {
        case ItemType::HealthPotion:  return engine::Color{220, 70, 90, 255};
        case ItemType::WeaponUpgrade: return engine::Color{200, 200, 215, 255};
        case ItemType::ArmourScrap:   return engine::Color{120, 160, 200, 255};
        case ItemType::Coin:          return engine::Color{230, 190, 90, 255};
    }
    return engine::colors::kWhite;
}

}  