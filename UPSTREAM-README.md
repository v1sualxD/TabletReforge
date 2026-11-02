# Tablet Helper

Current version: 1.4.7

Requires POE2Fixer 364 or newer.

A POEFixer plugin for Path of Exile 2 that highlights **Precursor Tablets** in every open item
window: your inventory, every stash tab (the Tablet stash too), the guild stash and merchants.
Each tablet type gets its own colour, and you can highlight only the tablets that carry the
bonuses, roll values and uses left you are looking for.

## What you see in game

- A coloured **frame** (or a cross, or a star) on every matching tablet, in the colour of its
  type's rule.
- The **type name** in the top of the cell. All names of one window are drawn at the same size;
  a name too long for the cell is shortened to three letters (Exp, Ovs, Irr, ...).
- **×N** in the lower-right corner: the tablet's remaining uses.
- The **matched bonuses** under the type name when the cell is large enough, otherwise **✓N** in
  the lower-left corner: how many of your selected bonuses the tablet carries.

Labels never leave their tablet's cell, so neighbouring tablets never overlap. While the
POEFixer window is open the highlights are drawn behind it.

## How to use

1. Install **Tablet Helper** from the **Plugin Manager** (the first sub-tab of the Plugins tab),
   then tick **Enabled** in its tab. The Plugin Manager also installs its updates.
2. Open a stash tab or your inventory with tablets: every tablet is highlighted in its type's
   colour straight away.
3. To look for specific tablets, click **Edit** next to a type in **Tablet types**, tick the
   bonuses you want, and optionally:
   - mark must-have bonuses as **Required** and set how many required and optional bonuses a
     tablet needs (e.g. one required plus any two of five others);
   - enter **Min** / **Max** values next to a bonus to match only good rolls
     (e.g. at least 12% increased Rarity);
   - set **Minimum uses left**.
4. Use **Filter profiles** to keep several sets of rules (one per strategy) and switch between
   them.

## Settings

**Highlight**
- *Style*: Border (a frame around the cell), Cross (two diagonal lines) or Star (a star in the
  lower-left corner).
- *Line thickness*: 1 to 8 pixels.

**Labels**
- *Type name*: writes the type in the cell. *Matched bonuses* (under it) lists the selected
  bonuses the tablet carries.
- *Uses left*: the ×N badge.
- *Text colour*: white or the rule's colour (type name only).
- *Text size*: 50% to 200% of the overlay font.
- *Background* / *Dark box behind labels*: a dark box behind each label line and its opacity.
- *Text outline*: a thin black outline around label text.

**Detection**
- *Read tablet modifiers*: reads each tablet's modifiers once, when it first appears. Needed for
  uses left and every bonus filter. Off: every enabled type is highlighted by type only.
- *Refresh every*: how often the open windows are checked (50 to 1000 ms, default 150). With no
  item window open the plugin checks at most every 300 ms.

**Filter profiles**: pick, rename, add, duplicate or delete profiles (up to 32). Highlight,
label and detection options are shared by all profiles.

**Tablet types**: one row per type (Irradiated, Breach, Delirium, Expedition, Abyss, Ritual,
Overseer, Temple) plus **All types**, a rule tested on every tablet. Each row has an on/off
switch, a colour (its alpha sets the opacity) and a summary of the rule. When a tablet's own
type rule does not highlight it but the All types rule accepts it, it is highlighted in the All
types colour.

**Diagnostics** (collapsed): read timings, the modifier dump (writes the raw modifiers of the
listed tablets to `config/tablet_debug_dump.txt`) and the roll-range summary.

## Files

Settings are saved to `config/settings.json` next to the plugin, about a second after each change.
The roll ranges of the value filters are built in. After a game patch you can override single
entries with an optional `config/tablet_mod_ranges.json`:

```json
{ "mods": [ { "id": "TowerDroppedItemRarityIncrease", "min": 8, "max": 14, "unit": "percent" } ] }
```

## Known limits

- The bonus list and roll ranges follow patch 0.5.5. A bonus added by a later patch is not
  listed until the plugin is updated; use the modifier dump to report its id.
- The value fields of "seconds" bonuses compare against the value the game stores, which has not
  been checked against the tooltip for every bonus.
- Tablets are recognised by their item path (`TowerAugment`), so unique tablets count as their
  base type.
- Languages: the plugin follows the program language. A profile made with **New** or
  **Duplicate** is named in the language of that moment ("Profile 2", "Профиль 2") and keeps that
  name after a language switch, like any name you type; only the shipped "Default" profile follows
  the language. The bonus search also finds a bonus by its English name.

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `TabletHelper.zip` from [Releases](https://github.com/POEFixer/TabletHelper/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\TabletHelper\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/TabletHelper/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against
- `tools/plugin_i18n/gen.py` - the translation tool: after editing `Plugins/TabletHelper/lang/en.json` run `python tools/plugin_i18n/gen.py Plugins/TabletHelper` (it rewrites the embedded English header); `--gate` checks every language file and the keys the sources use

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `TabletHelper.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\TabletHelper\TabletHelper.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `TabletHelper.dll` into `Plugins\TabletHelper\` next to the POE2Fixer executable, together with `Plugins\TabletHelper\lang` from this repository (same folder names).

## Credits

Originally created by Omer Faruk ARPA (https://github.com/omrfarukarpa/poefixer-tablet-helper).
Maintained by POEFixer since October 2026.

Licensed under GPL-3.0, see LICENSE.
