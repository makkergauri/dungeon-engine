# Audio

Drop files here to override the generated sounds. None of them are required — if
a file is missing I synthesise that sound at startup (`game/ProceduralAudio.cpp`).

| File          | Plays when          |
|---------------|---------------------|
| `swing.wav`   | you attack          |
| `hit.wav`     | you hit an enemy    |
| `hurt.wav`    | you take damage     |
| `death.wav`   | an enemy dies       |
| `pickup.wav`  | you grab an item    |
| `stairs.wav`  | you descend a floor |
| `ambient.ogg` | background loop     |

I made the override per file, so you can swap in one good sword sound and leave
the rest generated.

WAV for effects, OGG for music. SFML loads a `SoundBuffer` fully into memory but
streams `Music`, so minutes of WAV music would just sit there uncompressed.

Don't bother recording variations of the same hit — `AudioManager::playSound`
shifts the pitch ±8% every time it fires.