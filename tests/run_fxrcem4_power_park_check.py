"""FX-RCEM4 (crash parity 2026-09-24): BrnWorld::CheckVehicleForPowerPark (ARTIST 0x822B1FA0, the
header inline both ends of the Power Parking chain call) and its callee BrnMath::
GetPointToInfiniteLineDistance (0x82540448), plus the flt_82CDB4D4 alignment constant.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_power_park_check.py [--pre-fix <b5 rev>]
Extracts the pieces into tests/FxRcem4PowerParkCheck.cpp. A piece the source lacks (or only declares)
is replayed as a stub that does nothing.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

HEADER = RCEM + "PowerParking/BrnPowerParkingManager.h"
SOURCE = RCEM + "PowerParking/BrnPowerParkingManager.cpp"


def bodied(source, signature):
    """The brace-balanced definition starting at `signature` when it is DEFINED there (the next
    code token after the parameter list is `{`), else ''."""
    start = source.find(signature)
    while start >= 0:
        close = source.find(")", start)
        tail = re.sub(r"//[^\n]*", "", source[close + 1:close + 400]) if close >= 0 else ""
        if tail.lstrip().startswith("{"):
            return definition(source[start:], signature)
        start = source.find(signature, start + 1)
    return ""


def line(source, pattern):
    found = re.search(pattern, source, re.M)
    return found.group(0) if found else ""


def report(name, text):
    print(("found   " if text else "MISSING ") + name)
    return text


def main():
    rev = pre_fix_rev(sys.argv)
    header, source = read(HEADER, rev), read(SOURCE, rev)

    radius = report("KF_POWER_PARK_NEARBY_RADIUS",
                    line(header, r"^\s*const f32 KF_POWER_PARK_NEARBY_RADIUS\s*=\s*[^;]+;"))
    aligned = report("KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT definition",
                     line(source, r"^\s*f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT\s*=\s*[^;]+;"))
    detail = report("PowerParkingDetail (ATan2 + RwMathFPU constants)",
                    definition(header, "namespace PowerParkingDetail") if "namespace PowerParkingDetail" in header else "")
    check = report("CheckVehicleForPowerPark body", bodied(header, "inline bool CheckVehicleForPowerPark("))
    distance = report("BrnMath::GetPointToInfiniteLineDistance body",
                      bodied(source, "f32 GetPointToInfiniteLineDistance("))

    pieces = {
        "fxrcem4_pp_kf.inc": (radius or "const f32 KF_POWER_PARK_NEARBY_RADIUS = 0.0f;") + "\n"
                             + (aligned or "f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT = std::numeric_limits<f32>::quiet_NaN();"),
        "fxrcem4_pp_detail.inc": detail,
        "fxrcem4_pp_check.inc": check or ("inline bool CheckVehicleForPowerPark(Vector3, Vector3, Vector3, Vector3, "
                                          "f32&, f32&, f32&, f32&) { return false; }"),
        "fxrcem4_pp_line.inc": distance or "f32 GetPointToInfiniteLineDistance(Vector3, Vector3, Vector3) { return 0.0f; }",
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4PowerParkCheck.cpp", pieces, "fxrcem4_pp",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
