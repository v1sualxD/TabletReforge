# TabletReforge 0.3.5

An English-only POEFixer plugin for reforging identified Path of Exile 2 tablets from the inventory.
Based on TabletHelper by Omer Faruk ARPA and POEFixer, distributed under GPL-3.0.

## Use

1. Open an **empty Reforging Bench** and keep the inventory open.
2. In the plugin menu, enable **All tablets in inventory** and optionally **Highlight tablet types**.
3. Set **Execution speed**, then click **Start reforging**.
4. Close the POEFixer menu within 30 seconds. The game must remain foregrounded. Press **Esc** to stop.

The plugin uses Ctrl + left click to move each input tablet from the inventory to the bench. It activates
**Reforge** with a normal left click. To collect a result, it uses a normal left click on the output and
places the item into the first free inventory cell if the click picks it up instead of transferring it
directly. A new inventory snapshot must confirm the result before the next trio begins. If an unexpected
state is detected, the sequence stops and reports that the bench may need manual recovery.

## Tablet rules

Only identified, non-corrupted tablets with the same visible category and rarity are grouped, three at a
time. Abyss, Expedition, Ritual and other types are never mixed. Internal metadata path variants of the
same category can be grouped together. Any one or two leftovers remain in the inventory. Reforged results
are never added to the queue, so unidentified results are not reforged.

When no compatible trio is found, the plugin writes each tablet path, base type, category, rarity,
identification state and eligibility to `Plugins/TabletReforge/config/tablet-scan.txt`.

**All tablets in inventory** is the only selection mode. The former Ctrl + Alt selection has been removed.
The optional type labels show the tablet category on inventory items.

## Bench recognition and safety

The bench must already be open. The plugin finds the visible Reforging Bench panel, the **Reforge**
button and the output slot in the POEFixer UI tree. It validates the live game window and these UI
elements before every click; it does not guess fixed screen coordinates. If it cannot establish a unique
bench and output slot, it sends no input and writes the observed UI tree to
`Plugins/TabletReforge/config/reforge-ui.txt`.

The bench must be empty before starting. POEFixer's public SDK does not expose a reliable emptiness
check, so confirm all four bench slots are empty yourself. The plugin stops on focus loss, area/window
changes, an unexpected inventory change, or a timeout. Do not move items or open chat while it runs.

## Build and install

Repository: https://github.com/v1sualxD/TabletReforge

In POEFixer, choose **Plugins > Plugin Manager > Add from GitHub**, enter the repository address, and
install or update. The latest release contains `TabletReforge.zip` with
`Plugins/TabletReforge/TabletReforge.dll`.

To build locally, install Visual Studio 2022 Desktop C++ with MSVC v143 and the Windows SDK. Open
`TabletReforge.sln`, select **Release | x64**, and build. The DLL is written to
`x64/Release/Plugins/TabletReforge/TabletReforge.dll`.

Alternatively, run **Actions > Build TabletReforge > Run workflow** on GitHub. The workflow builds the
DLL, runs the grouping and UI-discovery checks 20 times, and creates the installable ZIP.

## Attribution

Original source: https://github.com/POEFixer/TabletHelper, commit
`ac8fa16e04a6237d1df95917051cfe2d8d709828`.
Credits: Omer Faruk ARPA and POEFixer. See LICENSE and UPSTREAM-README.md.
