#include "Sprite.h"

#include <algorithm>

namespace engine {

const AnimationClip* AnimationLibrary::addStrip(const std::string& name, float startX,
                                                float startY, float frameWidth,
                                                float frameHeight, int frameCount,
                                                float frameDuration, bool loop) {
    auto clip = std::make_unique<AnimationClip>();
    clip->frameDuration = frameDuration;
    clip->loop = loop;
    clip->frames.reserve(static_cast<std::size_t>(std::max(0, frameCount)));
    for (int i = 0; i < frameCount; ++i) {
        clip->frames.push_back(TextureRect{startX + frameWidth * i, startY,
                                           frameWidth, frameHeight});
    }

    const AnimationClip* result = clip.get();
    storage_.push_back(std::move(clip));
    names_.push_back(name);
    return result;
}

const AnimationClip* AnimationLibrary::find(const std::string& name) const {
    // Linear scan over a few dozen clips, looked up at spawn time rather than
    // per frame. A hash map here would be strictly more code for no measurable
    // difference.
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == name) return storage_[i].get();
    }
    return nullptr;
}

void playClip(Animator& animator, const AnimationClip* clip) {
    if (animator.clip == clip) return;
    animator.clip = clip;
    animator.timer = 0.0f;
    animator.frame = 0;
    animator.finished = false;
}

void AnimationSystem::update(Registry& registry, float dt) {
    registry.each<Animator, SpriteComponent>(
        [&](Entity, Animator& animator, SpriteComponent& sprite) {
            const AnimationClip* clip = animator.clip;
            if (!clip || clip->frames.empty()) return;

            if (!animator.finished) {
                animator.timer += dt * animator.speed;
                while (animator.timer >= clip->frameDuration) {
                    animator.timer -= clip->frameDuration;
                    ++animator.frame;
                    if (animator.frame >= static_cast<int>(clip->frames.size())) {
                        if (clip->loop) {
                            animator.frame = 0;
                        } else {
                            // Hold the last frame. Death and attack animations
                            // rely on this: the sprite stays on the final pose
                            // instead of popping back to standing.
                            animator.frame = static_cast<int>(clip->frames.size()) - 1;
                            animator.finished = true;
                            break;
                        }
                    }
                }
            }

            sprite.source = clip->frames[static_cast<std::size_t>(animator.frame)];
        });
}

}  // namespace engine
