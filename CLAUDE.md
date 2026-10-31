# Super Mario War 8P

Fork of [mmatyas/supermariowar](https://github.com/mmatyas/supermariowar) (C++20, CMake, SDL3)
that raises the player cap from 4 to 8 and ships as an AppImage for a Steam Deck running
Bazzite (installed with Gear Lever, launched from Steam Gaming Mode).

- Fork: `origin` = github.com/wjrowan/supermariowar, `upstream` = mmatyas/supermariowar (master)
- Work branch: `eight-player`
- Game data is the `data/` git submodule (supermariowar-data). After cloning: `git submodule update --init`

## Working with the user

- When showing terminal commands, put ONE command per code block. Never chain several
  commands in one block.
- Never run host `sudo` yourself: hand the command over and wait for "done".
  Inside the `smw-appimage` distrobox, sudo is passwordless and fine to use.
- Ask before patching upstream game code that isn't part of the 8-player work.

## Dev / build split

| Where | What | Why |
|---|---|---|
| Tumbleweed host | edit, debug build, tests, gdb, VS Code | fast native loop with system SDL3 |
| distrobox `smw-appimage` (`fedora:44`, the Deck's Bazzite base) | release build + AppImage | Tumbleweed's glibc/libstdc++ are newer than Bazzite's; a binary built on the host would need symbol versions the Deck doesn't have |

Always build on the Deck's Fedora version or older, never newer: a fedora:44 build runs on
44 and 45, but fedora:45 has glibc 2.44 (same as Tumbleweed) and wouldn't run on a 44 Deck.
If Bazzite moves to a new Fedora, the box can stay on 44 until there's a reason to move.

The box needs, on top of the original list, `libXtst-devel` (SDL 3.4 requires XTest),
`file` (appimagetool calls it), and `strace` (only used for debugging).

The release preset builds SDL3, SDL3_image and SDL3_mixer from source
(`FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER`) and uses rpath `$ORIGIN`, so the AppImage
bundles them next to the `smw` binary. Both presets set `NO_NETWORK=ON`.

`build/debug` belongs to the host, and `build/release` + `build/AppDir` belong to the box.
Don't configure the release preset on the host, or the box build will reuse a cache made
with the wrong toolchain (delete `build/release` if that happens).

## Commands

All of these run from the repo root.

Configure (debug):

```bash
cmake --preset debug
```

Build (debug):

```bash
cmake --build --preset debug
```

Test:

```bash
ctest --preset debug
```

Run:

```bash
./build/debug/smw --datadir data
```

Build the AppImage (in the box; also the VS Code task "Build AppImage for Steam Deck"):

```bash
distrobox enter smw-appimage -- ./packaging/appimage/build-appimage.sh
```

Output: `dist/SuperMarioWar-8P-x86_64.AppImage`. Check what it needs from glibc (must be
no newer than the Deck's Fedora glibc):

```bash
./packaging/appimage/check-glibc.sh dist/SuperMarioWar-8P-x86_64.AppImage
```

VS Code: CMake Tools picks up `CMakePresets.json`; the launch config "Debug smw (gdb)" builds
`smw` with the debug preset and runs it under gdb with `--datadir data`.

## Known issues

- The current AppImage needs GLIBC_2.43 (float math such as `sqrtf`/`atan2f` got new
  symbol versions in glibc 2.43; fedora:44 ships 2.43). That's fine on a Fedora 44+ Deck, but
  not on 43. `check-glibc.sh` prints the current maximum.
- On the Tumbleweed host, AppArmor's `curl` profile (from `apparmor-profiles`) also confines
  `/usr/bin/curl` inside the Fedora distrobox and blocks `/etc/pki/ca-trust/`, so curl
  over HTTPS fails there with error 77. `wget`/`python3` work. `build-appimage.sh` falls back
  to wget. The build box's `dnf` is unaffected.

- On the host's NVIDIA driver the game segfaults on exit: the global `GraphicsSDL gfx`
  destructor (src/common/gfx/gfxSDL.cpp) frees SDL/GL objects from an `exit()` handler,
  after NVIDIA's libGL has already torn itself down. Clean on Mesa (Intel iGPU, Deck).
  Upstream bug, not yet patched.

## 8-player hotspots

`MAX_PLAYERS` is in `src/common/GlobalConstants.h`. Much of the code ignores it and
hard-codes 4, so after bumping it, look for:

- `< 4` loops and `[4]` arrays. Not every 4 is a player count: tiles, directions
  (left/right/up/down), and some sprite frames also use 4. Read the context before changing one.
- `src/common/GameValues.h` / `GameValues.cpp`: per-player arrays (`inputConfiguration[4][2]`,
  `playercontrol`, `skinids`, `colorids`, `teamids[4][3]`, `teamcounts`, `storedpowerups`,
  `worldpowerups`, `tournament_scores`, bullet bill timers, secret-unlock arrays…), and
  `controlkeys[2][2][4][NUM_KEYS]` (default keys per player; also `extern`'d in
  `src/worldeditor/worldeditor.cpp`).
- `src/common/ConfigNames.h`: config/TOML arrays (`skins`, `player_slots`, …) sized by
  `MAX_PLAYERS`, plus serialisation that may assume 4 entries.
- `src/smw/GSGameplay.cpp`, `src/smw/GSMenu.cpp`, `src/smw/menu/ModeOptionsMenu.cpp`,
  `src/common/map.cpp`.
- UI: `src/smw/ui/MI_TeamSelect.*`, `MI_World.*`, `MI_InputControlContainer.*`,
  `MI_TournamentScoreboard.*`.
- Player colour palettes in the gfx data: `data/gfx/packs/*/palette.png` is 25x37
  (1 key row + 4 teams x 9 `PlayerPalette` states). `gfxPalette`'s `ColorSheet` is sized
  `MAX_PLAYERS * PlayerPalette::COUNT`, so 8 players needs 4 more colour sets (73 rows) in
  every gfx pack. Changes go in the `data` submodule, so they need their own commit and fork there.
- HUD/menu layout is fixed at 640x480. Scoreboards, team select and the input menu lay out
  4 slots; 8 won't fit without a new layout.
- Netplay: compiled out via `NO_NETWORK=ON`. Its protocol (`src/smw/net.cpp`,
  `src/smw/network/ProtocolGamePackages.h`, `src/server/`) is 4-player and out of scope for now.
