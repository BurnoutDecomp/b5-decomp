"""FX-CRASHVFX (crash parity 2026-09-25, C3 = CC-15): A STRIDE-0 FAST-SET STREAM IS DRAWN AT THE SHADER'S STRIDE.

renderengine::MeshHelper::Dispatch @0x8227B530 -- the debris meshes' bind -- binds stream 0 with stride 0
(`li r7, 0`). On the console that is not a cleared binding: D3DDevice_SetStreamSource @0x8293D688 stores stride >> 2
and raises the vfetch-patch dirty bit only when that is nonzero, so the stride baked into the vertex shader's vfetch
stays in force. The PC fast-set draw paths skipped such a stream ("[WorldDraw] draw skipped: no device/geometry/
declaration stash"), so no debris mesh could reach D3D. The FLAG PC platform leaf ResolveFastSetStrideFromShader now
draws it at the bound declaration's stream-0 extent. The [stride0] witness (BRN_STRIDE0_DIAG) showed, on the pre-change
exe, that no other binder reaches a draw with a stride-0 stream (sky, Lion, sparks and the simple particles bind 0 and
then re-bind a real stride): tests/FxCrashVfxStride0Live.ps1.

  1. WIRING -- WorldDraw_SetVertexSourceRaw remembers a stride-0 bind, the DISPATCH publisher forgets it, and both
     fast-set draw paths resolve the stride before their early-outs; the witness is default-off and counts at the
     bind (per binder) and at both draw entries; the comment no longer calls stride 0 a cleared binding.
  2. NUMERIC -- tests/FxCrashVfxStride0.cpp runs the PRODUCTION DeclTypeBytes / DeclarationStream0Extent on the
     declarations the path meets (6 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_stride0.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

SHIMS = "src/pc/gcm/renderengine/XenonD3D9Shims.cpp"
HELPERS = ("u32 DeclTypeBytes(", "u32 DeclarationStream0Extent(")

NUMERIC_CHECKS = 6


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
        try:
            return super().read(relative).replace("\r\n", "\n")
        except FileNotFoundError:
            return ""


def body(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def wiring(tree):
    source = tree.read(SHIMS)
    raw = code_only(body(source, "void WorldDraw_SetVertexSourceRaw("))
    dispatch = code_only(body(source, "void WorldDraw_SetVertexSource("))
    yield ("WorldDraw_SetVertexSourceRaw remembers a stride-0 bind (sbFastSetStrideFromShader) and the DISPATCH "
           "publisher forgets it",
           "sbFastSetStrideFromShader = (luStride == 0);" in raw and "sbFastSetStrideFromShader = false;" in dispatch)
    ok = True
    for signature, early in (("void WorldDraw_IndexedUP(", 'LogOnce("updraw"'),
                             ("void WorldDraw_NonIndexedUP(", 'LogOnce("updrawnv"')):
        text = code_only(body(source, signature))
        witness = text.find("Stride0Diag_AtDraw(")
        resolve = text.find("ResolveFastSetStrideFromShader(lpDevice);")
        ok = ok and 0 <= witness < resolve < text.find(early)
    yield ("both fast-set draw paths count the arrival (Stride0Diag_AtDraw) and resolve a stride-0 stream's stride "
           "(ResolveFastSetStrideFromShader) before their early-outs", ok)
    resolve_fn = code_only(body(source, "void ResolveFastSetStrideFromShader("))
    yield ("ResolveFastSetStrideFromShader reads the declaration bound at the draw and takes its stream-0 extent",
           "GetVertexDeclaration(" in resolve_fn and "GetDeclaration(" in resolve_fn
           and "DeclarationStream0Extent(" in resolve_fn and "sbFastSetStrideFromShader" in resolve_fn)
    stream = code_only(body(source, "void D3DDevice_SetStreamSource("))
    armed = code_only(body(source, "bool Stride0DiagArmed("))
    yield ("the [stride0] witness is default-off (BRN_STRIDE0_DIAG) and notes every stream-0 bind with its caller",
           '"BRN_STRIDE0_DIAG"' in armed and "Stride0Diag_NoteBind(lpStreamData, luStride, _ReturnAddress())" in stream)
    yield ("the stash comment no longer calls stride 0 the console's cleared binding",
           "the console's \"clear the previous binding\" call" not in source)


def numeric(tree):
    source = tree.read(SHIMS)
    texts = []
    for signature in HELPERS:
        try:
            texts.append(definition(source, signature))
        except ValueError:
            print("NUMERIC: cannot build -- no " + signature)
            return None
    return compile_and_run(Path(__file__).with_name("FxCrashVfxStride0.cpp"), "fxcrashvfx_stride0_body.inc",
                           "\n".join(texts) + "\n", "FxCrashVfxStride0")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_stride0", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
