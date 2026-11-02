# Changelog

## TabletReforge 0.6.5

- Removed Normal rarity from reforging and stash withdrawals; only Magic and Rare tablets can be processed.
- Added a shared minimum-uses threshold from 1 to 10 for both stash withdrawal and inventory reforging.
- Reorganized the settings panel into clearer processing, category, rarity, keep-filter, action and status sections.

## TabletReforge 0.6.4

- Fixed active Tablet stash page matching after category/page navigation using POEFixer grid geometry rather than a stale occupied-slot snapshot.
- Renamed a matcher helper to avoid a Windows compiler keyword conflict.

## TabletReforge 0.6.3

- Attempted the active-grid stash matcher; its Windows build issue was corrected in 0.6.4.

## TabletReforge 0.6.3

- Fixed stash-page matching to follow POEFixer's active tablet grid after switching categories or pages, instead of comparing against stale UI cells captured earlier.

## TabletReforge 0.5.8

- Added a stash cycle that scans the eight Tablet stash categories and their six pages, withdraws selected identified tablets, pauses for manual switching to the Reforging Bench, then pauses again to return results and leftovers to the stash.
- Added live stash grid discovery and exact occupied-slot matching. Ambiguous or missing page matches stop safely without clicking.
- Stash cycling requires an empty inventory and uses only Ctrl + left click for item transfers.

## TabletReforge 0.5.3

- Fixed all tablets being treated as protected when modifier filters are removed. Eligible tablets of selected rarities are now grouped into same-category, same-rarity trios.

## TabletReforge 0.5.2

- Removed tablet type and modifier filters and deleted saved filter profiles. Rarity choices remain; all tablet categories are considered and compatible trios still require the same category and rarity.

## TabletReforge 0.5.1

- Matched the tablet type list to TabletHelper's layout with category swatches, filter summaries and per-type Edit buttons. Inventory highlight colours remain rarity-based.

## TabletReforge 0.5.0

- Added saved filter profiles with selection, creation, duplication, renaming and deletion. Each profile stores its tablet type settings, rarity selection and per-type modifier filters.
- Added editable tablet modifier filters and roll ranges, plus rarity selection and rarity-coloured highlights.
- All tablet categories start disabled until selected.

## 1.4.7

- Built with the current plugin SDK: a controller that is not XInput reports no port (-1) even when ready.

## 1.4.6

- Follows the program language (ten languages).
- Tablet names and bonus lines read as the game words them in each language, and the bonus search and the bonus order follow the translated names.
- The bonus search also finds a bonus by its English name, in any program language.

## 1.4.5

- Works with the separate menu window (POEFixer 366): the host hands the plugin the right drawing context for its settings tab and for its overlay.

## 1.4.4

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.4.3

- The "?" next to a section title now sits in its own gap instead of on the section's line.
- Diagnostics now looks the same as in the other POEFixer plugins.
- The bottom of the settings tab names the plugin's original author, as in the other plugins maintained by POEFixer.

## 1.4.2

- Now maintained by POEFixer: new settings tab in the POEFixer style (cards, clear labels, a
  tooltip on every control, a live "on screen / highlighted" status).
- Tablets in your inventory are highlighted again (the backpack grid was skipped).
- No more stutter with large stashes: tablets are found and read in the background, and the
  highlights follow tab switches sooner.
- Labels stay inside their tablet's cell at one even size; uses left is a ×N corner badge, and
  matched bonuses show as names or a ✓N count when the cell is too small.
- Highlights draw behind the POEFixer window while it is open instead of over it.
- With "Read tablet modifiers" off, every enabled type is highlighted by type, as described
  (bonus and uses-left filters no longer hide everything).
- Settings save about a second after each change and can no longer be lost to a half-written
  file; a damaged file is kept as settings.json.corrupt. After an area change, tablets are
  re-read instead of reusing an old reading.
- The duplicate "Enable Tablet Helper" and "Show overlay" switches were removed: use the
  plugin's Enabled switch.

## 1.4.1 and earlier

Released by Omer Faruk ARPA at https://github.com/omrfarukarpa/poefixer-tablet-helper.
