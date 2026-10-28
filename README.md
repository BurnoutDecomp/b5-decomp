# Burnout Paradise: Xbox 360 byte matching experiment

Branch: `codex/experimental-x360-byte-match`.

This experiment removes the PC port from the reconstructed source so work can
move toward byte matching `BURNOUT_X360_ARTIST.XEX`: 32-bit PowerPC, big-endian.
`main` and `dev` continue to carry the PC project.

This branch removes the desktop entry point, PC build paths and host adapters,
and selects existing Xbox branches directly. `src/pc/` stays as an unbuilt
reference; shared sources must not include it. Some PC diagnostic and movie I/O
blocks remain in shared code and still need cleanup or console reconstruction.

This is an incomplete source experiment. The complete game does not build or boot
yet and has not produced a byte match. The executable build, missing platform and
middleware implementations, original class layouts, VMX code and compiler/codegen
recovery still need work. Missing implementations are left unresolved. Mixed methods that depend on PC
infrastructure are removed as whole definitions when their console implementation
is still missing; the base commit is `209fb3b94f7266506dff4ffd569e8553f1b3af93`.

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
unchanged; neither command installs software or changes system settings. Native
linking works for the CRC objects. The game still has source compatibility,
console layout and missing implementation work before it can link or boot.
SDK files and build output stay outside Git under `build/`.
