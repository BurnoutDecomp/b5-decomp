"""FX-GATE (crash parity 2026-09-25, REVIEW-J): RaceCarPhysics::UpdateTargetAssist's argmin accepts a NaN weight.

X360 0x8261FF50, the candidate loop's best-weight test:
    0x82620104  fcmpu cr6, f0 (weight), f31 (best)
    0x82620108  bgt   0x82620114          ; T: skip the candidate
    0x8262010C  fmr   f31, f0 ; mr r26, r11   ; F: it is the new best
`bgt` is taken only on an ORDERED greater, so an unordered compare (a NaN weight, or a NaN best left by an
earlier one) falls through and the candidate is TAKEN. The C++ form is `!(lfWeight > lfBestWeight)`; the old
`lfWeight <= lfBestWeight` rejected it. (The 0.766 dot gate before it, `ble` 0x826200E4, already drops a NaN dot.)

The test EXTRACTS the condition of the `if` that guards `lfBestWeight = lfWeight;` from the production body and
checks it against the branch, value by value and over whole candidate loops.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_target_assist_argmin.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, definition, report

SOURCE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp"
SIGNATURE = "void RaceCarPhysics::UpdateTargetAssist("
NUMERIC_CHECKS = 22


def condition(tree):
    body = definition(tree.read(SOURCE).replace("\r\n", "\n"), SIGNATURE)
    match = re.search(r"if\s*\((?P<cond>[^;{}]*?)\)\s*\{\s*lfBestWeight\s*=\s*lfWeight\s*;", body)
    return match.group("cond").strip() if match else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    cond = condition(tree)
    wiring = [("UpdateTargetAssist's argmin guard is `!(lfWeight > lfBestWeight)` (bgt 0x82620108)",
               cond is not None and re.sub(r"\s+", "", cond) == "!(lfWeight>lfBestWeight)")]
    numeric = None
    if cond is None:
        print("NUMERIC: cannot build -- the argmin guard was not found")
    else:
        inc = ("static bool Accept(float lfWeight, float lfBestWeight)\n{\n    return (" + cond + ");\n}\n")
        numeric = compile_and_run(Path(__file__).with_name("FxGateTargetAssistArgmin.cpp"),
                                  "fxgate_target_assist_argmin.inc", inc, "FxGateTargetAssistArgmin")
    return report("run_fxgate_target_assist_argmin", wiring, numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
