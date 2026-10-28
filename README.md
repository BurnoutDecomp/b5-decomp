# Burnout Paradise: Xbox 360 byte matching experiment

Branch: `codex/experimental-x360-byte-match`.

This experiment removes the PC port from the reconstructed source so work can
move toward byte matching `BURNOUT_X360_ARTIST.XEX`: 32-bit PowerPC, big-endian.
`main` and `dev` continue to carry the PC project.

PC platform sources, desktop entry point, renderer hooks, input/audio/storage
adapters, FFmpeg playback, test harnesses, diagnostics, frame interpolation and
native-8 Apt conversion paths are removed from the shared code. Existing Xbox
branches are selected directly. `src/pc/` stays as an unbuilt reference; shared
sources must not include it.

This is an incomplete source experiment. It does not build or boot yet and has
not produced a byte match. The Xbox 360 toolchain/project, missing platform and
middleware implementations, original class layouts, VMX code and compiler/codegen
recovery still need work. Missing implementations are left unresolved. Mixed methods that depend on PC
infrastructure are removed as whole definitions when their console implementation
is still missing; the base commit is `209fb3b94f7266506dff4ffd569e8553f1b3af93`.

The parent workflow's desktop build and existing PC runtime tests do not apply.
Use ARTIST assembly as the authority for all future work, including Apt; DecFIGS
can corroborate declarations. Do not import Xbox One pointer widths or file layouts.

The parent `BP-Decomp_Workflow` repository retains the references, exports and
ledger. Do not publish experimental coverage to the shared production ledger.
