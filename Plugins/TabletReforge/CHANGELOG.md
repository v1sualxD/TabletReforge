# Changelog

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
