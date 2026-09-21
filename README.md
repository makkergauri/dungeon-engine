# Dungeon

A 2D game engine I wrote from scratch in C++17, and a procedurally generated
dungeon crawler built on it.

![Gameplay](screenshots/gameplay.gif)

Nothing in `engine/` knows what a dungeon is — it just provides an ECS, a
fixed-timestep loop, a batching renderer, AABB physics, input and audio. The
game is one thing you could build with it.

All the art and sound is generated in code. No asset files ship with the repo.

## Build

Needs CMake 3.16+ and **SFML 2.6.x** (not 3 — the API changed, and vcpkg's port
is 3.x now, so use the official binaries from sfml-dev.org).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && ./dungeon
```

Windows: add `"-DCMAKE_PREFIX_PATH=C:/SFML-2.6.2"` to the configure step and copy
SFML's DLLs next to the exe.

## Play

WASD to move, Space to attack, E for stairs and the merchant, Esc to pause,
**H for how to play**. Clear eight floors to win.

## What's interesting

**A\*** with a binary heap and octile heuristic, corner-cut prevention so enemies
don't clip through doorways, and path smoothing. I skip the search when there's a
clear line to the player, throttle repathing, and cap it at 4 searches per step
(F1 shows the count).

**Two generators** — BSP rooms and cellular-automata caves, picked at the menu.
Every floor is flood-filled to guarantee it's fully connected, and repaired
rather than regenerated if it isn't.

**Fixed timestep** with a frame-time clamp, so physics behaves identically on a
stuttering laptop and a seeded run replays exactly.

**Generated assets** — the sprite atlas and every sound are built at startup.
Drop in a real `atlas.png` or `hit.wav` and it overrides them, per file.

## Tests

```bash
cd build && ctest
```

29 cases, ~45k assertions — collision edge cases, A\* optimality against
hand-computed costs, connectivity across 240 seeds, ECS index bookkeeping. They
link `dungeon_core`, which has no SFML dependency, so no window or GPU needed.

## Layout
engine/ loop, ECS, rendering, physics, input, audio
game/ entities, generation, AI, combat, shop, UI
tests/
docs/
