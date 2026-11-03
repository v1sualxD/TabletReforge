# TabletReforge

[![Build](https://github.com/v1sualxD/TabletReforge/actions/workflows/build.yml/badge.svg)](https://github.com/v1sualxD/TabletReforge/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/v1sualxD/TabletReforge)](https://github.com/v1sualxD/TabletReforge/releases/latest)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)

An English-language POEFixer plugin for the Path of Exile 2 tablet reforge cycle.

## Install

In **POEFixer > Plugin Manager > Add from GitHub**, enter:

```text
https://github.com/v1sualxD/TabletReforge
```

The [latest release](https://github.com/v1sualxD/TabletReforge/releases/latest) includes **TabletReforge.zip**, ready for the plugin manager. Restart or reload the plugin after updating.

## Use

1. Stand in your hideout with your stash and Reforging Bench accessible.
2. Under **Tablets to reforge**, select one or more categories.
3. Choose minimum uses (1–10), transfer speed (0–50), and any keep-mod filters.
4. Click **Start automatic cycle** at the top of the plugin settings.
5. Press **Esc** to stop.

The plugin opens your stash, scans the selected categories and their pages, withdraws eligible tablets, opens the bench, and reforges matching trios. It returns to stash, deposits bag contents, then resumes scanning.

**The return step Ctrl-clicks every item in the player bag, including non-tablet items.** The game determines their destination through stash routing and affinities; items that the open stash cannot accept can prevent the bag from being emptied.

## Processing rules

- Magic and Rare tablets are supported automatically.
- Each trio uses the same tablet category and rarity.
- Normal, unidentified, corrupted, Sanctified and below-threshold tablets are excluded from inputs.
- Tablets matching your keep-mod filters stay in stash.
- Unidentified results are collected and deposited; they are not queued for another reforge.
- Available categories: Irradiated, Ritual, Expedition, Delirium, Breach, Abyss, Temple and Overseer.
- World objects and inventory controls are located dynamically.
- Transfer completion is verified before the next item is selected.

The speed slider controls inventory-to-bench input transfers. Other action waits have fixed timings. Result collection waits **1.415 seconds** after the reforge action before sending its pickup click; actual elapsed time also includes input and scan confirmation.

## Debug recording

Expand **Diagnostics**, click **Start debug recording**, reproduce the problem, then click **Stop recording**.

The file is saved inside the installed plugin folder:

```text
config/tablet-reforge-live-debug.txt
```

It includes the plugin version, stages, inventory state, UI geometry and transfer differences. When item tooltips are unavailable through POEFixer, the plugin temporarily uses Ctrl+C to inspect tablet text and restores supported clipboard formats.

## Build and release

Requirements: Visual Studio 2022, MSVC v143, Windows SDK, **Release | x64**.

```powershell
MSBuild TabletReforge.sln /m /p:Configuration=Release /p:Platform=x64
```

The [single build workflow](.github/workflows/build.yml) compiles the DLL, runs the regression tests once, verifies the ZIP structure and uploads a build artifact. Successful builds on main publish a release only when the version does not already exist.

Maintainers: see [CONTRIBUTING.md](CONTRIBUTING.md) for versioning and release steps.

## Project

- [Changelog](CHANGELOG.md)
- [Releases](https://github.com/v1sualxD/TabletReforge/releases)
- [Report an issue](https://github.com/v1sualxD/TabletReforge/issues)

Derived from [TabletHelper](https://github.com/POEFixer/TabletHelper) by Omer Faruk ARPA and POEFixer, originally at commit `ac8fa16e04a6237d1df95917051cfe2d8d709828`. Original notices are retained. Distributed under [GPL-3.0](LICENSE).
