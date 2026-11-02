# TabletReforge 0.6.8

An English-only POEFixer plugin for reforging Path of Exile 2 tablets. Based on TabletHelper by Omer Faruk ARPA and POEFixer; distributed under GPL-3.0.

## Inventory reforging

1. Empty the inventory and open an empty Reforging Bench.
2. Select tablet categories and rarities, set the transfer speed (the timings now run about 1.3x faster), then click **Start reforging**.
3. Wait for the plugin to detect the ready panel; press **Esc** to stop.

Only identified, non-corrupted Magic and Rare tablets are used; Normal tablets are excluded. Set the **Minimum uses to process** slider from 1 to 10. Tablets below that uses threshold are skipped in both inventory reforging and stash withdrawal. A batch always contains three tablets of the same category and rarity. Results and leftovers are not reused in the same batch plan.

## Stash cycle

1. Open your **TABLET+FRAGMENTS** stash tab on **Tablets**. The plugin uses available inventory space and leaves unrelated items alone.
2. Select the categories and rarities to process, then click **Start stash cycle** and close the POEFixer menu.
3. The plugin scans the eight tablet categories in the confirmed left-to-right order and all six pages within each category. It withdraws only identified, non-corrupted Magic or Rare tablets that match the selected categories, rarities, minimum uses, and keep filters.
4. After the stash scan, the plugin closes the stash, locates the movable Reforging Bench by its live world entity and screen projection, opens it, and reforges compatible groups of three.
5. It closes the bench, finds and opens your own stash, returns results and leftovers, and scans again until no compatible trio remains. Stash and bench panels are switched automatically; press **Esc** to cancel. The action and pickup waits remain about 1.3x faster than before.

The bench is detected from its metadata path and current world position, so it does not need a fixed screen coordinate. The plugin only clicks a visible, targetable bench match; if it cannot identify one safely, it waits without clicking. Only tablets withdrawn during this run and their results are returned.

## Diagnostics

Open the plugin's final **Diagnostics** section and capture a report after an error, with the relevant in-game panel open. It saves `config/tablet-reforge-diagnostics.txt` in the plugin folder. The report includes controller state, inventory, visible UI, and entity metadata paths and targetability to help locate the movable hideout bench.

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
