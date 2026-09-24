"""FX-GEOMETRIC (crash parity 2026-09-24): BrnWorld::PlaceOnTrackManager::PlaceCarOnTrack -- the per-answer tail of
PrePhysicsUpdate (ARTIST 0x822F711C..0x822F7898). Its no-intersection arm (0x822F7384..0x822F73B8) reverts to
ActiveRaceCar::GetResetCoords with the world Y normal; the PC parked the car on the requested pose instead until the
place-on-track round trip went live. Both arms then print the console's "Selected reset data" line (0x822F725C).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgeometric_place_car_on_track.py [--pre-fix <b5 rev>]
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import REPO, build_and_run, code_mask, definition, pre_fix_rev, read

MANAGER_CPP = "src/GameSource/World/BrnPlaceOnTrackManager.cpp"


def constant(source, name):
    match = re.search(r"^[^\n/]*\b" + name + r"\s*=\s*[^;]+;", code_mask(source), re.M)
    if not match:
        raise ValueError("constant not found: " + name)
    return source[match.start():match.end()].strip()


def main():
    rev = pre_fix_rev(sys.argv)
    cpp = read(MANAGER_CPP, rev)
    helpers = constant(cpp, "KF_PLACE_ON_TRACK_SIMILAR_EPSILON") + "\n" + \
        definition(cpp, "static bool AreVectorsSimilar(")
    body = definition(cpp, "void PlaceOnTrackManager::PlaceCarOnTrack(")
    pieces = {"fxg_pcot_helpers.inc": helpers, "fxg_pcot_body.inc": body}
    rc = build_and_run(REPO / "tests" / "FxGeometricPlaceCarOnTrack.cpp", pieces, "fxg_pcot")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
