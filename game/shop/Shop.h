#pragma once

#include <array>
#include <string>

#include "../GameCommon.h"

namespace game {

enum class OfferKind {
    Potion,       // stocks a health potion in your inventory
    Sharpen,      // permanent attack damage
    Vitality,     // permanent max health
    FullHeal,     // immediate top-up
};

struct Offer {
    OfferKind kind = OfferKind::Potion;
    std::string name;
    std::string detail;
    int price = 0;
    bool soldOut = false;
};

class Shop {
public:
    void restock(int floorNumber);
    bool purchase(std::size_t slot, PlayerTag& player, engine::Health& health);

    const std::array<Offer, 4>& offers() const { return offers_; }

private:
    std::array<Offer, 4> offers_{};
};

}  