# Renderware 4 core — `rw::` type vocabulary (generated)

These headers are the **shared `rw::` type vocabulary** for the Burnout PC decomp. They
exist so the reversing loop (and hand-written port code) reference *one* canonical definition
of each Renderware engine type instead of re-deriving struct layouts per function — which is
the main source of layout drift in a decomp.

**This is types only, by design.** Per the project strategy (see `CLAUDE.md` → "rwcore"),
Renderware is engine middleware: its *type layouts* are ingested eagerly (here), but its
*function bodies* are **not** decompiled at all. Instead, the project statically links
against the original PC binaries (`rwcore_master.obj` / `.lib`). If an agent is asked to
reverse a RenderWare function, they must skip it and block it in the ledger, allowing the
linker to resolve it from the binary. So you will find structs and enums here, but no `.cpp`.

## What's in here

```
include/
  rwcore.h                ← umbrella; #include this
  rw/rwcore_enums.h       ← rw:: enums
  rw/rwcore_structs.h     ← rw:: structs (68), topologically ordered
lib/
  rwcore.lib              ← the compiled PC binary
```

Usage:

```cpp
#include "rwcore.h"
::rw::core::debug::Channel ch;   // fully-qualified rw:: types
```

CMake: link the `renderware` (a.k.a. `rw::core`) target. This will automatically include the headers and statically link against `lib/rwcore.lib`.

## Provenance & fidelity

- **Source:** `rwcore_master.obj` (built from `rwcore.lib` + `rwcore.pdb`), via the offline
  Ghidra export at `.ghidra-exports/rwcore/`. These are **real PDB symbols** — the highest-
  fidelity type source in the project for the `rw::` namespace (better than the DWARF structs
  in DecFIGS/PS3 for these types specifically).
- **Layout is faithful to the x64 PDB.** Each struct carries `+offset` comments and a guarded
  `static_assert(sizeof == …)`. The asserts only fire under `-DRW_VERIFY_LAYOUT` on a 64-bit
  build (pointers are 8 bytes in the PDB); they are no-ops otherwise so a 32-bit PC build still
  compiles.
- **Foreign fields are exact-size opaque blobs.** Fields whose type lives in EA / eastl / std /
  the CRT are emitted as `uint8_t name[N];  // was: <original type>` to preserve layout without
  pulling in (and possibly mis-guessing) those foreign layouts. Cross-references *within* `rw::`
  are kept as real typed members.
- **Padding** runs of Ghidra `undefined` bytes are coalesced into `_padN[…]` arrays.

## Not emitted (intentionally skipped)

Template instantiations (`BaseResources<4>`, `AtomicInt<int>`…), unnamed types, and nested
classes whose enclosing type is itself a struct are skipped as named types (illegal/ambiguous
C++ identifiers) and appear as opaque blobs where referenced. The generator prints the full
skip list each run.

## Regenerating

Do **not** hand-edit these files. After re-exporting rwcore (e.g. annotations changed), run
from the repo root:

```
python tools/renderware/generate_headers.py
```

Generator: `tools/renderware/generate_headers.py`.
