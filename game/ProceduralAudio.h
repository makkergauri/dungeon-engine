#pragma once

#include <SFML/Config.hpp>

#include <string>
#include <vector>

namespace game {

/// One synthesised sound: raw 16-bit mono PCM plus its sample rate.
struct GeneratedSound {
    std::vector<sf::Int16> samples;
    unsigned int sampleRate = 44100;
};

GeneratedSound makeSwing();    // sword whoosh
GeneratedSound makeHit();      // impact on an enemy
GeneratedSound makeHurt();     // the player taking damage
GeneratedSound makeDeath();    // an enemy dying
GeneratedSound makePickup();   // coin / item chime
GeneratedSound makeStairs();   // descending a floor
GeneratedSound makeAmbientLoop();

}  