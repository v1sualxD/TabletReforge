# Contributing

## Scope

Keep changes focused and preserve the inventory checks around transfers. A smaller delay alone does not establish better throughput; measure a complete withdrawal, reforge and deposit cycle.

## Validation

Build Release x64 and run ReforgePlanTests, SelectionTests and InventoryContentsTests. For changes to UI discovery or input timing, also verify a live cycle and retain the debug recording. Unit tests cannot validate host UI availability or game input delivery.

## Versioning

The single version source is `Plugins/TabletReforge/src/PluginVersion.h`. POEFixer requires X.Y.Z with one digit each for Y and Z. After 1.0.9, use 1.1.0.

For a release:
1. Update PluginVersion.h, CHANGELOG.md and docs/RELEASE_NOTES.md.
2. Commit the complete change together.
3. Let the main build finish successfully.
4. Confirm the release contains TabletReforge.zip.

The workflow publishes only a missing version; it does not replace existing release assets. Use a new version for changed binaries.

## Packaging

The ZIP root must contain `Plugins/TabletReforge/TabletReforge.dll`. Do not wrap TabletReforge.zip inside another ZIP. The package also includes LICENSE, README.md and CHANGELOG.md.

## Commits

Use concise messages describing the final change. Group related source, documentation and release changes in one commit when practical. Preserve published history and tags.
