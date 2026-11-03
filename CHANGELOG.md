# Changelog

## 1.1.0

### Maintenance
- Centralized plugin and debug version in PluginVersion.h.
- Extracted inventory comparison into a reusable component with regression coverage for duplicate counts, rebuilt addresses and changed item properties.
- Removed remaining unused highlight data and obsolete settings.
- Replaced historical per-version workflows with one build/test/package/release workflow.
- Updated installation, processing rules and release documentation.
- Kept runtime transfer and result timings unchanged from 1.0.3.

### Validation
- Windows Release x64 build and automated regression tests run in GitHub Actions.
- The automated tests do not validate a complete live PoE2 cycle.

## 1.0.0–1.0.3
- Simplified settings, removed rarity/type highlights and optimized stable inventory comparisons.
- Added confirmed Ctrl-click deposits for all player-bag items.
- Added recovery for transient bag geometry and input availability.
- Shortened action waits while preserving result collection confirmation.

## Earlier development
Earlier tags and releases remain available in GitHub for reference and rollback.
