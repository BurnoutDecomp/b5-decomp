# Burnout Paradise: Xbox 360 byte matching experiment

Branch: `codex/experimental-x360-byte-match`.

This experiment removes the PC port from the reconstructed source so work can
move toward byte matching `BURNOUT_X360_ARTIST.XEX`: 32-bit PowerPC, big-endian.
`main` and `dev` continue to carry the PC project.

This branch removes the desktop entry point, PC build paths and host adapters,
and selects existing Xbox branches directly. `src/pc/` stays as an unbuilt
reference; shared sources must not include it. Some PC diagnostic and movie I/O
blocks remain in shared code and still need cleanup or console reconstruction.

This is an incomplete source experiment. R173 passed the full native compile
gate and produced a strict-linked Xbox 360 XEX. R174 is a source checkpoint
awaiting a fresh full link and sound manager registration check. Launch and
whole-image byte matching remain open. Platform and middleware recovery,
original class layouts, VMX code and compiler code generation still need work.
The base commit is `209fb3b94f7266506dff4ffd569e8553f1b3af93`.

The parent workflow's desktop build and existing PC runtime tests do not apply.
Use ARTIST assembly as the authority for all future work, including Apt; DecFIGS
can corroborate declarations. Do not import Xbox One pointer widths or file layouts.

The parent `BP-Decomp_Workflow` repository retains the references, exports and
ledger. Do not publish experimental coverage to the shared production ledger.

## Native build tools

The extracted XDK 6534.4 compiler (14.00.6502) works without running setup or
changing system settings. From this repository, use its extracted `XDK` directory:

```powershell
python tools/build_x360.py --sdk "path\to\XDK" --all --jobs 8 --quiet
```

This sweeps the tracked game and reconstructed middleware sources. It emits
PowerPC objects, assembly, logs and `compile-results.json` under
`build/x360/objects/`, including every compile failure. Without `--all`, it builds
six core files; repeated `--source src/...cpp` arguments select specific files.
PC sources are rejected.

The extracted linker needs its bundled CRT declared in its manifest. Prepare a
local copy with the installed Windows SDK's `mt.exe` (auto-detected):

```powershell
python tools/prepare_x360_linker.py --sdk "path\to\XDK"
```

The prepared linker is `build/x360/tool-host/link.exe`. Original SDK files stay
unchanged. SDK files and build output stay outside Git under `build/` or in
an external artifact directory.

See the [Xbox 360 tooling guide](tools/x360/README.md) for the strict-link
history, current verification commands and archived worktree scripts.
