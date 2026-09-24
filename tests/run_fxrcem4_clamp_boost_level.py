"""FX-RCEM4 (crash parity 2026-09-24, FX-FPUMAX leftover): CrashPlayManager::ClampBoostLevel is the
console's Clamp(x, 0, 100) = Min(100, Max(0, x)) -- the fsel pair inlined at all six sites (OnBounce
0x822A7F78..0x822A7F88, UpdateMomentum 0x82302268..0x82302278 / 0x823022D8..0x823022EC,
OnVehicleHitConfirmed 0x822C33B8..0x822C33C8 / 0x822C3454..0x822C3464, OnHitOverheadSign
0x822A805C..0x822A8078). A NaN meter comes out 100 and -0 comes out +0.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_clamp_boost_level.py [--pre-fix <b5 rev>]
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

CRASHPLAY = RCEM + "CrashPlay/BrnCrashPlayManager.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    body = definition(read(CRASHPLAY, rev), "void CrashPlayManager::ClampBoostLevel()")
    print("found   CrashPlayManager::ClampBoostLevel")
    rc = build_and_run(REPO / "tests" / "FxRcem4ClampBoostLevel.cpp", {"fxrcem4_cbl_body.inc": body}, "fxrcem4_cbl")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
