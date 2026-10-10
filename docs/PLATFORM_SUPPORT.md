# Platform support organization

Recovered game and middleware identities retain their original names and paths.
Handwritten host support belongs under `src/pc/<subsystem>/`, or the subsystem's
existing `PC` directory. Use descriptive module filenames without `PCLeaf`.
Directories convey the target; C++ namespaces convey type ownership. New support
types should use ordinary names in their owning namespace. Keep existing
`// FLAG PC-platform leaf: <reason>` provenance comments and binary evidence.

Reflection capture helpers live in `src/pc/gcm/renderengine/reflections/` and
debug configuration support in `src/pc/debug/`. Other renderer, input, and
geometry support remains in the existing `pc/gcm/renderengine`, `pc/input`, and
`pc/geometric` directories.

Issue #38 renames all 48 former `*PCLeaf.h`/`*PCLeaf.cpp` support files and
updates includes, regression fixtures/runners, documentation, workflow shipping
mounts, and path-keyed audit records. Recovered APIs and runtime policy remain
unchanged. Audit findings are migrated by path, not regenerated or discarded.
The complete old-to-new inventory is [PLATFORM_SUPPORT_RENAMES.json](PLATFORM_SUPPORT_RENAMES.json).

Validation (2026-10-10): shipping MSVC build, 2,848 TUs, zero compile failures,
successful link; all 50 affected regression runners pass. Stale standalone
fixtures were updated for the current shader/configuration dependencies, real
untextured renderer instantiation, Jobs allocator ownership, and legacy geometry
page-map registration. Production behavior is unchanged by these fixture repairs.

Future platforms can use `src/<platform>/<subsystem>/` with explicit target
source selection. Reference-attributed platform paths describe source provenance;
they do not establish current target support. A port requires its own toolchain,
ABI, graphics, input/audio/files, assets, packaging, and runtime verification.
