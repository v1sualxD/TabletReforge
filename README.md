# TabletReforge 0.6.9

An English-only POEFixer plugin for reforging Path of Exile 2 tablets. Based on TabletHelper by Omer Faruk ARPA and POEFixer; distributed under GPL-3.0.

## Automatic stash-to-bench cycle

Select categories, rarities, minimum uses, and keep-mod filters, then click the single **Start automatic cycle** button. The plugin opens your own stash, selects the Tablet tab and Tablets section, scans selected categories and pages, opens the movable Reforging Bench, processes same-category/same-rarity trios, and returns results and leftovers to the stash. It repeats until no compatible trio remains. Press **Esc** to stop.

Normal, unidentified, corrupted, and below-threshold tablets are excluded. Matching keep-mod tablets stay in the stash. The transfer-speed setting is retained, with action and pickup delays about 1.3x faster.

## Stash cycle

1. Click **Start automatic cycle** and close the POEFixer menu; the plugin opens your own stash and selects the Tablet/ Tablets tab.
2. It scans selected tablet categories and all six pages in each, withdrawing eligible identified, non-corrupted Magic or Rare tablets that satisfy minimum uses and keep filters.
3. It closes the stash, detects the movable bench by its live world position, opens it, and reforges compatible groups of three.
4. It reopens the stash, returns results and leftovers, then repeats until no compatible trio remains. Press **Esc** to cancel.

The inventory needs free space for withdrawals and results. If the plugin cannot find one visible, targetable bench match, it waits without clicking.

## Controls and safety

- Category order: Irradiated, Ritual, Expedition, Delirium, Breach, Abyss, Temple, Overseer. The ninth stash icon is ignored.
- Rarity colors: Normal gray, Magic blue, Rare yellow.
- **Esc** cancels an active step. If a validation fails, check the status message and recover any items manually before restarting.
- The plugin uses live UI geometry and Ctrl + left click. It does not use fixed screen coordinates.

Diagnostics now record world entity metadata paths and targetability to help locate the movable Reforging Bench.

Repository: https://github.com/v1sualxD/TabletReforge

POEFixer installs the latest release from the repository using **Plugins > Plugin Manager > Add from GitHub**. The release ZIP must contain `Plugins/TabletReforge/TabletReforge.dll`.

## Build

Build `TabletReforge.sln` with Visual Studio 2022, MSVC v143 and the Windows SDK, using **Release | x64**. The GitHub Actions workflow compiles the DLL and packages the installable ZIP.

Original source: https://github.com/POEFixer/TabletHelper, commit `ac8fa16e04a6237d1df95917051cfe2d8d709828`.
