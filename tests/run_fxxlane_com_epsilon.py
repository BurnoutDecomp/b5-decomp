"""Regression for StreamedDeformationSpec::TransformToNewCOMSpace @0x825E3148 (crash parity G39-D1, FX-XLANE).

The COM re-frame gate compares |oldCOM - newCOM|^2 against the .rdata constant stru_8208F620 lane 0
(0x825E3164 lvlx + 0x825E3168 vspltw; x360rd 0x34000000 = FLT_EPSILON) with a strict vcmpgtfp
(0x825E3198) and returns on false (0x825E31AC beqlr). The PC used an invented 1.0e-6f placeholder.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxxlane_com_epsilon.py [--pre-fix <b5 rev>]
The placeholder body (b5 451d2363) fails 4/9 checks; the FLT_EPSILON gate passes 9/9.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import PHYS, REPO, anonymous_namespace, build_and_run, definition, pre_fix_rev, read

TU = PHYS + "/BrnStreamedDeformationSpec.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    text = read(TU, rev)
    pieces = {
        "anon.inc": anonymous_namespace(text),
        "methods.inc": definition(text, "    void StreamedDeformationSpec::TransformToNewCOMSpace("),
    }
    rc = build_and_run(Path(__file__).with_name("FxXlaneComEpsilon.cpp"), pieces, "fxxlane_com_epsilon",
                       extra_sources=[REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"])
    sys.exit(rc)


if __name__ == "__main__":
    main()
