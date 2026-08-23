#include "ProceduralArt.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game {
namespace {


struct Canvas {
    sf::Image* image = nullptr;
    int originX = 0;
    int originY = 0;

    void px(int x, int y, sf::Color color) {
        if (x < 0 || y < 0 || x >= kFrame || y >= kFrame) return;
        if (color.a == 0) return;
        image->setPixel(static_cast<unsigned>(originX + x),
                        static_cast<unsigned>(originY + y), color);
    }

    void rect(int x, int y, int w, int h, sf::Color color) {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) px(x + i, y + j, color);
        }
    }

    void hLine(int x, int y, int w, sf::Color color) { rect(x, y, w, 1, color); }
    void vLine(int x, int y, int h, sf::Color color) { rect(x, y, 1, h, color); }

    void disc(float cx, float cy, float radius, sf::Color color) {
        const int minX = static_cast<int>(std::floor(cx - radius));
        const int maxX = static_cast<int>(std::ceil(cx + radius));
        const int minY = static_cast<int>(std::floor(cy - radius));
        const int maxY = static_cast<int>(std::ceil(cy + radius));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = x + 0.5f - cx;
                const float dy = y + 0.5f - cy;
                if (dx * dx + dy * dy <= radius * radius) px(x, y, color);
            }
        }
    }


    void outline(sf::Color color) {
        sf::Color before[kFrame][kFrame];
        for (int y = 0; y < kFrame; ++y) {
            for (int x = 0; x < kFrame; ++x) {
                before[y][x] = image->getPixel(static_cast<unsigned>(originX + x),
                                               static_cast<unsigned>(originY + y));
            }
        }
        for (int y = 0; y < kFrame; ++y) {
            for (int x = 0; x < kFrame; ++x) {
                if (before[y][x].a != 0) continue;
                const bool touching =
                    (x > 0 && before[y][x - 1].a != 0) ||
                    (x < kFrame - 1 && before[y][x + 1].a != 0) ||
                    (y > 0 && before[y - 1][x].a != 0) ||
                    (y < kFrame - 1 && before[y + 1][x].a != 0);
                if (touching) px(x, y, color);
            }
        }
    }
};

/// Deterministic per-pixel hash, used for floor speckle and decor scatter.
/// Same input always gives the same texture, so the atlas is reproducible.
std::uint32_t hash2(int x, int y, std::uint32_t salt) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u + salt * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

sf::Color shade(sf::Color base, float factor) {
    auto channel = [factor](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::clamp(v * factor, 0.0f, 255.0f));
    };
    return sf::Color(channel(base.r), channel(base.g), channel(base.b), base.a);
}

const sf::Color kOutline(16, 14, 22, 255);
const sf::Color kClear(0, 0, 0, 0);


// Characters

struct BodyPalette {
    sf::Color primary;    // tunic / shell
    sf::Color secondary;  // trousers / underside
    sf::Color skin;
    sf::Color accent;     // belt, trim, eyes
};

enum class Pose { Idle, IdleBob, Step1, Step2, Attack, AttackFollow, Dead };


void drawHumanoid(Canvas& c, const BodyPalette& palette, Pose pose) {
    const int bob = (pose == Pose::IdleBob || pose == Pose::Step2) ? 1 : 0;
    const bool dead = pose == Pose::Dead;
    const bool attacking = pose == Pose::Attack || pose == Pose::AttackFollow;

    if (dead) {
        c.rect(3, 11, 10, 3, palette.primary);
        c.rect(4, 10, 8, 1, shade(palette.primary, 1.15f));
        c.rect(2, 13, 12, 1, shade(palette.secondary, 0.8f));
        c.disc(5.0f, 11.0f, 1.6f, palette.skin);
        c.outline(kOutline);
        return;
    }

    const int top = 2 + bob;

    // Head
    c.rect(5, top, 6, 5, palette.skin);
    c.rect(5, top, 6, 1, shade(palette.skin, 1.12f));  // forehead highlight
    // Eyes, kept two pixels apart -- one pixel reads as a smudge, three as a mask.
    c.px(6, top + 2, palette.accent);
    c.px(9, top + 2, palette.accent);

    // Torso
    c.rect(4, top + 5, 8, 5, palette.primary);
    c.rect(4, top + 5, 8, 1, shade(palette.primary, 1.2f));
    c.hLine(4, top + 8, 8, palette.accent);  // belt

    // Arms
    const int armY = top + 6;
    if (attacking) {
        c.rect(2, armY, 2, 3, palette.skin);
        c.rect(11, armY, 2, 3, palette.skin);
        if (pose == Pose::Attack) {
            c.rect(12, armY - 6, 2, 7, sf::Color(228, 234, 246));
            c.vLine(12, armY - 6, 7, sf::Color(255, 255, 255));
            c.rect(11, armY, 4, 1, sf::Color(150, 110, 60));  // crossguard
        } else {
            c.rect(12, armY, 4, 2, sf::Color(228, 234, 246));
            c.hLine(12, armY, 4, sf::Color(255, 255, 255));
            c.rect(11, armY - 1, 1, 4, sf::Color(150, 110, 60));
        }
    } else {
        const int swing = (pose == Pose::Step1) ? 1 : (pose == Pose::Step2 ? -1 : 0);
        c.rect(2, armY + swing, 2, 4, palette.skin);
        c.rect(12, armY - swing, 2, 4, palette.skin);
    }

    // Legs
    const int legY = top + 10;
    if (pose == Pose::Step1) {
        c.rect(4, legY, 3, 3, palette.secondary);
        c.rect(9, legY, 3, 2, palette.secondary);
    } else if (pose == Pose::Step2) {
        c.rect(4, legY, 3, 2, palette.secondary);
        c.rect(9, legY, 3, 3, palette.secondary);
    } else {
        c.rect(4, legY, 3, 3, palette.secondary);
        c.rect(9, legY, 3, 3, palette.secondary);
    }

    c.outline(kOutline);
}

/// Shambler: a hunched blob with heavy brows. Reads as slow and solid.
void drawShambler(Canvas& c, int frame) {
    const sf::Color body(168, 88, 92);
    const int bob = (frame == 1 || frame == 3) ? 1 : 0;
    const int lean = (frame == 1) ? -1 : (frame == 3 ? 1 : 0);

    c.disc(8.0f, 8.0f + bob, 5.2f, body);
    c.disc(8.0f, 6.5f + bob, 4.4f, shade(body, 1.18f));  // lit dome
    // Brow ridge, which is what makes it look angry rather than like a ball.
    c.rect(4, 6 + bob, 8, 1, shade(body, 0.55f));
    c.px(6 + lean, 8 + bob, sf::Color(255, 220, 120));
    c.px(9 + lean, 8 + bob, sf::Color(255, 220, 120));
    // Stubby legs alternate so it visibly trudges.
    c.rect(4, 12 + bob, 3, 2, shade(body, 0.7f));
    c.rect(9, 12 + bob, 3, 2, shade(body, 0.7f));
    c.outline(kOutline);
}


void drawSentinel(Canvas& c, int frame) {
    const sf::Color stone(196, 150, 66);
    c.rect(3, 9, 10, 5, stone);
    c.rect(3, 9, 10, 1, shade(stone, 1.2f));
    c.rect(2, 13, 12, 1, shade(stone, 0.65f));
    c.rect(5, 4, 6, 6, shade(stone, 0.9f));

    // Pulsing eye, the only animated part.
    const float radius = (frame == 0) ? 2.0f : 2.6f;
    c.disc(8.0f, 7.0f, radius, sf::Color(255, 160, 60));
    c.disc(8.0f, 7.0f, radius * 0.5f, sf::Color(255, 240, 190));
    c.outline(kOutline);
}

/// Skitterer: spindly legs and a small body. Legs cycle hard so it looks frantic.
void drawSkitterer(Canvas& c, int frame) {
    const sf::Color body(186, 92, 190);
    c.disc(8.0f, 7.5f, 3.4f, body);
    c.disc(8.0f, 6.5f, 2.6f, shade(body, 1.25f));
    c.px(7, 7, sf::Color(255, 250, 210));
    c.px(9, 7, sf::Color(255, 250, 210));

    // Three legs a side, each a two-segment kink: out from the body, then down.
    // Drawing them as one connected run per leg is what stops them reading as
    // loose speckle around the body.
    const int phase = frame % 2;
    const sf::Color leg = shade(body, 0.55f);
    for (int side = 0; side < 2; ++side) {
        const int dir = side == 0 ? -1 : 1;
        for (int i = 0; i < 3; ++i) {
            const int hipY = 7 + i;
            const int out = ((i + phase) % 2 == 0) ? 3 : 2;  // alternating stride
            for (int step = 1; step <= out; ++step) {
                c.px(8 + dir * (2 + step), hipY, leg);
            }
            c.px(8 + dir * (2 + out), hipY + 1, leg);   // foot
            c.px(8 + dir * (2 + out), hipY + 2, leg);
        }
    }
    c.outline(kOutline);
}

// -----------------------------------------------------------------------------
// Tiles
// -----------------------------------------------------------------------------

/// Floor variants. Four of them, picked per tile by a coordinate hash, because a
/// single repeated tile is the fastest way to make a level look like graph paper.
void drawFloor(Canvas& c, int variant) {
    // Range kept very tight. At +/-6% the variants read as a checkerboard --
    // adjacent tiles differ enough that the grid becomes the dominant pattern in
    // the room. 2% is felt as unevenness without ever resolving into squares.
    const float tone = 0.985f + 0.01f * static_cast<float>(variant % 2);
    const sf::Color base = shade(sf::Color(84, 78, 98), tone);
    c.rect(0, 0, kFrame, kFrame, base);

    // Speckle. Low contrast on purpose: enough to break the flatness, not enough
    // to compete with entities standing on it.
    for (int y = 0; y < kFrame; ++y) {
        for (int x = 0; x < kFrame; ++x) {
            const std::uint32_t h = hash2(x, y, static_cast<std::uint32_t>(variant) + 1u);
            if ((h & 7u) == 0) c.px(x, y, shade(base, 1.16f));
            else if ((h & 15u) == 1) c.px(x, y, shade(base, 0.84f));
        }
    }

    // Variant features are deliberately LOW contrast and small.
    //
    // The first pass made them bold, and the result was worse than a plain tile:
    // at 25% frequency a strong mark repeats across the whole room and the eye
    // immediately locks onto the grid. Texture should be felt, not read -- if a
    // player can point at the repeating shape, it is too strong.
    if (variant == 1) {
        c.px(3, 6, shade(base, 0.86f));
        c.px(4, 6, shade(base, 0.86f));
        c.px(5, 7, shade(base, 0.86f));
    } else if (variant == 2) {
        c.rect(9, 9, 4, 3, shade(base, 0.9f));
        c.hLine(9, 9, 4, shade(base, 1.06f));
    } else if (variant == 3) {
        c.px(4, 11, shade(base, 0.88f));
        c.px(11, 4, shade(base, 0.88f));
        c.px(12, 12, shade(base, 1.08f));
    }
}

/// The top surface of a wall block, seen from above.
void drawWallTop(Canvas& c) {
    const sf::Color base(46, 42, 58);
    c.rect(0, 0, kFrame, kFrame, base);
    for (int y = 0; y < kFrame; ++y) {
        for (int x = 0; x < kFrame; ++x) {
            if ((hash2(x, y, 77u) & 15u) == 0) c.px(x, y, shade(base, 1.1f));
        }
    }
    c.hLine(0, 0, kFrame, shade(base, 1.25f));
}

/// The vertical face of a wall, drawn on blocks that have floor below them.
///
/// This is the single biggest change to how the level reads. A top-down map
/// where walls and floors are both flat squares gives no sense of height; adding
/// a lit brick face to the south-facing edge instantly makes walls look solid
/// and the floor look walkable.
void drawWallFace(Canvas& c, bool lit) {
    const sf::Color base = lit ? sf::Color(92, 82, 104) : sf::Color(66, 60, 78);
    c.rect(0, 0, kFrame, kFrame, base);

    // Brick courses, offset every other row.
    const sf::Color mortar = shade(base, 0.62f);
    for (int y = 0; y < kFrame; y += 5) {
        c.hLine(0, y, kFrame, mortar);
        const int offset = ((y / 5) % 2 == 0) ? 0 : 8;
        c.vLine(offset, y, 5, mortar);
        c.vLine((offset + 8) % kFrame, y, 5, mortar);
    }
    // Top highlight: catches the light and separates the face from the top.
    c.hLine(0, 0, kFrame, shade(base, 1.3f));
    c.hLine(0, kFrame - 1, kFrame, shade(base, 0.5f));
}

void drawStairs(Canvas& c) {
    const sf::Color stone(120, 200, 148);
    c.rect(1, 1, 14, 14, sf::Color(30, 40, 36));
    // Four descending steps, each lighter than the last, reading as depth.
    for (int i = 0; i < 4; ++i) {
        const int y = 2 + i * 3;
        const int inset = 1 + i;
        c.rect(inset, y, 14 - inset * 2, 2, shade(stone, 0.55f + 0.15f * i));
        c.hLine(inset, y, 14 - inset * 2, shade(stone, 0.8f + 0.15f * i));
    }
}

// -----------------------------------------------------------------------------
// Items and props
// -----------------------------------------------------------------------------

void drawPotion(Canvas& c) {
    c.rect(6, 2, 4, 2, sf::Color(150, 140, 130));       // cork
    c.rect(5, 4, 6, 2, sf::Color(190, 200, 210));       // neck
    c.disc(8.0f, 10.0f, 4.2f, sf::Color(200, 210, 225));  // glass
    c.disc(8.0f, 10.8f, 3.4f, sf::Color(214, 62, 84));    // liquid
    c.disc(6.6f, 9.4f, 1.0f, sf::Color(255, 255, 255, 190));  // specular dot
    c.outline(kOutline);
}

void drawSword(Canvas& c) {
    c.rect(7, 1, 2, 9, sf::Color(228, 234, 246));   // blade
    c.vLine(7, 1, 9, sf::Color(255, 255, 255));     // edge highlight
    c.rect(4, 10, 8, 2, sf::Color(198, 158, 72));   // crossguard
    c.rect(7, 12, 2, 3, sf::Color(120, 84, 52));    // grip
    c.px(7, 15, sf::Color(198, 158, 72));
    c.px(8, 15, sf::Color(198, 158, 72));
    c.outline(kOutline);
}

void drawShield(Canvas& c) {
    const sf::Color metal(128, 168, 208);
    c.rect(3, 2, 10, 7, metal);
    for (int i = 0; i < 5; ++i) c.rect(4 + i, 9 + i, 8 - i * 2, 1, metal);
    c.rect(3, 2, 10, 1, shade(metal, 1.3f));
    c.disc(8.0f, 7.0f, 2.2f, shade(metal, 0.7f));   // boss
    c.disc(8.0f, 6.6f, 1.2f, shade(metal, 1.15f));
    c.outline(kOutline);
}

/// Coin, drawn as four spin frames. An animated coin is worth the extra frames:
/// it's the clearest possible signal that something is a pickup rather than
/// scenery, without needing any text.
void drawCoin(Canvas& c, int frame) {
    const sf::Color gold(238, 194, 82);
    const float widths[4] = {5.0f, 3.0f, 1.2f, 3.0f};
    const float halfWidth = widths[frame % 4];

    for (int y = -5; y <= 5; ++y) {
        // Elliptical falloff. Using a linear taper here is the obvious thing and
        // it produces a diamond, not a coin -- the silhouette has to bulge.
        const float norm = static_cast<float>(y) / 5.5f;
        const float t = std::sqrt(std::max(0.0f, 1.0f - norm * norm));
        const int w = static_cast<int>(std::round(halfWidth * t * 2.0f));
        if (w <= 0) continue;
        c.rect(8 - w / 2, 8 + y, std::max(1, w), 1, gold);
    }
    if (halfWidth > 2.0f) {
        c.disc(6.8f, 6.4f, 1.2f, shade(gold, 1.25f));  // shine
        c.px(9, 10, shade(gold, 0.7f));
    }
    c.outline(kOutline);
}

void drawProjectile(Canvas& c) {
    c.disc(8.0f, 8.0f, 3.2f, sf::Color(255, 170, 70));
    c.disc(8.0f, 8.0f, 2.0f, sf::Color(255, 226, 150));
    c.disc(7.4f, 7.4f, 0.9f, sf::Color(255, 255, 255));
    c.outline(sf::Color(120, 50, 10, 200));
}

void drawMushroom(Canvas& c) {
    c.rect(7, 9, 2, 5, sf::Color(226, 214, 196));
    c.disc(8.0f, 8.0f, 4.0f, sf::Color(190, 66, 72));
    c.rect(4, 9, 8, 1, kClear);
    c.disc(6.5f, 6.5f, 1.2f, sf::Color(240, 226, 214));
    c.disc(10.0f, 7.5f, 0.9f, sf::Color(240, 226, 214));
    c.outline(kOutline);
}

void drawRock(Canvas& c) {
    const sf::Color stone(86, 82, 96);
    c.disc(7.5f, 10.0f, 4.2f, stone);
    c.disc(11.0f, 11.0f, 2.4f, shade(stone, 0.85f));
    c.disc(6.5f, 8.8f, 2.0f, shade(stone, 1.25f));
    c.rect(2, 14, 12, 2, kClear);
    c.outline(kOutline);
}

void drawBones(Canvas& c) {
    const sf::Color bone(222, 216, 200);
    c.disc(6.0f, 9.0f, 2.6f, bone);                 // skull
    c.px(5, 9, kOutline);
    c.px(7, 9, kOutline);
    c.rect(9, 11, 5, 1, bone);                      // ribs
    c.rect(9, 13, 4, 1, bone);
    c.disc(8.6f, 11.5f, 0.8f, bone);
    c.outline(kOutline);
}

/// Stalagmite -- the closest a dungeon gets to a tree, and what gives caves
/// their vertical texture.
void drawStalagmite(Canvas& c) {
    const sf::Color stone(74, 78, 96);
    for (int y = 0; y < 14; ++y) {
        const int w = 1 + (y * 6) / 14;
        c.rect(8 - w / 2, 2 + y, std::max(1, w), 1, stone);
    }
    for (int y = 0; y < 14; ++y) {
        const int w = 1 + (y * 6) / 14;
        c.px(8 - w / 2, 2 + y, shade(stone, 1.35f));  // lit left edge
    }
    c.outline(kOutline);
}

void drawCrystal(Canvas& c) {
    const sf::Color gem(110, 200, 220);
    for (int y = 0; y < 12; ++y) {
        const int w = (y < 5) ? 1 + y : 6 - (y - 5) / 2;
        c.rect(8 - w / 2, 3 + y, std::max(1, w), 1, gem);
    }
    c.vLine(7, 4, 9, shade(gem, 1.4f));
    c.vLine(9, 6, 6, shade(gem, 0.7f));
    c.outline(kOutline);
}

void drawTorch(Canvas& c, int frame) {
    c.rect(7, 8, 2, 7, sf::Color(96, 68, 44));  // bracket
    c.rect(6, 7, 4, 2, sf::Color(70, 66, 78));

    // Flame: taller and narrower on the second frame, which is enough to read as
    // flicker without a full particle effect.
    const float height = (frame == 0) ? 4.0f : 5.0f;
    for (int y = 0; y < static_cast<int>(height); ++y) {
        const float t = 1.0f - y / height;
        const int w = std::max(1, static_cast<int>(4 * t));
        c.rect(8 - w / 2, 7 - y, w, 1, sf::Color(255, 150, 50));
    }
    c.disc(8.0f, 5.5f, 1.4f, sf::Color(255, 226, 140));
    c.px(8, 3, sf::Color(255, 250, 220));
    c.outline(kOutline);
}

/// Merchant: a hooded figure, deliberately unlike any enemy silhouette so the
/// player never attacks it by mistake.
void drawMerchant(Canvas& c) {
    const sf::Color robe(92, 76, 150);
    c.rect(3, 6, 10, 8, robe);                      // robe body
    c.rect(3, 6, 10, 1, shade(robe, 1.25f));
    c.disc(8.0f, 5.0f, 4.0f, shade(robe, 0.85f));   // hood
    c.rect(4, 5, 8, 3, shade(robe, 0.85f));
    c.rect(6, 4, 4, 3, sf::Color(28, 24, 36));      // shadowed face
    c.px(6, 5, sf::Color(255, 214, 120));           // glinting eyes
    c.px(9, 5, sf::Color(255, 214, 120));
    c.hLine(3, 11, 10, sf::Color(198, 158, 72));    // gold trim
    c.outline(kOutline);
}

}  // namespace

// -----------------------------------------------------------------------------
// Atlas assembly
// -----------------------------------------------------------------------------

sf::Image generateAtlas() {
    sf::Image atlas;
    atlas.create(static_cast<unsigned>(kAtlasCols * kFrame),
                 static_cast<unsigned>(kAtlasRows * kFrame), sf::Color::Transparent);

    auto at = [&atlas](int col, int row) {
        Canvas c;
        c.image = &atlas;
        c.originX = col * kFrame;
        c.originY = row * kFrame;
        return c;
    };

    const BodyPalette player{sf::Color(86, 156, 224), sf::Color(58, 72, 110),
                             sf::Color(238, 200, 168), sf::Color(212, 168, 72)};

    // Row 0: idle
    { Canvas c = at(0, 0); drawHumanoid(c, player, Pose::Idle); }
    { Canvas c = at(1, 0); drawHumanoid(c, player, Pose::IdleBob); }

    // Row 1: walk cycle -- contact, pass, contact, pass
    { Canvas c = at(0, 1); drawHumanoid(c, player, Pose::Step1); }
    { Canvas c = at(1, 1); drawHumanoid(c, player, Pose::Idle); }
    { Canvas c = at(2, 1); drawHumanoid(c, player, Pose::Step2); }
    { Canvas c = at(3, 1); drawHumanoid(c, player, Pose::IdleBob); }

    // Row 2: attack
    { Canvas c = at(0, 2); drawHumanoid(c, player, Pose::Idle); }
    { Canvas c = at(1, 2); drawHumanoid(c, player, Pose::Attack); }
    { Canvas c = at(2, 2); drawHumanoid(c, player, Pose::AttackFollow); }

    // Row 3: death
    { Canvas c = at(0, 3); drawHumanoid(c, player, Pose::Idle); }
    { Canvas c = at(1, 3); drawHumanoid(c, player, Pose::Step1); }
    { Canvas c = at(2, 3); drawHumanoid(c, player, Pose::Dead); }
    { Canvas c = at(3, 3); drawHumanoid(c, player, Pose::Dead); }

    // Rows 4-6: enemies
    for (int i = 0; i < 4; ++i) { Canvas c = at(i, 4); drawShambler(c, i); }
    for (int i = 0; i < 2; ++i) { Canvas c = at(i, 5); drawSentinel(c, i); }
    for (int i = 0; i < 4; ++i) { Canvas c = at(i, 6); drawSkitterer(c, i); }

    // Row 7: a generic enemy death -- a collapsing puff, palette-tinted per type
    // at draw time rather than needing one death animation per enemy.
    for (int i = 0; i < 4; ++i) {
        Canvas c = at(i, 7);
        const float scale = 4.5f - i * 1.0f;
        const std::uint8_t alpha = static_cast<std::uint8_t>(230 - i * 50);
        c.disc(8.0f, 10.0f + i, scale, sf::Color(150, 150, 160, alpha));
        c.outline(sf::Color(20, 18, 26, alpha));
    }

    // Row 8: floors and stairs
    for (int i = 0; i < 4; ++i) { Canvas c = at(i, 8); drawFloor(c, i); }
    { Canvas c = at(4, 8); drawStairs(c); }

    // Row 9: walls
    { Canvas c = at(0, 9); drawWallTop(c); }
    { Canvas c = at(1, 9); drawWallFace(c, false); }
    { Canvas c = at(2, 9); drawWallFace(c, true); }

    // Row 10: items
    { Canvas c = at(0, 10); drawPotion(c); }
    { Canvas c = at(1, 10); drawSword(c); }
    { Canvas c = at(2, 10); drawShield(c); }
    { Canvas c = at(3, 10); drawCoin(c, 0); }
    { Canvas c = at(4, 10); drawProjectile(c); }

    // Row 11: coin spin
    for (int i = 0; i < 4; ++i) { Canvas c = at(i, 11); drawCoin(c, i); }

    // Row 12: decor
    { Canvas c = at(0, 12); drawMushroom(c); }
    { Canvas c = at(1, 12); drawRock(c); }
    { Canvas c = at(2, 12); drawBones(c); }
    { Canvas c = at(3, 12); drawStalagmite(c); }
    { Canvas c = at(4, 12); drawCrystal(c); }

    // Row 13: torch and merchant
    { Canvas c = at(0, 13); drawTorch(c, 0); }
    { Canvas c = at(1, 13); drawTorch(c, 1); }
    { Canvas c = at(2, 13); drawMerchant(c); }

    return atlas;
}

}  // namespace game