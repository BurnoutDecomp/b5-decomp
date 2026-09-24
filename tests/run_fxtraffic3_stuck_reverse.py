"""FX-TRAFFIC3 item 1b (crash parity wave 5, 2026-09-24): CC-2's back-off.

  CC-2 (e179f6a2) made a traffic car pinned at its nose or tail raise its contact side, so the stuck
  timers grow and GenerateDriverInputs' wedge arm cuts gas and brake after 0.05 s. The console's next
  two legs were still gates: DriveTowardsTarget never called CheckIfPhysicalVehicleIsStuck @0x8272C010
  (0x8273E150) and GenerateDriverInputs' STUCK_REVERSE arm never called UpdateStuckReverseManoeuvre
  @0x82719430 (0x827493C8). So a pinned car sat with its pedals cut and never backed off; on the
  console, past 3.2 s at its nose it rolls (0.4 %) into the stuck-reverse manoeuvre -- brake 0.75
  (reverse) steering on the dot to its target until 30 degrees off, the back touches, or 3 s; then
  the handbrake (0.4) until below 2 m/s or 3 s -- and at both ends it gives up.

Wiring: DriveTowardsTarget returns when CheckIfPhysicalVehicleIsStuck(luVehicle) is true, before it
reads its target; the STUCK_REVERSE arm calls UpdateStuckReverseManoeuvre(liVehicle, &lControls).
Numeric: tests/FxTraffic3StuckReverse.cpp compiled against the PRODUCTION bodies and constants
(working tree, or --rev <b5 rev>), with every accessor they reach.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic3_stuck_reverse.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
RANDOM_CPP = "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
FIXTURE = "StuckFixture"
NUMERIC_CHECKS = 26

MODULE_BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
    "    TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32 luVehicle)",
    "    bool TrafficEntityModule::NeedToTakeActionAgainstJunctionFUP()",
    "bool TrafficEntityModule::CheckIfPhysicalVehicleIsStuck(u32 luVehicle)",
    "void TrafficEntityModule::UpdateStuckReverseManoeuvre(",
]
CONSTANTS = [
    "KF_MIN_TIME_FOR_STUCK", "KF_VEHICLE_STUCK_REVERSE_CHANCE", "KF_STUCK_REVERSE_ROLL_MAX",
    "kfManoeuvreStuckReverse_MaxTime_Phase0Dot_Phase1Speed_W",
    "KI_STUCK_DIAG_CAP", "KI_STUCK_NONE_DIAG_CAP", "giStuckDiagLines", "giStuckNoneDiagLines",
    "giStuckBelowDiagLines", "gfStuckBelowDiagHigh", "gbStuckDispatchLogged",
    "giStuckRollCount", "KI_STUCK_ROLL_CENSUS_PERIOD",
]
DIAG_ARRAYS = [r"^[ \t]*s8[ \t]+gaiStuckReverseDiagPhase\[[^\]]*\];"]
VEHICLE_BODIES = [
    "void Vehicle::SetCurrentManoeuvre(Manoeuvre leManoeuvre)",
    "void Vehicle::SetCurrentManoeuvrePhase(s8 liPhase)",
    "s32 Vehicle::GetCurrentManoeuvrePhase() const",
    "Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const",
    "void Vehicle::StartGiveUpManoeuvre()",
    "void Vehicle::SetHeadlightsFlashed(bool lbOn)",
    "void Vehicle::SetLeftIndicatorOn(bool lbOn)",
    "void Vehicle::SetRightIndicatorOn(bool lbOn)",
    "VecFloat Vehicle::GetSpeed() const",
    "Vector3 Vehicle::GetTargetPos() const",
    "f32 Vehicle::GetManoeuvreTime() const",
    "void Vehicle::ResetManoeuvreTime(f32 lfTime)",
]
RANDOM_BODIES = ["    f32 Random::RandomFloat(f32 lfMin, f32 lfMax)"]


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
    drive = body_or_empty(module, "void TrafficEntityModule::DriveTowardsTarget(")
    give_up = drive.find("lpVehicle->StartGiveUpManoeuvre();")
    stuck = drive.find("if (CheckIfPhysicalVehicleIsStuck(luVehicle))")
    returned = drive.find("return;", stuck) if stuck >= 0 else -1
    target = drive.find("lpVehicle->GetTargetPos()")
    inputs = body_or_empty(module, "void TrafficEntityModule::GenerateDriverInputs(")
    arm = inputs.find("if (leManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE)")
    call = inputs.find("UpdateStuckReverseManoeuvre(static_cast<u32>(liVehicle), &lControls);", arm) if arm >= 0 else -1
    swerve = inputs.find("else if (lpVehicle->IsExtremeSwerving())", arm) if arm >= 0 else -1
    return [
        ("DriveTowardsTarget returns when CheckIfPhysicalVehicleIsStuck(luVehicle) is true, after the give-up "
         "test and before it reads its target (0x8273E150 -> 0x8273E75C)",
         0 <= give_up < stuck < returned < target),
        ("GenerateDriverInputs' STUCK_REVERSE arm calls UpdateStuckReverseManoeuvre(liVehicle, &lControls) "
         "(0x827493C8)", 0 <= arm < call < swerve),
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
        parts.append("namespace CgsNumeric {")
        parts += [definition(tree.read(RANDOM_CPP), body) for body in RANDOM_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic3StuckReverse.cpp"), "stuck_reverse.inc",
                           "\n".join(parts), "FxTraffic3StuckReverse", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic3_stuck_reverse", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
