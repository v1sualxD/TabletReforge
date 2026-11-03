# TabletReforge 0.7.7

An English-only POEFixer plugin for reforging Path of Exile 2 tablets. Based on TabletHelper by Omer Faruk ARPA and POEFixer; distributed under GPL-3.0.

## Automatic stash-to-bench cycle

1. Select categories, rarities, minimum uses, and any keep-mod filters, then click the single **Start automatic cycle** button. The plugin attempts to close its focused POEFixer window automatically.
2. The plugin opens your own stash, selects the Tablet tab and the Tablets section, then scans the selected categories and all six pages in each.
3. It withdraws eligible identified Magic or Rare tablets, closes the stash, detects and opens the movable Reforging Bench, and reforges groups of three from the same category and rarity.
4. It closes the bench, reopens the stash, returns results and leftovers, then repeats until no compatible trio remains. Press **Esc** to cancel.

Normal, unidentified, corrupted, Sanctified, and below-threshold tablets are excluded. Before withdrawing a candidate, the plugin checks its live tooltip. If POEFixer does not expose it in either UI tree, the plugin copies the hovered item's text with Ctrl+C, verifies the base name and Sanctified marker, then restores the clipboard contents it saved. Sanctified tablets stay in the stash. If the copied text cannot be verified or the clipboard cannot be safely preserved and restored, the plugin stops without clicking. Tablets matching selected keep mods remain in the stash. Only items withdrawn during this run and their results are returned. After each bench trip, scanning resumes at the same stash category and page, so an abundant early category cannot starve later selected categories. Stash interaction pacing has been shortened to make withdrawals faster. After reforging, the plugin waits 1.7 seconds before collecting the result.

## Diagnostics

Open the plugin's final **Diagnostics** section. **Start debug recording**, reproduce the issue in game (including switching between stash and bench), then return to POEFixer and press **Stop recording**. It saves `config/tablet-reforge-live-debug.txt` in the plugin folder, sampling every 500 ms with controller stage, foreground window, mouse position, tablet inventory geometry and nearby UI text. Send that file to help identify where the automation stops. The separate **Capture diagnostic report** button still saves a one-time snapshot to `config/tablet-reforge-diagnostics.txt`.

## Controls and safety

- Category order: Irradiated, Ritual, Expedition, Delirium, Breach, Abyss, Temple, Overseer. The ninth stash icon is ignored.
- Only Magic and Rare rarities are processed; rarity highlight colors are Magic blue and Rare yellow.
- **Esc** cancels an active step. If a validation fails, check the status message and recover any items manually before restarting.
- The plugin uses live UI geometry and Ctrl + left click. It does not use fixed screen coordinates.

Repository: https://github.com/v1sualxD/TabletReforge

POEFixer installs the latest release from the repository using **Plugins > Plugin Manager > Add from GitHub**. The release ZIP must contain `Plugins/TabletReforge/TabletReforge.dll`.

## Build

Build `TabletReforge.sln` with Visual Studio 2022, MSVC v143 and the Windows SDK, using **Release | x64**. The GitHub Actions workflow compiles the DLL and packages the installable ZIP.

Original source: https://github.com/POEFixer/TabletHelper, commit `ac8fa16e04a6237d1df95917051cfe2d8d709828`.
