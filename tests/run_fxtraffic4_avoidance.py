"""FX-TRAFFIC4 item 3 (crash parity wave 5, 2026-09-24): the avoidance steering.

  DriveTowardsTarget @0x8273DFC0 steers every driving physical car through
  CalculateAndSetSteeringUsingAvoidance @0x8273D258 (0x8273E56C) unless it is in the first 3 s of an
  extreme swerve, and pulls the handbrake to 0.5 when the risk it reports reaches 0.7 (0x8273E71C).
  The call was a gate falling back to plain target steering at scale 0, and the pipeline under it had
  no body (PrecalculateAvoidanceFeelerData @0x82708E78, Avoidance_GetBestVehicleDirection @0x8272C248,
  Avoidance_CalculatePassingScore @0x827199B8, Avoidance_CalculateDistancePosVelToOrigin @0x82708DD0,
  the inlined feelers / Convert3DVectorTo2D / GetAvoid* accessors): a driving car steered straight at
  its target through whatever was in the way and never pulled the handbrake.

Wiring: DriveTowardsTarget makes the avoidance call (no gate log left), Reset calls
PrecalculateAvoidanceFeelerData (0x8272D7E8), and Construct keeps mbDEBUGEnableAvoidance ON
(li r27,1 @0x82740988 stored at 0x82740C58).
Numeric: tests/FxTraffic4Avoidance.cpp compiled against the PRODUCTION bodies and constants (working
tree, or --rev <b5 rev>), with every accessor they reach.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic4_avoidance.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
PARAM_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.cpp"
AVOID = "AvoidFixture"
DRIVE = "DriveFixture"
NUMERIC_CHECKS = 42

CONSTANTS = [
    # CalculateAndSetSteering @0x82718E48 and DriveTowardsTarget @0x8273DFC0
    "KF_STEERING_SCALE_THRESHOLD", "KF_STEERING_SCALE_HIGH",
    "KF_GIVE_UP_RANDOM_CHANCE", "KF_GIVE_UP_SPEED", "KF_DRIVER_FAR_FROM_TARGET_DIST",
    "KF_DRIVER_MIN_PHYSICAL_TIME_TO_RETURN", "KF_DRIVER_RETURN_TO_TRAFFIC_DIST",
    "KF_DRIVER_RETURN_TO_TRAFFIC_DOT", "KF_DRIVER_SWERVE_STEERING_TIME", "KF_DRIVER_REVERSE_TURN_DIST",
    # the avoidance block
    "KF_AVOIDANCE_FEELERS_START_ANGLE", "KF_TRAFFIC_AVOIDANCE_FEELERS_ANGLE", "KF_AVOIDANCE_FEELER_MEAN",
    "KF_AVOIDANCE_MIN_RISK", "KF_AVOIDANCE_MIN_TARGET_DIST", "KF_AVOIDANCE_SNAP_DOT",
    "KF_AVOIDANCE_HANDBRAKE_RISK", "KF_AVOIDANCE_HANDBRAKE",
    "KI_AVOID_DIAG_CAP", "KI_AVOID_QUIET_DIAG_CAP", "KI_AVOID_HANDBRAKE_DIAG_CAP",
    "giAvoidDiagLines", "giAvoidQuietDiagLines", "giAvoidHandbrakeDiagLines",
]
HELPERS = [
    "inline VecFloat SplatDrive(f32 lfValue)",
    "inline Vector3 ZeroVector3()",
    "inline f32 AvoidVmxMax(f32 lfA, f32 lfB)",
    "inline f32 AvoidVmxMin(f32 lfA, f32 lfB)",
    "inline f32 AvoidPacketLane(const Vector4& lrMember, u32 luLane)",
]
FREE_BODIES = [
    "void Convert3DVectorTo2D(Vector3 l3DVector, Vector2& l2DVector)",
]
ACCESSORS = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
]
AVOID_BODIES = [
    "VecFloat TrafficEntityModule::GetAvoidPassImpactTimeMax() const",
    "VecFloat TrafficEntityModule::GetAvoidPassImpactTimeScoreFactor() const",
    "VecFloat TrafficEntityModule::GetAvoidPassMaxDistance() const",
    "VecFloat TrafficEntityModule::GetAvoidPassHeightSkip() const",
    "VecFloat TrafficEntityModule::GetAvoidOffcourseScoreFactor() const",
    "VecFloat TrafficEntityModule::GetAvoidMaxOverallRisk() const",
    "void TrafficEntityModule::PrecalculateAvoidanceFeelerData()",
    "VecFloat TrafficEntityModule::Avoidance_CalculateDistancePosVelToOrigin(",
    "VecFloat TrafficEntityModule::Avoidance_CalculatePassingScore(",
    "void TrafficEntityModule::Avoidance_CalculateFeelers(",
    "void TrafficEntityModule::Avoidance_GetBestVehicleDirection(",
    "void TrafficEntityModule::CalculateAndSetSteeringUsingAvoidance(",
    "void TrafficEntityModule::CalculateAndSetSteering(u32 luVehicle, Vector3 lTargetDirection,",
]
DRIVE_BODIES = [
    "void TrafficEntityModule::DriveTowardsTarget(",
]
VEHICLE_BODIES = [
    "VecFloat Vehicle::GetSpeed() const",
    "VecFloat Vehicle::GetSteering() const",
    "void Vehicle::SetSteering(f32 lfValue)",
    "Vector3 Vehicle::GetTargetPos() const",
    "f32 Vehicle::GetRandomVal() const",
    "f32 Vehicle::GetPhysicalTime() const",
    "bool Vehicle::IsRecoveringFromSlam() const",
    "bool Vehicle::IsExtremeSwerving() const",
    "Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const",
    "void Vehicle::SetCurrentManoeuvre(Manoeuvre leManoeuvre)",
]
PARAM_BODIES = [
    "VecFloat ParamTransform::GetSpeed() const",
    "Vector3 ParamTransform::GetDirection() const",
]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def wiring(tree):
    module = tree.read(MODULE_CPP)
    drive = body_or_empty(module, "void TrafficEntityModule::DriveTowardsTarget(")
    reset = body_or_empty(module, "void TrafficEntityModule::Reset(")
    construct = body_or_empty(module, "void TrafficEntityModule::Construct(")
    return [
        ("DriveTowardsTarget calls CalculateAndSetSteeringUsingAvoidance (0x8273E56C) and logs no missing leg",
         "CalculateAndSetSteeringUsingAvoidance(" in drive and "LogMissingLeg" not in drive),
        ("Reset calls PrecalculateAvoidanceFeelerData() (0x8272D7E8)",
         "PrecalculateAvoidanceFeelerData();" in reset),
        ("Construct keeps mbDEBUGEnableAvoidance ON (li r27,1 @0x82740988, stbx at 0x82740C58)",
         re.search(r"mbDEBUGEnableAvoidance\s*=\s*true\s*;", construct) is not None),
    ]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts += [definition(module, helper) for helper in HELPERS]
        parts.append("}")
        parts += [definition(module, body) for body in FREE_BODIES]
        for fixture in (AVOID, DRIVE):
            parts += [definition(module, body).replace("TrafficEntityModule::", fixture + "::", 1) for body in ACCESSORS]
        parts += [definition(module, body).replace("TrafficEntityModule::", AVOID + "::", 1) for body in AVOID_BODIES]
        parts += [definition(module, body).replace("TrafficEntityModule::", DRIVE + "::", 1) for body in DRIVE_BODIES]
        parts += [definition(tree.read(VEHICLE_CPP), body) for body in VEHICLE_BODIES]
        parts += [definition(tree.read(PARAM_CPP), body) for body in PARAM_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic4Avoidance.cpp"), "avoidance.inc",
                           "\n".join(parts), "FxTraffic4Avoidance", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic4_avoidance", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
