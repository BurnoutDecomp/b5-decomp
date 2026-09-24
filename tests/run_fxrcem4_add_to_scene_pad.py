"""FX-RCEM4 (crash parity 2026-09-24): ActiveRaceCar::AddToScene's handling-box Y pad
(ARTIST 0x822EB80C..0x822EB854) is vmaxfp128(fabs(COM.y) - dims.y, 0) + 0.05, halved -- and vmaxfp
keeps a NaN (AltiVec PEM), where the PC's `(x > 0) ? x : 0` turned it into 0.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_add_to_scene_pad.py [--pre-fix <b5 rev>]
Extracts the statements from `const f32 lfComY` through the `lfHalfDelta` initialiser VERBATIM.
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

CAR = RCEM + "BrnActiveRaceCar.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    body = definition(read(CAR, rev), "void ActiveRaceCar::AddToScene(")
    start = body.index("const f32 lfComY")
    anchor = body.index("const f32 lfHalfDelta", start)
    end = body.index(";", anchor) + 1
    block = body[start:end]
    print("found   AddToScene's pad statements (%d lines)" % (block.count("\n") + 1))
    rc = build_and_run(REPO / "tests" / "FxRcem4AddToScenePad.cpp", {"fxrcem4_ats_block.inc": block}, "fxrcem4_ats")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
