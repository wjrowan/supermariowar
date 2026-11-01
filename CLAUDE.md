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

Bounds-checking build: `build/ubsan` is the debug preset plus `-fsanitize=bounds`, reusing
`build/debug`'s fetched dependency sources. It aborts with file and line on any out-of-range
index into a fixed-size array, which is how leftover 4-player arrays show up. Rebuild and run:

```bash
cmake --build build/ubsan --target smw
```

```bash
./build/ubsan/smw --datadir data
```

Settings live in `~/.local/share/supermariowar-8p/` (`options.toml`, `controls.8p.bin`),
apart from a stock SMW install's `supermariowar/`.

## Known issues

- The current AppImage needs GLIBC_2.43 (float math such as `sqrtf`/`atan2f` got new
  symbol versions in glibc 2.43; fedora:44 ships 2.43). That's fine on a Fedora 44+ Deck, but
  not on 43. `check-glibc.sh` prints the current maximum.
- On the Tumbleweed host, AppArmor's `curl` profile (from `apparmor-profiles`) also confines
  `/usr/bin/curl` inside the Fedora distrobox and blocks `/etc/pki/ca-trust/`, so curl
  over HTTPS fails there with error 77. `wget`/`python3` work. `build-appimage.sh` falls back
  to wget. The build box's `dnf` is unaffected.

## 8-player status

`MAX_PLAYERS` (8), `MAX_TEAMS` (8) and `MAX_TEAM_SIZE` (7) are in `src/common/GlobalConstants.h`.
Phase 1 (functionality test) is in. Open Phase 2 decisions: the 4 real new colours and their
art, final HUD / team select layouts, CTF flag bases for teams 5-8 (maps store 4), the other
4-team modes and tournament/tour/world screens, and remembering which pad belongs to whom.

What's placeholder or limited for now:

- Colours 5-8 are generated at load time (`src/common/gfx/TeamColors.*`, called from
  `gfxPalette::load` and `CResourceManager::extendTeamColoredSprites`): the 4 original
  variants of each sheet / palette entry are compared, pixels that differ are team coloured,
  and get the new team's base colour (hue + saturation) at the original shade's lightness.
  Pixels shared by all 4 (outlines, hammer handles, fuses) stay as they are. The base colours
  (purple, white, cyan, pink) are one table in `TeamColors.cpp`. SMW's "yellow" team is
  orange-ish, so avoid orange as a new colour. Sheets that already have 8 variants are left
  alone, so real art in the `data` submodule (needs its own fork) replaces them without code
  changes.
- With more than 4 teams, CTF, Race, King of the Hill, Jail, Eggs, Owned and Star are hidden
  (`GameModeSupportsTeams` in `src/smw/gamemodes/GameMode.cpp`), and tournaments, tours and
  worlds are refused (`MatchTypeSupportsTeams` in `src/smw/GSMenu.cpp`). They still use
  per-team data sized for 4 (`flagbaselocations[4]`, `WO_RaceGoal`, `WO_KingOfTheHillZone`,
  `Jail.cpp`, `MI_TournamentScoreboard`, `MI_World`).
- Maps only have spawn areas for teams 1-4; teams 5-8 spawn in the general areas.
- Team select, in-game scoreboard and the end-of-game ranking use placeholder layouts
  (8 narrow columns, two score rows, squeezed rows). HUD is still fixed at 640x480.
- The announcer has no clips for colours 5-8, so it stays silent for them.
- Controllers: only devices SDL knows as gamepads are used (the Keychron keyboard receiver
  shows up as a joystick). Pads aren't remembered: every launch they go to players 1, 2, 3...
  in the order found, skipping players who chose "Keyboard" in the controls menu (that choice
  is saved in `controls.8p.bin`). Players left over get the keyboard with a free default
  layout (`CGameConfig::AssignKeyboardLayouts`). Pads connected later go to the first
  fallback keyboard player. The controls menu lists the pads found at startup.

When hunting leftover 4s: not every 4 is a player count (tiles, directions, switches, swirl
arms, egg/yoshi colours, sprite frames, the autotest script's "4 == all"). Read the context.

Netplay is compiled out (`NO_NETWORK=ON`) and stays 4-player (`src/smw/net.*`,
`src/smw/network/`, `src/server/`); it only touches players 1-4 of the shared arrays.
