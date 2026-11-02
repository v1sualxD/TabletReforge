# TabletReforge 0.6.8

An English-only POEFixer plugin for reforging Path of Exile 2 tablets. Based on TabletHelper by Omer Faruk ARPA and POEFixer; distributed under GPL-3.0.

## Inventory reforging

1. Empty the inventory and open an empty Reforging Bench.
2. Select tablet categories and rarities, set the transfer speed; action and pickup waits are now about 1.3x faster, then click **Start reforging**.
3. Wait for the plugin to detect the ready panel; press **Esc** to stop.

Only identified, non-corrupted tablets are used. A batch always contains three tablets of the same category and rarity. Results and leftovers are not reused in the same batch plan.

## Stash cycle

1. Open your **TABLET+FRAGMENTS** stash tab on **Tablets**. The plugin uses available inventory space and leaves unrelated items alone.
2. Select the categories and rarities to process, then click **Start stash cycle** and close the POEFixer menu. Transfer and pickup waits are reduced by approximately 1.3x.
3. The plugin scans the eight tablet categories and all six pages in each, withdrawing only identified, non-corrupted tablets matching the selected categories, rarities, minimum uses, and keep filters.
4. It closes the stash, detects the Reforging Bench from its live world entity and screen position, opens it, and reforges compatible groups of three.
5. It then closes the bench, finds and opens your own stash, returns results and leftovers, and repeats until no compatible trio remains. Panel switching is automatic; press **Esc** to cancel.

Bench detection uses its metadata path and current world position rather than fixed screen coordinates. If the plugin cannot find one visible, targetable match, it waits without clicking. Only tablets withdrawn during this run and their results are returned.

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
