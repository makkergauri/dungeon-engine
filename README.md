# dungeon-engine

A small 2D game engine written from scratch in C++17, and a procedurally
generated dungeon crawler built on top of it.

The engine and the game are separate layers. Nothing in `engine/` knows what a
dungeon, an enemy or a floor is — it provides an ECS, a fixed-timestep loop, a
batching renderer, AABB physics, action-based input and audio, and the game is
one possible consumer of that.

> **Screenshots**

---

## Contents

- [What's interesting here](#whats-interesting-here)
- [Building](#building)
- [Running](#running)
- [Controls](#controls)
- [Tests](#tests)
- [Project layout](#project-layout)
- [Assets](#assets)
- [Documentation](#documentation)

---

## What's interesting here

Three parts carry most of the engineering weight:

**A\* pathfinding** (`game/ai/Pathfinding.cpp`) — 8-connected grid search with a
binary heap, an octile heuristic (admissible, so paths are provably shortest),
corner-cut prevention so enemies don't clip through doorway corners, and
line-of-sight path smoothing. Scratch arrays are invalidated with a search-ID
stamp instead of being cleared, which avoids a ~6000-cell memset per repath.
Searches are budgeted per frame and staggered per enemy.

**Procedural generation** (`game/generation/DungeonGenerator.cpp`) — two
algorithms, selectable at the main menu. BSP recursively partitions the map and
connects siblings bottom-up for architectural room-and-corridor floors; cellular
automata smooths random noise into caves, discards runt regions and tunnels
between the survivors. Both are validated by flood fill and repaired if
validation fails, so **every generated floor is guaranteed fully connected**.
The exit is placed at the greatest BFS *walking* distance from the entrance,
not straight-line distance.

**A fixed-timestep loop** (`engine/core/Game.cpp`, `engine/core/Time.cpp`) —
gameplay advances in constant steps while rendering runs as fast as the display
allows, with a frame-time clamp that prevents the "spiral of death". This is
why a seeded run is reproducible and why physics behaves identically on a
stuttering laptop and a 240Hz desktop.

Also worth a look: the ECS (`engine/ecs/Entity.h`) uses packed component pools
with swap-and-pop removal and deferred destruction; the renderer sorts and
batches quads so a floor costs a handful of draw calls rather than one per
sprite; and `dungeon_core` — ECS, physics, timing, camera, pathfinding,
generation — links with **no SFML dependency at all**, which is what lets the
test suite exercise real production code with no window or GPU.

## Building

Requires a C++17 compiler, CMake 3.16+, and **SFML 2.6.x**.

> SFML 3 is not supported. It renamed a significant amount of the API (`sf::Quads`
> was removed, event handling moved to `std::variant`). This targets SFML 2.

### Install SFML

```bash
# Debian / Ubuntu
sudo apt install libsfml-dev

# macOS
brew install sfml@2

# Windows / reproducible builds — vcpkg
vcpkg install sfml
# then configure with:
#   -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
```

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Outputs `build/dungeon` (and `build/dungeon_tests`). Assets are copied next to
the binary automatically.

If SFML isn't found, CMake warns and skips the game executable — the core
library and the tests still build, which keeps CI useful on machines with no
graphics stack.

Useful options:

| Option | Default | Effect |
|---|---|---|
| `DUNGEON_BUILD_TESTS` | `ON` | Build the Catch2 suite |
| `DUNGEON_WARNINGS_AS_ERRORS` | `OFF` | `-Werror` / `/WX` |

Catch2 v3 is used if installed; otherwise CMake fetches it.

## Running

```bash
cd build
./dungeon
```

| Flag | Effect |
|---|---|
| `--no-vsync` | Disable vsync, cap at 240fps (useful for checking frame pacing) |
| `--windowed-small` | Start at 960×540 |

At the main menu, pick a generation style: **1** for BSP rooms-and-corridors,
**2** for cellular-automata caves.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | `WASD` / arrows | Left stick |
| Attack | `Space` | A |
| Descend stairs | `E` | X |
| Pause | `Esc` | Start |
| Confirm | `Enter` | A |
| Debug overlay | `F1` | — |

Input is bound to abstract *actions*, not keys. `DungeonGame::bindInput()` is
the only function in the entire game that names a physical key, so remapping
means editing one table.

The debug overlay shows fps, entity count, quads submitted, draw calls issued,
A\* searches this step, and live particles. Quads-vs-draw-calls is the pair
worth watching — if they start tracking each other, batching has broken.

## Tests

```bash
cd build && ctest --output-on-failure
# or run the binary directly for Catch2's own output:
./dungeon_tests
```

29 test cases, ~45,000 assertions. Coverage focuses on the parts where a bug is
silent rather than loud:

- **Collision** — overlap edge cases (touching boxes must *not* count), minimum
  translation vector axis selection, swept AABB, plus physics-level tests that a
  fast body can't tunnel through a wall and that blocking one axis leaves the
  other free.
- **Pathfinding** — optimality against hand-computed octile costs, a maze that
  defeats greedy movement, unreachable/blocked/out-of-bounds handling, the node
  budget, corner-cutting on and off, simplification validity, and reuse across
  many searches (to catch stale scratch state).
- **Generation** — full connectivity across 240 seeds spanning both algorithms,
  entrance-to-exit reachability cross-checked with A\*, determinism per seed,
  sealed map borders, and spawn-candidate safety.
- **ECS** — component lifetimes, deferred destruction, `each()` filtering,
  mutation during iteration, and swap-and-pop index bookkeeping.

Every test links `dungeon_core` only — no window, no GPU, no audio device.

## Project layout

```
engine/            # game-agnostic; no dungeon-crawler concepts appear here
  core/            # Game loop, Window, Time (fixed timestep)
  ecs/             # Entity/Registry, components, System interface
  rendering/       # Renderer (batching), Sprite/Animation, Camera, ParticleSystem
  physics/         # AABB collision, movement integration + tile resolution
  input/           # Action-based InputManager
  audio/           # SFX voice pool + music
game/
  entities/        # Player, Enemy (+ AI system), Item
  generation/      # DungeonGenerator (BSP + cellular automata), Room
  ai/              # A* pathfinding
  combat/          # Damage, pickups, death
  ui/              # HUD and menu screens
  DungeonGame.*    # Screen state machine, floor lifecycle, event wiring
tests/
docs/
```

The physics layer talks to levels through an abstract `engine::SolidGrid`
interface — the engine never learns what a dungeon is, it only ever asks "is
this cell solid".

## Assets

The repository ships **without** art or audio. Each `assets/` subfolder has a
README describing the expected files and the atlas layout.

Missing assets degrade gracefully rather than crashing: sprites render as flat
tinted quads, sounds are skipped, text is omitted. A fresh clone is playable
immediately — but drop in a pixel-art pack before recording anything.

## Recording a demo

This project is judged on the demo far more than on the source. A short clip
should show: the main menu with both generation styles, a fight against more
than one enemy type, taking a hit (screen shake + i-frame blink), an enemy death
burst, a pickup, and descending to a new floor.

`--no-vsync` gives smoother capture on some setups. OBS or `wf-recorder` on
Linux, OBS or QuickTime on macOS.

## Documentation

- [`docs/architecture.md`](docs/architecture.md) — ECS design, the fixed-timestep
  loop, why BSP, how A\* is implemented.
- [`docs/design-decisions.md`](docs/design-decisions.md) — tradeoffs taken
  knowingly, and what would change with more time.
