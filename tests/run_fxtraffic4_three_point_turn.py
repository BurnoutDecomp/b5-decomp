"""FX-TRAFFIC4 item 2 (crash parity wave 5, 2026-09-24): the three-point turn.

  DriveTowardsTarget starts E_MANOEUVRE_3_POINT_TURN when the target is more than 15 m behind the car
  (0x8273E6B0), and GenerateDriverInputs' jump-table arm for manoeuvre 2 calls
  Update3PointTurnManoeuvre @0x827190B0 (0x82749500) to drive it. The arm was a gate and the function
  had no body, so a turning car kept the zero-control record and sat still until the ten-second
  no-driving latch made it give up. On the console it reverses on brake 0.8 steering on the side dot
  until the target is 74 degrees or more to one side (|dot| > 0.96), then drives forward on gas 0.8
  with opposite lock, and ends once it faces within 45 degrees of the target (dot > 0.707).

Wiring: the arm calls Update3PointTurnManoeuvre(liVehicle, &lControls) and no gate log remains in it.
Numeric: tests/FxTraffic4ThreePointTurn.cpp compiled against the PRODUCTION body and constants
(working tree, or --rev <b5 rev>), with every accessor it reaches.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic4_three_point_turn.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
FIXTURE = "TurnFixture"
NUMERIC_CHECKS = 17

MODULE_BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
    "void TrafficEntityModule::Update3PointTurnManoeuvre(",
]
CONSTANTS = [
    "KF_3_POINT_TURN_DONE_DOT", "KF_3_POINT_TURN_REVERSE_MAX_SIDE", "KF_3_POINT_TURN_PEDAL",
    "KI_3_POINT_TURN_DIAG_CAP", "gi3PointTurnDiagLines",
]
DIAG_ARRAYS = [r"^[ \t]*s8[ \t]+gai3PointTurnDiagState\[[^\]]*\];"]
VEHICLE_BODIES = [
    "void Vehicle::SetCurrentManoeuvre(Manoeuvre leManoeuvre)",
    "void Vehicle::SetCurrentManoeuvrePhase(s8 liPhase)",
    "s32 Vehicle::GetCurrentManoeuvrePhase() const",
    "Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const",
    "VecFloat Vehicle::GetSpeed() const",
    "Vector3 Vehicle::GetTargetPos() const",
]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def pattern_line(source, pattern):
    match = re.search(pattern, source, re.M)
    if match is None:
        raise ValueError("declaration absent: " + pattern)
    return match.group(0).strip()


def wiring(tree):
    module = tree.read(MODULE_CPP)
    inputs = body_or_empty(module, "void TrafficEntityModule::GenerateDriverInputs(")
    arm = inputs.find("else if (leManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN)")
    give_up = inputs.find("else if (leManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP)", arm) if arm >= 0 else -1
    arm_text = inputs[arm:give_up] if 0 <= arm < give_up else ""
    return [
        ("GenerateDriverInputs' 3_POINT_TURN arm calls Update3PointTurnManoeuvre(liVehicle, &lControls) "
         "(0x82749500)",
         "Update3PointTurnManoeuvre(static_cast<u32>(liVehicle), &lControls);" in arm_text),
        ("... and no longer logs a missing leg there", arm_text != "" and "LogMissingLeg" not in arm_text),
    ]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts += [pattern_line(module, pattern) for pattern in DIAG_ARRAYS]
        parts.append("}")
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in MODULE_BODIES]
        parts += [definition(tree.read(VEHICLE_CPP), body) for body in VEHICLE_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic4ThreePointTurn.cpp"), "three_point_turn.inc",
                           "\n".join(parts), "FxTraffic4ThreePointTurn", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic4_three_point_turn", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
