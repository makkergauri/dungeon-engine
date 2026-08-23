#include "Shop.h"

#include <algorithm>

namespace game {

void Shop::restock(int floorNumber) {
    const int depth = std::max(0, floorNumber - 1);

    offers_[0] = Offer{OfferKind::Potion, "Health Potion",
                       "Restores health when you need it", 12 + depth * 3, false};
    offers_[1] = Offer{OfferKind::Sharpen, "Sharpen Blade",
                       "+2 attack damage, permanent", 35 + depth * 12, false};
    offers_[2] = Offer{OfferKind::Vitality, "Iron Vitality",
                       "+4 max health, permanent", 30 + depth * 10, false};
    offers_[3] = Offer{OfferKind::FullHeal, "Field Surgery",
                       "Restore all health right now", 20 + depth * 6, false};
}

bool Shop::purchase(std::size_t slot, PlayerTag& player, engine::Health& health) {
    if (slot >= offers_.size()) return false;

    Offer& offer = offers_[slot];
    if (offer.soldOut || player.coins < offer.price) return false;

    switch (offer.kind) {
        case OfferKind::Potion: {
            bool stored = false;
            for (InventorySlot& inventorySlot : player.inventory) {
                if (inventorySlot.empty() || inventorySlot.type == ItemType::HealthPotion) {
                    inventorySlot.type = ItemType::HealthPotion;
                    inventorySlot.count += 1;
                    stored = true;
                    break;
                }
            }
            if (!stored) return false;
            break;
        }
        case OfferKind::Sharpen:
            player.attackDamage += 2;
            break;
        case OfferKind::Vitality:
            health.max += 4;
            health.current += 4;
            break;
        case OfferKind::FullHeal:
            if (health.current >= health.max) return false;  // nothing to buy
            health.current = health.max;
            break;
    }

    player.coins -= offer.price;
    if (offer.kind == OfferKind::Sharpen || offer.kind == OfferKind::Vitality) {
        offer.soldOut = true;
    }
    return true;
}

}  
