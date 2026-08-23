# Architecture

How the engine is put together and why it is shaped this way.

---

## 1. Layering

```
game/     dungeon crawler: floors, enemies, loot, screens
   |      (depends on engine)
engine/   ECS, loop, rendering, physics, input, audio
   |      (depends on SFML — and only in half its files)
SFML
```

The rule enforced throughout: **nothing in `engine/` mentions a dungeon-crawler
concept.** No floors, no loot, no goblins. The engine could carry a completely
different 2D game without edits.

The interesting question is how physics resolves collision against a level
without knowing what a level is. The answer is a one-method interface:

```cpp
class SolidGrid {
    virtual bool isSolid(int tileX, int tileY) const = 0;
    virtual float tileSize() const = 0;
    // ...
};
```

`Dungeon` implements it. `PhysicsSystem` holds a `const SolidGrid*` and only ever
asks "is this cell solid". That single indirection is the whole contract between
the two layers.

There is a second, sharper boundary inside the engine: **`dungeon_core`**
(ECS, collision, physics, timing, camera, plus the game's pathfinding and
generation) includes no SFML headers at all. It builds and links on a machine
with no graphics stack, which is exactly what the test suite relies on.

---

## 2. Entity-Component-System

### Why not inheritance

The obvious design is a `GameObject` base class with `Player : Enemy : Item`
below it. It reads well for about a week. Then you want an item that shoots, or
a destructible wall, or an enemy that can be picked up — and the hierarchy has
to be reshaped, because inheritance forces every entity to commit up front to a
single position in a tree.

Composition doesn't have that problem. An entity is an ID; what it *is* is
decided at runtime by which components it happens to own. A projectile is
`Transform + Velocity + Collider + Hitbox + Sprite`. Make it stationary by not
adding `Velocity`. Make it a pickup by swapping `Hitbox` for `ItemPickup`. No
class is edited and no hierarchy is disturbed.

Three concrete wins in this codebase:

- **Decoupling.** `PhysicsSystem` moves everything with `Transform + Velocity +
  Collider`. It has never heard of a player. `LifetimeSystem` ages out corpses,
  particles-as-entities and hitboxes with one loop, because they share a
  component rather than a base class.
- **Data locality.** Components of one type live packed in a contiguous
  `std::vector`. A system walking every `Velocity` touches one cache-friendly
  array instead of chasing pointers across the heap. With a few hundred entities
  this is not a measured win here — but it is the reason the layout is worth
  having as entity counts grow.
- **Testability.** A component is a plain struct with no constructor logic and
  no back-pointer to the world. A test builds one in a line and points a real
  system at a throwaway registry.

### Implementation

```
Registry
 ├── ComponentPool<Transform>   entities[] ─┐ parallel arrays
 │                              components[]┘
 │                              lookup: Entity -> dense index
 ├── ComponentPool<Velocity>
 └── ...
```

Details worth knowing:

- **Swap-and-pop removal.** Order within a pool is never meaningful, so removal
  moves the last element into the hole and rewrites its index. Removal is O(1)
  and the array stays dense. This index bookkeeping is the easiest thing in the
  ECS to get subtly wrong — one entity silently reading another's data — so it
  has dedicated tests.
- **Deferred destruction.** `destroy()` queues; `flushDestroyed()` at the end of
  the fixed step actually removes. Systems kill entities mid-iteration
  constantly (an enemy dies while combat is walking enemies), and yanking
  components out immediately would invalidate the pool being walked.
- **Monotonic IDs.** IDs are never recycled, so there are no stale-handle bugs
  and no need for generation counters. At 32 bits you would have to spawn an
  entity every frame for two years to exhaust them.
- **`each<A, B>()` drives off the first type.** Call sites list the rarest
  component first: `each<Health, Transform>` walks the few damageable entities,
  while the reverse would walk everything and reject most of it. A heavier ECS
  picks the smallest pool automatically; doing that generically requires runtime
  dispatch over the type list, which isn't worth the complexity at this scale.
- **Iteration is snapshot-based.** The callback may spawn entities or add
  components, either of which can reallocate the pool underneath the loop.

### System ordering

Systems run in registration order, and the order is load-bearing:

```
PlayerSystem   -> reads input, sets velocity
EnemyAISystem  -> pathfinds, sets velocity
PhysicsSystem  -> integrates, resolves walls, emits overlap events
CombatSystem   -> reacts to those overlaps
ItemSystem, AnimationSystem, LifetimeSystem
```

Running combat before physics would resolve *last* frame's overlaps against
*this* frame's positions.

Components split across two headers by ownership: `engine/ecs/Component.h` holds
the reusable ones (`Transform`, `Velocity`, `Collider`, `Health`, `Lifetime`),
`game/GameCommon.h` holds the game-specific ones (`PlayerTag`, `EnemyTag`,
`AIState`, `ItemPickup`). The registry is generic, so this costs nothing and
keeps the layer boundary visible in the file tree.

---

## 3. The fixed-timestep loop

```cpp
time.beginFrame();                  // clamped delta into the accumulator

while (time.consumeFixedStep()) {   // 0..n times
    systems.fixedUpdate(registry, 1.0f/60.0f);
    registry.flushDestroyed();
}

systems.update(registry, frameDelta);   // once, at display rate
particles.update(frameDelta);
camera.update(frameDelta);
render();
```

### Why not just use the frame delta

Feeding the raw frame delta into movement and collision means a machine that
stutters gets *different physics*. Entities move further per step, and past a
certain step size they clip through walls the collision code would otherwise
have caught. Bugs become unreproducible because they depend on frame timing.

Constant steps fix that and buy determinism: with the same seed and the same
inputs, a run replays identically. That is the only reason a movement bug is
ever debuggable.

### The spiral of death, and the clamp

The naive accumulator has a failure mode. A long stall banks two seconds; the
loop runs 120 catch-up steps; that takes longer than a frame; which banks more
time. The simulation never catches up and the game locks solid.

`Time` clamps any single frame's contribution to 0.25s. Past that, simulation
time is *dropped* rather than banked — a stall costs a visible hitch instead of
a freeze. The loop is therefore bounded at ~15 steps.

### The split

- **`fixedUpdate`** — anything affecting gameplay: input, movement, collision,
  AI decisions, damage.
- **`update`** — cosmetic only: animation timers, particle fade, camera
  smoothing. These run at display rate so they stay smooth at 144Hz while
  physics stays at 60.

`Time::alpha()` exposes the interpolation factor between simulation states. At
32px tiles and 60Hz the visual difference from interpolating is imperceptible,
so entities render at their current position — but the value is there and
correct if it's ever wanted.

One subtlety: `discardLostTime()` is called after asset loading and after every
screen change. Otherwise the accumulator opens holding several seconds of debt
and the first frame back fast-forwards through it.

---

## 4. Procedural generation

Two algorithms behind one interface, chosen by the player at the menu.

### BSP — rooms and corridors

Recursively split the map into regions, carve one room per leaf, connect
siblings bottom-up.

```
┌─────────────┐   ┌──────┬──────┐   ┌──────┬──────┐
│             │ → │      │      │ → │ ┌──┐ │ ┌─┐  │
│             │   │      │      │   │ └──┘─┼─┴─┘  │
└─────────────┘   └──────┴──────┘   └──────┴──────┘
   region           split              rooms + corridor
```

**Why BSP over random room placement.** The obvious alternative — scatter
rectangles, reject overlaps, connect nearest neighbours — degrades badly. Room
count is capped by rejection rate, the distribution clumps, and connection is a
separate problem you have to solve with a spanning tree.

BSP gets both for free. Partitioning guarantees rooms cannot overlap (they live
in disjoint regions), and the tree *is* the connectivity plan: joining the two
children of every internal node connects the whole map by construction, with no
separate graph pass. It also gives a natural difficulty dial — `minLeafSize`
trades few large rooms against many small ones.

Implementation notes:

- Splitting is **iterative, breadth-first**, not recursive. Recursion would hold
  references into `leaves_` across a `push_back` that can reallocate it.
- Splits run **across the long axis**, otherwise you get slivers too thin to hold
  a room, which become dead corridor space.
- Connection searches the subtree for a leaf that actually carved a room, rather
  than assuming the immediate child has one — some regions are too small.
- Corridors are two tiles wide. One-tile corridors look tidy in a screenshot and
  feel awful to walk through.
- L-corridor leg order is a coin flip; always going horizontal-first produces a
  visible grid-like regularity across the floor.

### Cellular automata — caves

Fill randomly at 45% wall, then apply the 4-5 rule: a cell becomes wall if 5+ of
its 8 neighbours are walls, floor if 3 or fewer, **unchanged at exactly 4**.
Preserving ties is what keeps thin natural-looking walls instead of eroding them.

Raw output is a spray of disconnected caverns. Post-processing labels every
region, fills in anything below `minCaveSize` (a three-tile pocket joined by a
corridor reads as a bug; filled in, nobody knows it existed), and tunnels each
survivor to the largest region between their closest sampled pair.

### Connectivity validation

The property that actually matters: **every floor tile is reachable from the
entrance.** A sealed-off treasure room isn't a quirk — to the player it's a
broken game.

After generation, a 4-connected BFS floods from the entrance and compares the
count against total floor tiles. Deliberately 4-connected even though movement is
8-way, because corner-cutting is disabled: two regions touching only diagonally
are genuinely separate.

On failure the generator **repairs rather than retries**: find an orphaned tile,
find the nearest reachable tile, carve between them, repeat. Regenerating from
scratch on failure is the usual answer, but it makes generation time unbounded
and hides real bugs behind a retry loop.

The exit is placed at the greatest BFS **walking** distance from the entrance.
Straight-line distance is wrong: two rooms can sit adjacent on screen and be a
long way apart through the corridors, and the corridors are what the player
walks. Searching tiles rather than room centres also handles caves, where a
"room" is a bounding box around an irregular blob and its centre is frequently
a wall. (This was a real bug — caught by the test suite, which found a seed
where the exit landed on the entrance.)

---

## 5. A\* pathfinding

`Pathfinder::findPath` over an 8-connected `NavGrid`.

### The heuristic

Octile distance — the exact cost of an unobstructed 8-way walk:

```
h = √2 · min(dx, dy) + 1 · (max(dx, dy) − min(dx, dy))
```

Two properties matter. It is **admissible** (never overestimates), so A\* still
returns shortest paths. And it is **tight** — far tighter than Manhattan or
Euclidean on a grid — so far fewer nodes get expanded. Using `1.0` for diagonal
cost instead of `√2` would break admissibility and silently return non-optimal
paths, which is the kind of bug you don't notice until an enemy takes a visibly
stupid route. Tests pin the exact costs.

### Implementation details

- **Binary heap** via `std::priority_queue`, comparator inverted for min-heap
  behaviour.
- **No decrease-key.** `std::priority_queue` can't do it, so improved nodes are
  pushed again and stale duplicates are skipped when popped (`if (closed) continue`).
  Standard, and cheaper than hand-rolling an indexed heap.
- **Search-ID stamping instead of clearing.** Scratch arrays (g-score, parent,
  closed) are sized once. Each search bumps a counter; a cell whose stamp doesn't
  match is treated as unvisited. On a 96×64 map that avoids ~6000 cells of memset
  per repath, per enemy.
- **Corner-cut prevention.** A diagonal move requires both orthogonal neighbours
  to be open, otherwise enemies squeeze through the gap between two wall corners.
  Toggleable, and both settings are tested.
- **Node budget.** Capped expansions so an unreachable target can't stall a frame
  exploring the whole map.

### Path smoothing

Raw grid paths are visible staircases. `simplify()` greedily reaches for the
furthest waypoint still in line of sight, typically collapsing ~54 waypoints to
~10. The line-of-sight walk applies the same corner rule as the search —
otherwise smoothing would happily shortcut through a wall the search carefully
avoided.

### Using it in the AI

Three things keep A\* off the critical path:

1. **Line of sight first.** If nothing is in the way, steer straight at the
   player. No graph search runs at all, and in an open room this is the common
   case.
2. **Throttled repathing.** Each enemy repaths every 0.3–0.5s, with the initial
   phase offset derived from its entity ID so enemies spawned on the same frame
   don't repath in lockstep forever.
3. **A per-step budget.** At most 4 searches per fixed step. An enemy that misses
   its slot keeps following its previous path for another few hundred
   milliseconds — imperceptible in play.

The overlay (`F1`) reports searches per step so this is observable rather than
assumed.

### Enemy FSM

```
Idle ──(in aggro range + line of sight)──> Chase
Chase ──(within attack range)──> Attack ──(beyond range × 1.35)──> Chase
Chase ──(beyond lose-interest range)──> Idle
any ──(health ≤ 0)──> Dead
```

Two pieces of hysteresis, both there because the naive version oscillates:
`loseInterestRange` is larger than `aggroRange`, and leaving Attack requires
exceeding the range by 35%. Without them an enemy hovering at exactly the
boundary flickers between states every frame.

Three enemy types share the machine and differ in the behaviour hung off it:
the **Shambler** paths straight in; the **Sentinel** never moves and fires only
with line of sight (shooting through walls would be unreadable and unfair, and
requiring sight makes cover a real tactic); the **Skitterer** overlays a
persistent wander vector and a sine-driven speed burst on its pathing. Per-frame
randomness was tried and looks like vibration — noise averages to nothing, so
the wander offset persists for 0.2–0.5s instead.

---

## 6. Rendering

Draw commands are queued, sorted, and batched rather than issued per sprite.

```
submit → queue → sort(layer, depth, texture) → append quads → draw per texture run
```

Sort key order is deliberate: **layer, then depth, then texture.** Putting
texture first would give perfect batching and wrong overlap order, and a player
notices a goblin drawn on top of a wall long before they notice extra draw calls.
With a single atlas — which is how the art is authored — the texture key is
constant and this degenerates to one batch per layer anyway.

Sprites sort by their feet (`y + height/2`), not their centre, so a tall sprite
standing behind a short one resolves correctly. Horizontal mirroring is a UV
swap, not a second set of frames.

Untextured quads sample a 1×1 white pixel at texture slot 0 and take their colour
entirely from the vertex tint. That means flat rectangles and textured sprites
share one code path *and one batch* — and it's what lets the game run without any
art.

Tiles are view-culled: a 96×64 floor is 6144 tiles and the window shows maybe
400. Culling is one clamp per axis and it's the single biggest rendering win in
the project. Interior wall tiles with no exposed face are skipped entirely,
which matters most on cave maps where they dominate the grid.

**Particles are deliberately not entities.** One death burst spawns thirty, they
live half a second, and they have no behaviour worth composing. Routing that
churn through entity creation and component pools would cost more than it buys —
a flat pre-allocated array with a live count and swap-on-death is the right shape
for that data. Choosing *not* to use the architecture everywhere is part of
understanding it.

---

## 7. Physics

Movement integrates as `position += velocity · dt`, then resolves against the
tile grid **one axis at a time**.

Axis separation is what produces clean wall sliding: run diagonally into a wall
and the blocked axis is cancelled while the free axis keeps full speed. Resolving
both at once makes the character stick to walls, which feels broken long before
anyone can explain why. There's a test for exactly this.

Motion is substepped so no single step crosses more than half a tile. Without it
a fast entity starts on one side of a wall and ends on the other, with the
overlap test never seeing it inside. Also tested, at 4000 px/s.

A small skin (0.01px) keeps entities from landing exactly on a tile boundary,
where float rounding makes the next frame's overlap test ambiguous. Relatedly,
`overlaps()` uses strict comparison — boxes that merely touch don't count, or an
entity resting flush against a wall re-triggers a collision every frame.

Entity-vs-entity is an O(n²) pass over colliders, filtered first by layer/mask
bitmasks (a couple of integer ANDs discards most pairs before any box maths).
With a few dozen entities per floor that's microseconds. See
`design-decisions.md` for when this stops being true.

Physics only *detects* overlaps; it emits `OverlapEvent` pairs and stops there.
Deciding that "player touched enemy" means damage is a game-layer concern, and
`CombatSystem` is the single owner of that decision — which is what prevents two
systems both reacting to one touch and applying damage twice.
