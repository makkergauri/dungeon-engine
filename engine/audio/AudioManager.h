#pragma once

#include <SFML/Audio.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
class AudioManager {
public:
    bool loadSound(const std::string& name, const std::string& path);
    bool loadMusic(const std::string& path);
    bool createSound(const std::string& name, const std::vector<sf::Int16>& samples,
                     unsigned int sampleRate = 44100, unsigned int channels = 1);
    bool createMusic(const std::vector<sf::Int16>& samples, unsigned int sampleRate = 44100,
                     unsigned int channels = 1);
    void playSound(const std::string& name, float volume = 100.0f, float pitchJitter = 0.08f);

    void playMusic(bool loop = true);
    void stopMusic();
    void pauseMusic(bool paused);

    void setMasterVolume(float volume);   // 0..100
    void setMusicVolume(float volume);
    float masterVolume() const { return masterVolume_; }

private:
    sf::Sound* acquireVoice();

    std::unordered_map<std::string, std::unique_ptr<sf::SoundBuffer>> buffers_;
    std::vector<std::unique_ptr<sf::Sound>> voices_;
    sf::Music music_;
    bool musicLoaded_ = false;
    /// Generated-music path: buffer plus its dedicated looping voice.
    sf::SoundBuffer generatedMusicBuffer_;
    sf::Sound generatedMusic_;
    bool generatedMusicLoaded_ = false;
    float masterVolume_ = 70.0f;
    float musicVolume_ = 45.0f;

    static constexpr std::size_t kVoiceCount = 32;
};

}  