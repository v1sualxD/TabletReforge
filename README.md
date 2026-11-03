# TabletReforge 0.7.6

An English-only POEFixer plugin for reforging Path of Exile 2 tablets. Based on TabletHelper by Omer Faruk ARPA and POEFixer; distributed under GPL-3.0.

## Automatic stash-to-bench cycle

Select categories, rarities, minimum uses, and keep-mod filters, then click the single **Start automatic cycle** button. The plugin opens your own stash, selects the Tablet tab and Tablets section, scans selected categories and pages, opens the movable Reforging Bench, processes same-category/same-rarity trios, and returns results and leftovers to the stash. After each bench trip, it resumes at the same category and page instead of restarting from the first category, so later selected categories are reached even when the first has many tablets. Stash interaction pacing is faster. It repeats until no compatible trio remains. After start, the plugin attempts to close its focused POEFixer window with Esc. World interactions pause between attempts and retry until the stash or bench opens. Press **Esc** to stop.

Normal, unidentified, corrupted, Sanctified, and below-threshold tablets are excluded. The plugin checks each candidate's live tooltip before withdrawal. If POEFixer does not expose it, the plugin reads the hovered item text with Ctrl+C, verifies the tablet base and Sanctified marker, then restores the clipboard contents it saved. It stops without clicking if that check or clipboard restoration fails. Matching keep-mod tablets stay in the stash. The transfer-speed setting is retained, with action and pickup delays about 1.3x faster.

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
