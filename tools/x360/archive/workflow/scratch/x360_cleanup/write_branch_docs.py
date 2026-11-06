from pathlib import Path
import strip_host as s

text = """# Burnout Paradise: Xbox 360 byte matching experiment

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
recovery still need work. Missing implementations are left unresolved.

The parent workflow's desktop build and existing PC runtime tests do not apply.
Use ARTIST assembly as the authority for all future work, including Apt; DecFIGS
can corroborate declarations. Do not import Xbox One pointer widths or file layouts.

The parent `BP-Decomp_Workflow` repository retains the references, exports and
ledger. Do not publish experimental coverage to the shared production ledger.
"""
(s.ROOT / "README.md").write_bytes(text.replace("\n", "\r\n").encode("utf-8"))

for name in ["cmake/Gamepad.cmake", "config.ini.example"]:
    (s.ROOT / name).unlink(missing_ok=True)

# Only the reconstructed Xenon hardware path belongs in the selected target.
name = "src/GameShared/GameClasses/System/CMakeLists.txt"
p = s.ROOT / name
text = p.read_text(encoding="utf-8-sig").replace("if (WIN32)\nendif()\n", "")
text = text.replace("    PRIVATE\n", "    PRIVATE\n        PS3/CgsHardwareInitPS3.cpp\n        PS3/CgsHardwareLanguagePS3.cpp\n        PS3/CgsHardwareSkuPS3.cpp\n        X360/CgsXOverlappedX360.cpp\n", 1)
p.write_bytes(text.replace("\n", "\r\n").encode("utf-8"))

print("Wrote experimental branch README and native-only operating instructions")
