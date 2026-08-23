# Design decisions

Tradeoffs I made on purpose, and what I'd change with more time.

---

## Known limitations

**Entity collision is O(n²).** I compare every collider pair, filtered by
layer/mask bitmasks first. At ~40 entities a floor that's microseconds. It stops
being fine around 300–500. The fix is a uniform spatial hash — the map is already
a grid, so it's maybe 60 lines and the event interface wouldn't change. I didn't
build it because a spatial hash you don't need is more code to get wrong than it
saves, and the F1 overlay would tell me long before a player noticed.

**Component lookup is a hash map.** The component arrays are packed and
cache-friendly, but finding an entity's index costs a hash. A sparse array would
be one indexed read — except it grows with the highest ID ever issued, and I
never recycle IDs. Doing it properly means sparse arrays *plus* generation
counters, which is a lot of machinery for no gain at this size.

**`each<A, B>()` doesn't pick the smallest pool.** It drives off the first type,
so I have to list the rarest component first or it walks the whole world.
Automating that needs runtime dispatch over the type list. It's documented at the
declaration and every call site is ordered correctly, but it's a footgun.

**No render interpolation.** `Time::alpha()` is there and correct, I just render
at the current simulation position. At 32px tiles and 60Hz you can't see the
difference. At 30Hz you would. It's ~20 lines whenever I want it.

**Naive audio voice stealing.** Fixed pool of 32; when it's full I steal the
front voice, which isn't necessarily the oldest or quietest. A priority scheme
would be better. In practice it only runs dry on a mass death.

**No save/load.** Progress is copied by hand across floors in `descend()`, so
quitting loses the run. Since generation is fully seeded, a save file only needs
`{runSeed, floorNumber, playerStats}` to rebuild any floor exactly — determinism
already paid for this feature, I just didn't scope it in.

**The font has no fallback.** Missing art gets generated, missing audio gets
synthesised, but a missing font means `drawText` returns early and every label
vanishes — leaving a black menu that looks crashed. Inconsistent with the rest of
the asset handling and I'd fix it.

---

## Calls that could have gone the other way

**SFML over SDL2.** SDL2 is more raw and arguably the better portfolio signal,
but the interesting work here is the ECS, the fixed timestep, generation and A*.
SDL2 would have spent my time on windowing boilerplate that demonstrates none of
those. I contained it instead: SFML appears in seven files, and `dungeon_core`
includes zero SFML headers. Swapping backends means rewriting `Renderer`,
`Window`, `InputManager` and `AudioManager` — gameplay wouldn't move.

**Repair, not regenerate, on failed connectivity.** Throwing the dungeon away and
rolling a new seed is easier to write, but generation time becomes unbounded and
a retry loop *hides* bugs — a systematic flaw shows up as "generation feels slow"
instead of as a failure. Repairing stays bounded and reproducible.

**Deferred destruction.** Immediate removal is simpler right up until the first
crash, because systems destroy entities mid-iteration constantly. One flush point
per step costs a vector and kills the whole bug class.

**An event bus for game feel.** Combat raises events; `DungeonGame` wires them to
audio, particles and screen shake. The alternative has combat holding references
to three subsystems, which means stubbing all three to unit test a damage
calculation — and scatters game feel across code where it can't be tuned as a
whole.

**Generated art and audio.** Everything is synthesised at startup, so a clone
needs nothing but SFML. It's programmer art and synth sounds; a real artist beats
both. The alternative was a project that looks broken until you go find assets,
and real files still override it per file.

**Actions, not keys.** Exactly one function names a physical key. Gamepad support
came free. What's missing is the rebinding UI — `lastKeyPressed()` exists to
support one and nothing calls it.

---

## Things I got wrong

**The exit could land on the entrance.** Cave "rooms" are bounding boxes around
irregular blobs, so their centre is often a wall. My exit search skipped every
candidate whose centre wasn't floor and fell through to the default — the
entrance. The seed sweep caught it. Fixing it with a BFS distance field also made
the code match its own comment, which had claimed walking distance while
computing straight-line distance.

**`PhysicsSystem::overlaps()` shadowed the free `overlaps(a, b)`.** The accessor
hid the collision test inside the class. Caught at compile time — but only
because I actually built it instead of assuming it worked.

**My first A\* test maze was sealed.** The pathfinder correctly said "no path"
and I briefly assumed the pathfinder was broken. It was concentric rings with no
openings. A failing test is a claim about the fixture as much as the code.

**Per-frame randomness for the Skitterer looked like vibration.** Noise resampled
every frame averages to zero over any visible timescale. A wander vector that
persists 0.2–0.5s actually curves the approach.

**Bold floor-tile detail read as a repeating pattern.** My first textured floor
stamped a visible crack on 25% of tiles and the room turned into graph paper. My
fix overcorrected into a checkerboard. Third attempt: barely-there marks and a 2%
tonal spread.