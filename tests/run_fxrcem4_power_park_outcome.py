"""FX-RCEM4 (crash parity 2026-09-24): PowerParkingManager::DetermineOutcome (ARTIST 0x822A74A0) --
in particular its alignment test `lfs 0x80 ; fcmpu flt_82CDB4D4 ; bge -> outcome 0`, which is TAKEN
on a NaN perpendicular distance.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_power_park_outcome.py [--pre-fix <b5 rev>]
Extracts DetermineOutcome and the KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT definition into
tests/FxRcem4PowerParkOutcome.cpp (a missing definition is replayed as 3.0 so the old body still links).
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = RCEM + "PowerParking/BrnPowerParkingManager.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    source = read(SOURCE, rev)
    found = re.search(r"^\s*f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT\s*=[^;]+;", source, re.M)
    print(("found   " if found else "MISSING ") + "KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT definition")
    pieces = {
        "fxrcem4_po_kf.inc": found.group(0).strip() if found else "f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT = 3.0f;",
        "fxrcem4_po_body.inc": definition(source, "void PowerParkingManager::DetermineOutcome("),
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4PowerParkOutcome.cpp", pieces, "fxrcem4_po",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
