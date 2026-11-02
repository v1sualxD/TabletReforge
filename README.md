# TabletReforge 0.2.0

An English-only POEFixer plugin for selecting and reforging inventory tablets.
Based on TabletHelper by Omer Faruk ARPA and POEFixer, distributed under GPL-3.0.

## Controls

- Hover an identified tablet in your inventory and hold **Ctrl + Alt** to toggle its selection.
- Keep both keys held and move across tablets to select several. Each tablet toggles once per key hold.
- Selected tablets have a green outline. Release the keys and press them again over a tablet to deselect it.
- Alternatively, tick **All tablets in inventory**.
- Open an **empty Reforging Bench**, open the plugin menu, and press **Start reforging**.
- Close the POEFixer menu within 30 seconds to begin. **Esc** stops the sequence.

There are no category lists, mod filters, filter profiles, calibration controls, or auto-reforge enable switch.
All plugin labels, status messages and instructions are in English, regardless of the POEFixer language.

## Batch rules

Only compatible groups of three are used: Abyss + Abyss + Abyss, Expedition + Expedition + Expedition,
Ritual + Ritual + Ritual, and so on. The exact metadata base and rarity must match across all three items.
Categories and rarities are never mixed. Groups smaller than three remain in inventory.

Only identified, non-corrupted, non-unique tablets are eligible in either selection mode.
The queue is frozen at the start. Results are returned to inventory and are never added to that queue.
Unidentified tablets are excluded even on later runs.

## Automatic bench discovery

The bench must already be open; this version does not walk to or open an NPC or bench object.
There is no manual calibration. The plugin searches the visible UI through the public POEFixer SDK for
an unambiguous reforging panel, Reforge button and result/output slot. It uses semantic control names
and English labels, never guessed screen coordinates or private child offsets.

**This discovery heuristic has not been validated against a live PoE2 UI.** A successful build and
synthetic discovery tests do not establish compatibility with the game's current control names.
If the controls cannot be recognized, the plugin sends no clicks and writes the observed UI names
to `Plugins/TabletReforge/config/reforge-ui.txt`. This file can be used to add verified control names.

The bench's three inputs and result slot must be empty before starting. The SDK does not provide a
certified empty-bench check. Transfers use Ctrl + left click; a plain click activates Reforge; the result
is collected with Ctrl + left click. Each inventory change must match a fresh snapshot before continuing.

## Installation and build

Repository: https://github.com/v1sualxD/TabletReforge

In POEFixer: **Plugins > Plugin Manager > Add from GitHub**, paste the repository address, then install
or update. A release must include `TabletReforge.zip` containing `Plugins/TabletReforge/TabletReforge.dll`.

To build locally, install Visual Studio 2022 Desktop C++ with MSVC v143 and the Windows SDK.
Open `TabletReforge.sln`, select **Release | x64** and build.
The DLL is written to `x64/Release/Plugins/TabletReforge/TabletReforge.dll`.

Alternatively, run **Actions > Build TabletReforge > Run workflow** on GitHub. The workflow compiles
the DLL, runs grouping/selection/discovery tests, and produces the installable archive.

The previous version's category and filter settings are ignored. Only the all-inventory checkbox is
saved. Manual selections stay in memory and are cleared when the game window or area changes.

**In-game behavior remains experimental. Start with three low-value tablets.** Do not move items,
hold modifier keys or open chat during a running sequence. After an interruption, recover any tablets
left in the bench before starting again.

## Attribution

Original source: https://github.com/POEFixer/TabletHelper, commit
`ac8fa16e04a6237d1df95917051cfe2d8d709828`.
Credits: Omer Faruk ARPA and POEFixer. See LICENSE and UPSTREAM-README.md.
