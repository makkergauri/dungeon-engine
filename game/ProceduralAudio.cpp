#include "ProceduralAudio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game {
namespace {

constexpr unsigned int kRate = 44100;
constexpr float kPi = 3.14159265358979f;

struct Noise {
    std::uint32_t state = 0x13579BDFu;
    float next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        // Map to [-1, 1].
        return (static_cast<float>(state & 0xFFFFFF) / 8388607.5f) - 1.0f;
    }
};


float decay(float t, float rate) { return std::exp(-t * rate); }
struct LowPass {
    float value = 0.0f;
    float alpha = 0.2f;
    float process(float input) {
        value += alpha * (input - value);
        return value;
    }
};

/// Soft clip, so layered voices never wrap around into a nasty crackle.
sf::Int16 toSample(float v) {
    v = std::tanh(v);
    return static_cast<sf::Int16>(std::clamp(v * 30000.0f, -32000.0f, 32000.0f));
}

std::vector<sf::Int16> allocate(float seconds) {
    return std::vector<sf::Int16>(static_cast<std::size_t>(seconds * kRate), 0);
}

}  

GeneratedSound makeSwing() {
    // Filtered noise with a falling cutoff: the classic "air moving" cue. The
    // sweep is what sells direction -- static noise just sounds like TV static.
    GeneratedSound out;
    out.samples = allocate(0.18f);

    Noise noise;
    LowPass filter;
    const std::size_t n = out.samples.size();

    for (std::size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float progress = static_cast<float>(i) / n;

        filter.alpha = 0.55f - 0.45f * progress;  // brightness falls off
        float v = filter.process(noise.next());
        // Fade in fast, out slower, so it has an attack rather than a click.
        const float env = std::min(1.0f, progress * 12.0f) * decay(t, 16.0f);
        out.samples[i] = toSample(v * env * 1.6f);
    }
    return out;
}

GeneratedSound makeHit() {
    // Low body (the thump) + a short noise transient (the crack). Layering the
    // two is what separates "impact" from "beep".
    GeneratedSound out;
    out.samples = allocate(0.22f);

    Noise noise;
    LowPass filter;
    filter.alpha = 0.25f;
    const std::size_t n = out.samples.size();

    for (std::size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kRate;
        // Pitch drops as it decays, which reads as something heavy landing.
        const float freq = 180.0f * decay(t, 9.0f) + 55.0f;
        const float body = std::sin(2.0f * kPi * freq * t) * decay(t, 13.0f);
        const float crack = filter.process(noise.next()) * decay(t, 45.0f);
        out.samples[i] = toSample(body * 1.1f + crack * 0.9f);
    }
    return out;
}

GeneratedSound makeHurt() {
    // Falling two-tone. Descending pitch is near-universally read as "bad thing
    // happened", which is exactly the message.
    GeneratedSound out;
    out.samples = allocate(0.34f);
    const std::size_t n = out.samples.size();

    for (std::size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float freq = 420.0f * decay(t, 4.5f) + 90.0f;
        // Slight square character via a soft third harmonic: harsher than a pure
        // sine, which suits damage.
        const float tone = std::sin(2.0f * kPi * freq * t) +
                           0.35f * std::sin(6.0f * kPi * freq * t);
        out.samples[i] = toSample(tone * decay(t, 7.0f) * 0.8f);
    }
    return out;
}

GeneratedSound makeDeath() {
    // Longer fall with noise mixed in: tone for the "creature", noise for the
    // collapse.
    GeneratedSound out;
    out.samples = allocate(0.5f);

    Noise noise;
    LowPass filter;
    const std::size_t n = out.samples.size();

    for (std::size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float progress = static_cast<float>(i) / n;

        const float freq = 300.0f * decay(t, 3.2f) + 45.0f;
        const float tone = std::sin(2.0f * kPi * freq * t) * decay(t, 4.0f);

        filter.alpha = 0.4f - 0.3f * progress;
        const float rumble = filter.process(noise.next()) * decay(t, 6.0f);

        out.samples[i] = toSample(tone * 0.9f + rumble * 0.7f);
    }
    return out;
}

GeneratedSound makePickup() {
    // Two-note rising blip with a bell-like harmonic. Rising intervals read as
    // "gained", and the sixth above is bright without being shrill.
    GeneratedSound out;
    out.samples = allocate(0.26f);
    const std::size_t n = out.samples.size();
    const std::size_t split = n / 3;

    for (std::size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kRate;
        const bool second = i > split;
        const float base = second ? 1244.5f : 830.6f;  // roughly G#5 -> D#6
        const float local = second ? t - static_cast<float>(split) / kRate : t;

        const float tone = std::sin(2.0f * kPi * base * local) +
                           0.4f * std::sin(4.0f * kPi * base * local) +
                           0.15f * std::sin(6.0f * kPi * base * local);
        out.samples[i] = toSample(tone * decay(local, 14.0f) * 0.55f);
    }
    return out;
}

GeneratedSound makeStairs() {
    // Four-note rising arpeggio: a small fanfare for the only genuinely
    // rewarding moment on a floor.
    GeneratedSound out;
    out.samples = allocate(0.7f);
    const std::size_t n = out.samples.size();

    const float notes[4] = {392.0f, 523.3f, 659.3f, 784.0f};  // G4 C5 E5 G5
    const std::size_t step = n / 4;

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t index = std::min<std::size_t>(3, i / step);
        const float local = static_cast<float>(i - index * step) / kRate;
        const float freq = notes[index];

        const float tone = std::sin(2.0f * kPi * freq * local) +
                           0.3f * std::sin(4.0f * kPi * freq * local);
        out.samples[i] = toSample(tone * decay(local, 6.0f) * 0.5f);
    }
    return out;
}

GeneratedSound makeAmbientLoop() {
    // 16 seconds of low drone plus a slow arpeggio in A minor.
    //
    // Loop length matters more than content: a short loop becomes maddening
    // within a couple of minutes, and this is meant to sit under the whole run.
    GeneratedSound out;
    constexpr float kSeconds = 16.0f;
    out.samples = allocate(kSeconds);
    const std::size_t n = out.samples.size();

    Noise noise;
    LowPass rumbleFilter;
    rumbleFilter.alpha = 0.004f;  // very dark: this is "air", not hiss

    // A minor: A2 drone, with C4 / E4 / A4 / E4 drifting over it.
    const float droneA = 110.0f;
    const float melody[4] = {261.6f, 329.6f, 440.0f, 329.6f};
    const float noteLength = kSeconds / 4.0f;

    for (std::size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kRate;

        // Drone: two detuned oscillators beating slowly against each other,
        // which keeps a sustained tone from sounding synthetic and static.
        float value = 0.22f * std::sin(2.0f * kPi * droneA * t);
        value += 0.18f * std::sin(2.0f * kPi * (droneA * 1.004f) * t);
        value += 0.10f * std::sin(2.0f * kPi * (droneA * 0.5f) * t);

        // Melody note with a slow swell, so notes fade in rather than pluck.
        const int index = static_cast<int>(t / noteLength) % 4;
        const float local = std::fmod(t, noteLength);
        const float swell = std::sin(kPi * std::min(1.0f, local / noteLength));
        value += 0.14f * std::sin(2.0f * kPi * melody[index] * t) * swell;

        // Filtered noise floor for room tone.
        value += rumbleFilter.process(noise.next()) * 0.5f;

        // Crossfade the last half second into the start so the loop point is
        // inaudible. Without this every repeat lands on an obvious click.
        const float fade = 0.5f;
        if (t > kSeconds - fade) {
            value *= (kSeconds - t) / fade;
        } else if (t < fade) {
            value *= t / fade;
        }

        out.samples[i] = toSample(value * 0.55f);
    }
    return out;
}

} 