# Changelog

## TabletReforge 0.7.10

- Identify occupied stash cells from the tablet's actual use-count label, ignoring empty grid-cell controls. This fixes stash-page matching when the stash is almost full.
- Record the selected category's visible occupied-cell count and POEFixer inventory match state in the live debug report.
- Write diagnostic line breaks correctly so inventory fullness and stash matching evidence is readable.


## TabletReforge 0.7.9

- Match the visible Tablet stash grid when POEFixer reports the inner cell grid slightly inset from the UI container; ambiguity still stops safely.
- Wait for stash-withdrawal scans to settle, retry the same Ctrl-click at most once only when the inventory is unchanged, and time out with an actionable status instead of waiting indefinitely.
- Keeps the 0.7.8 inventory-first trio check and delayed initial stash interaction.

## TabletReforge 0.7.8

- Check compatible tablets already in the inventory first and start reforging immediately when a matching trio is present.
- If the inventory has room but no matching trio yet, keep scanning and withdrawing from the selected Tablet stash categories; if it is full without a trio, report what must be changed instead of silently doing nothing.
- Wait before the initial stash interaction and wait for inventory scans to settle after each withdrawal, with one safe retry only when no inventory change occurred.

## TabletReforge 0.7.7

- Wait 1.7 seconds before collecting a reforging result, giving the result time to settle.
- Includes the 0.7.6 fixes for resuming multi-category stash scans and faster stash withdrawals.

## TabletReforge 0.7.6

- Resume stash scanning at the current category and page after each bench trip, preventing an abundant early category from starving later selected categories.
- Shorten fixed stash interaction waits to speed up tablet withdrawal.
- Include the selected stash category and page in live debug samples.

## TabletReforge 0.7.5

- Fixes stash withdrawal when POEFixer does not expose the hovered item tooltip in either UI tree.
- Reads the hovered item's copied text to verify its base type and leave Sanctified tablets in the stash; restores the saved clipboard formats before clicking.
- Stops safely without clicking if item text cannot be confirmed or the clipboard cannot be preserved and restored.

## TabletReforge 0.6.9

- Fixed movable-bench detection to match POEFixer's live path `Metadata/MiscellaneousObjects/Hideout/ReforgingBench`, with a live UI-label fallback.
- The single launch button now opens the own stash, selects the Tablet tab, runs the full reforge cycle, and returns results automatically.
- Checks each stash tablet's tooltip before withdrawal and leaves Sanctified tablets in the stash; stops without clicking if the tooltip cannot be read.

## TabletReforge 0.6.8

- Added a one-button stash cycle that closes and reopens the stash and reforging panels automatically.
- Locates the movable Reforging Bench by its live metadata path, targetable state, and projected on-screen position instead of fixed coordinates.
- Automatically returns results and leftovers to the stash, then resumes scanning selected categories and pages.
- Diagnostics now include each world's entity metadata path, targetability, and state to help diagnose missed bench detection.


## TabletReforge 0.6.7

- Increased tablet transfer, reforge, and result pickup pace by approximately 1.3x. The selected transfer-speed slider still applies; click pulse duration and six-second safety confirmation remain unchanged.

## TabletReforge 0.6.6

- Added a final Diagnostics section that captures controller state, stash/inventory data, visible UI controls, and world entity paths to `config/tablet-reforge-diagnostics.txt`.
- Removed the 30-second request timeout; pending steps wait until the expected game panel is available or the player presses Esc.


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
