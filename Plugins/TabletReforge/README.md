# TabletReforge 0.6.8

An English-only POEFixer plugin for reforging Path of Exile 2 tablets. Based on TabletHelper by Omer Faruk ARPA and POEFixer; distributed under GPL-3.0.

## Inventory reforging

1. Empty the inventory and open an empty Reforging Bench.
2. Select tablet categories and rarities, set the transfer speed (the timings now run about 1.3x faster), then click **Start reforging**.
3. Wait for the plugin to detect the ready panel; press **Esc** to stop.

Only identified, non-corrupted Magic and Rare tablets are used; Normal tablets are excluded. Set the **Minimum uses to process** slider from 1 to 10. Tablets below that uses threshold are skipped in both inventory reforging and stash withdrawal. A batch always contains three tablets of the same category and rarity. Results and leftovers are not reused in the same batch plan.

## Stash cycle (new in 0.5.8)

1. Open your **TABLET+FRAGMENTS** stash tab on **Tablets**. The plugin uses available inventory space and leaves unrelated items alone.
2. Select the categories and rarities to process, then click **Start stash cycle** and close the POEFixer menu.
3. The plugin's action and pickup waits are about 1.3x shorter while click press duration stays unchanged. It scans the eight tablet categories in the confirmed left-to-right order and all six pages within each category. It withdraws only identified, non-corrupted tablets that match the selected categories and rarities. It stops when the inventory is full or the selected pages are exhausted.
4. When prompted, open the Reforging Bench, click **Continue with Reforging Bench**, then close the POEFixer menu.
5. When prompted again, open **TABLET+FRAGMENTS > Tablets**, click **Continue at Tablet stash**, then close the menu. The plugin returns results and leftovers to a page with space, then scans again until no compatible identified trio remains.

Stash and bench panels cannot stay open together, so the plugin pauses for the player to switch between them. Only tablets withdrawn during this run and their reforge results are returned. If page contents cannot be matched uniquely to the live UI, the plugin stops without clicking.

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
