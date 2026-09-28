"""L2 CAMCOLLIDE piece 5 (owner's list 2026-09-28): the camera's traffic predictor against the ARTIST words on emu64.

  BrnDirector::Camera::Utils::VehicleCollisionPredictor::Update @0x822230D8
                     -- src/GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.cpp

VisibilityCollisionPolicy::ProcessSceneQueryResults @0x82224530 calls it every frame (0x822245F8): for each traffic
record, is the camera's line (its position, along its velocity relative to that vehicle) through the vehicle's
ellipsoid (the record's transform, its half extents as radii)? A hit sets the flag and the time until the camera meets
the vehicle; a camera that starts a shot within 1 s of a vehicle fails with E_FAILED_STARTED_TOO_NEAR_VEHICLE (10).
On the PC it had no body (the header kept only the flag / time pair), so the call was a FLAGGED gate and reason 10
could never fire.

Numeric: tests/L2VehiclePredictor.cpp compiles the revision's BrnVehicleCollisionPredictor.cpp (the revision's
header shadowed in) and replays tests/L2VehiclePredictorData.h, generated from the console's words on emu64 by
scratch/OWNERLIST_0927/L2/emu/gen_vcp.py (seed 9285, 1606 rows, 604 hits): 33 fixed geometries over four frames (head
on, pulling away, beside, inside, past, crossing, no relative motion, tangent), two / three cars (the LAST hit wins, a
miss keeps an earlier hit), the boundaries (a start exactly on the unit sphere, an exactly tangent line, radii whose
|h|^2 is exactly FLT_EPSILON), a sheared and a scaled frame, zero radii, every transform / velocity / radius lane and
the camera's position / velocity lanes set to NaN / inf, and 1500 random scenes of 1..4 cars. Checks: the flag byte,
the time word (0xA5A5A5A5 = untouched; a NaN by class) and the number of asserts fired, per row class.
A revision without the body does not build (every numeric check counts as failed).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l2_vehicle_predictor.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

U = "src/GameSource/Director/Camera/Utils/"
VCP_CPP = U + "BrnVehicleCollisionPredictor.cpp"
VCP_H = U + "BrnVehicleCollisionPredictor.h"
NUMERIC_CHECKS = 3   # F, A, S


def _read(tree, path):
    try:
        return tree.read(path)
    except (OSError, ValueError):
        return ""


def wiring(tree):
    header = code_only(_read(tree, VCP_H))
    declared = re.search(r"void\s+Update\s*\(\s*const\s+AllVehicleData\s*&\s*\w+\s*,\s*VecFloat\s+\w+\s*,\s*Vector3\s+\w+"
                         r"\s*,\s*Vector3\s+\w+\s*\)\s*;", header) is not None
    layout = re.search(r"bool\s+mbHasPredictedCollision\s*;\s*PredictedCollision\s+mSoonestPredictedCollision\s*;",
                       header) is not None
    return [
        ("BrnVehicleCollisionPredictor.h declares Update(const AllVehicleData&, VecFloat, Vector3, Vector3) (DWARF :63)",
         declared),
        ("BrnVehicleCollisionPredictor.h has the DWARF layout: bool mbHasPredictedCollision (+0), PredictedCollision "
         "mSoonestPredictedCollision (+4) (:73 / :74)", layout),
    ]


def numeric(tree):
    source = _read(tree, VCP_CPP)
    header = _read(tree, VCP_H)
    if not source or "VehicleCollisionPredictor::Update(" not in source:
        print("NUMERIC: cannot build -- " + VCP_CPP + " has no Update body")
        return None
    with tempfile.TemporaryDirectory(prefix="brn_l2_vcp_") as directory:
        vcp = Path(directory) / "BrnVehicleCollisionPredictor.cpp"
        vcp.write_text(source, encoding="utf-8")
        return compile_and_run(Path(__file__).with_name("L2VehiclePredictor.cpp"), "l2_vcp_unused.inc",
                               "// not included: the body comes from BrnVehicleCollisionPredictor.cpp\n",
                               "L2VehiclePredictor", shadow={VCP_H: header}, extra_sources=(vcp,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_l2_vehicle_predictor", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
