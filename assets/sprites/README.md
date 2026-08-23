# Sprites

Drop a texture atlas here as `atlas.png` to replace the generated art.

You don't have to. If this file is missing I build the whole sheet in code at
startup (`game/ProceduralArt.cpp`) and the game looks the same as the
screenshots. It's programmer art though — a real artist will beat it, and this
is the file to beat it with.

## Layout

16×16 frames on a 16px grid, 8 columns wide. Row = `y / 16`, frames left to right.

| Row | Contents                                     | Frames |
|-----|----------------------------------------------|--------|
| 0   | player idle                                  | 2      |
| 1   | player walk                                  | 4      |
| 2   | player attack                                | 3      |
| 3   | player death                                 | 4      |
| 4   | shambler walk                                | 4      |
| 5   | sentinel idle                                | 2      |
| 6   | skitterer walk                               | 4      |
| 7   | enemy death                                  | 4      |
| 8   | floor ×4, stairs                             | 5      |
| 9   | wall top, wall face, wall face lit           | 3      |
| 10  | potion, sword, shield, coin, projectile      | 5      |
| 11  | coin spin                                    | 4      |
| 12  | mushroom, rock, bones, stalagmite, crystal   | 5      |
| 13  | torch ×2, merchant                           | 3      |

The four floor variants get picked per tile by a coordinate hash, so give them
some variety or the ground reads as graph paper. Keep the differences subtle —
I tried bold marks first and the repetition was worse than a plain tile.

Everything faces **down/right**. I mirror UVs at draw time for left-facing, so
don't draw separate left frames.

Different layout? All the offsets live in `DungeonGame::loadAssets()` — that's
the only place the atlas is described.

Kenney (kenney.nl, CC0) and the 0x72 Dungeon Tileset on itch.io are both good
starting points. Note whatever licence you end up using here.