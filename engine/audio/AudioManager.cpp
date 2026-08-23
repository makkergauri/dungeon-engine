#include "AudioManager.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace engine {

bool AudioManager::loadSound(const std::string& name, const std::string& path) {
    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (!buffer->loadFromFile(path)) {
        std::cerr << "[audio] missing sound: " << path << "\n";
        return false;
    }
    buffers_[name] = std::move(buffer);
    return true;
}

bool AudioManager::loadMusic(const std::string& path) {
    // Music is streamed rather than fully decoded, so this only opens the file.
    musicLoaded_ = music_.openFromFile(path);
    if (!musicLoaded_) std::cerr << "[audio] missing music: " << path << "\n";
    return musicLoaded_;
}

bool AudioManager::createSound(const std::string& name, const std::vector<sf::Int16>& samples,
                               unsigned int sampleRate, unsigned int channels) {
    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (samples.empty() || !buffer->loadFromSamples(samples.data(), samples.size(), channels,
                                                    sampleRate)) {
        return false;
    }
    buffers_[name] = std::move(buffer);
    return true;
}

bool AudioManager::createMusic(const std::vector<sf::Int16>& samples, unsigned int sampleRate,
                               unsigned int channels) {
    if (samples.empty() || !generatedMusicBuffer_.loadFromSamples(samples.data(),
                                                                  samples.size(), channels,
                                                                  sampleRate)) {
        return false;
    }
    generatedMusic_.setBuffer(generatedMusicBuffer_);
    generatedMusic_.setLoop(true);
    generatedMusicLoaded_ = true;
    return true;
}

sf::Sound* AudioManager::acquireVoice() {
    for (auto& voice : voices_) {
        if (voice->getStatus() != sf::Sound::Playing) return voice.get();
    }
    if (voices_.size() < kVoiceCount) {
        voices_.push_back(std::make_unique<sf::Sound>());
        return voices_.back().get();
    }
    return voices_.front().get();
}

void AudioManager::playSound(const std::string& name, float volume, float pitchJitter) {
    auto it = buffers_.find(name);
    if (it == buffers_.end()) return;

    sf::Sound* voice = acquireVoice();
    voice->setBuffer(*it->second);
    voice->setVolume(volume * (masterVolume_ / 100.0f));

    if (pitchJitter > 0.0f) {
        const float jitter = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
        voice->setPitch(1.0f + jitter * pitchJitter);
    } else {
        voice->setPitch(1.0f);
    }
    voice->play();
}

void AudioManager::playMusic(bool loop) {
    // A real file always wins; the generated loop is the fallback.
    if (musicLoaded_) {
        music_.setLoop(loop);
        music_.setVolume(musicVolume_ * (masterVolume_ / 100.0f));
        music_.play();
        return;
    }
    if (generatedMusicLoaded_) {
        generatedMusic_.setLoop(loop);
        generatedMusic_.setVolume(musicVolume_ * (masterVolume_ / 100.0f));
        generatedMusic_.play();
    }
}

void AudioManager::stopMusic() {
    if (musicLoaded_) music_.stop();
    if (generatedMusicLoaded_) generatedMusic_.stop();
}

void AudioManager::pauseMusic(bool paused) {
    if (musicLoaded_) {
        if (paused) {
            music_.pause();
        } else if (music_.getStatus() == sf::Music::Paused) {
            music_.play();
        }
        return;
    }
    if (!generatedMusicLoaded_) return;
    if (paused) {
        generatedMusic_.pause();
    } else if (generatedMusic_.getStatus() == sf::Sound::Paused) {
        generatedMusic_.play();
    }
}

void AudioManager::setMasterVolume(float volume) {
    masterVolume_ = volume;
    const float scaled = musicVolume_ * (masterVolume_ / 100.0f);
    music_.setVolume(scaled);
    generatedMusic_.setVolume(scaled);
}

void AudioManager::setMusicVolume(float volume) {
    musicVolume_ = volume;
    const float scaled = musicVolume_ * (masterVolume_ / 100.0f);
    music_.setVolume(scaled);
    generatedMusic_.setVolume(scaled);
}

}  