"""OWNERLIST 2026-09-27, lane L5 MENUS: the ICE camera mover's scalar sub-updates (the pause camera's look).

ICECameraMover::UpdateFrameEnd @0x8253D988 runs eight sub-updates per frame. This test covers five of them:
  - UpdateSimTime @0x8252E418: Clamp(TIME_SCALE * 0.01, 0.01, 1);
  - UpdateLens @0x8252E548: the lens clamped to its element range, 5 .. 500 mm;
  - UpdateFocus @0x8252E678: the depth-of-field band, in the console's argument order;
  - UpdateFade @0x8252E788;
  - UpdateBloom @0x8252E328: Clamp(FADE * 0.01, 0.2, 1) -> (level - 0.2) * -2, and FADE * 0.02.
The pre-eae2db92 SDK body was never linked, and it diverged from the console:
  - the time scale floored at 0, not 0.01;
  - the lens had no clamp;
  - the DOF band was passed blurriness-first;
  - the bloom level was clamped the wrong way round.
Numeric: tests/MenusIceMover.cpp compiles the revision's five bodies (and their constant block) against a stand-in take
and a recording camera. The goldens come from a numpy float32 model of the same operations.
The old bodies do not link on their own: UpdateFocus calls ICEMath::Max(f32, f32), which is declared and never
bodied. The runner supplies fsel stand-ins for Max / Min when the revision has no body for them. With those, the
revision before eae2db92 scores 9/17, and its 8 failures are the four divergences above.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_ice_mover.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

MOVER_CPP = "src/SDKs/Packages/ICE/ICECameraMover.cpp"
ICEMATH_HPP = "src/SDKs/Packages/ICE/ICEMath.hpp"
NUMERIC_CHECKS = 17
BODIES = ("void ICECameraMover::UpdateSimTime(", "void ICECameraMover::UpdateLens(", "void ICECameraMover::UpdateFocus(",
          "void ICECameraMover::UpdateFade(", "void ICECameraMover::UpdateBloom(")


def constant_block(source):
    """The TU's first anonymous namespace inside namespace ICE (the element slots, constants and read helpers)."""
    start = source.find("namespace\n{")
    if start < 0:
        start = source.find("namespace\r\n{")
    if start < 0:
        return ""
    return definition(source, source[start:source.index("{", start) + 1]) + "\n"


def float_minmax_standins(header):
    """ICEMath.hpp declares f32 Max / Min without a body, and no linked TU defines them. The pre-eae2db92 UpdateFocus
    calls ICEMath::Max(f32, f32), so that revision does not link at all. Stand-ins with the fsel semantics
    (a - b >= 0 ? a : b, a NaN takes b) let the old bodies run, so the test also shows their arithmetic."""
    out = []
    for name, cond in (("Max", "lfA - lfB >= 0.0f"), ("Min", "lfB - lfA >= 0.0f")):
        if re.search(r"f32\s+" + name + r"\s*\(\s*f32[^;{]*\)\s*\{", header):
            continue  # the revision bodies it inline
        out.append("f32 " + name + "(f32 lfA, f32 lfB) { return (" + cond + ") ? lfA : lfB; }")
    if not out:
        return ""
    return "namespace ICEMath\n{\n" + "\n".join(out) + "\n}\n"


def numeric(tree):
    source = tree.read(MOVER_CPP)
    texts = []
    for signature in BODIES:
        try:
            texts.append(definition(source, signature))
        except ValueError:
            print("NUMERIC: " + signature + " is not in this revision")
            return None
    inc = float_minmax_standins(tree.read(ICEMATH_HPP)) + constant_block(source) + "\n".join(texts) + "\n"
    return compile_and_run(Path(__file__).with_name("MenusIceMover.cpp"), "menus_ice_mover.inc", inc, "MenusIceMover",
                           shadow={ICEMATH_HPP: tree.read(ICEMATH_HPP)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_menus_ice_mover", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
