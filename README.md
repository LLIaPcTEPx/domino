# Random Console (Geode mod)

A Geometry Dash 2.2091 Geode mod that automatically adds an in-game command console to game scenes. Press **F5** to show or hide it, including during gameplay.

## Commands

- `help` — lists the available commands.
- `random (version) {coins} (stars)` — requests random online levels, filters them, and launches one directly.
  - Omit arguments for an unrestricted pick.
  - Versions use the game-version form, such as `2.2` or `1.1-2.0-2.1`; use `*` to skip the version filter.
  - Coins accept `{true}`, `{false}`, or `*`.
  - Stars accept a dash-separated list from `1` to `14`, `*`, or `0` (no star filter). Values `10`–`14` mean Easy, Medium, Hard, Insane, and Extreme Demon. For example: `random * {true} 5-6-7`.
- `randomofficial (stars)` — chooses among the built-in main levels and bonus official levels (The Challenge and the Tower worlds), then launches the selected level. Tower levels are unrated, so they appear with omitted stars or `*`.

The online randomizer searches random pages from Geometry Dash's recent-level results and picks randomly from each page's matching entries. The game API's `m_gameVersion` field is used for the version filter.

## Build

This is a Geode SDK project for Geometry Dash **2.2091**. With Geode CLI 4.0.1+ and the Geode SDK installed:

```sh
geode build -p windows
```

Or configure CMake with `GEODE_SDK` pointing at your SDK checkout and use the platform setup documented by Geode. Linux builds for Windows require Geode's cross-compilation toolchain (clang 19+, lld, Windows SDK headers).

The current Arena build environment does not have the required Clang/Windows SDK cross-toolchain, and GitHub release-asset downloads are blocked, so a packaged `.geode` binary could not be built here. The source and project files are ready to build in a configured Geode environment.
