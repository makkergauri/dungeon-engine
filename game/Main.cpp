#include <iostream>
#include <string>

#include "DungeonGame.h"


int main(int argc, char** argv) {
    engine::Window::Config config;
    config.width = 1280;
    config.height = 720;
    config.title = "Dungeon";
    config.vsync = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-vsync") {
            config.vsync = false;
            config.frameLimit = 240;
        } else if (arg == "--windowed-small") {
            config.width = 960;
            config.height = 540;
        } else if (arg == "--help") {
            std::cout << "Usage: dungeon [--no-vsync] [--windowed-small]\n";
            return 0;
        }
    }

    game::DungeonGame game;
    return game.run(config);
}
