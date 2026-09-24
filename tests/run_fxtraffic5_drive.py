"""FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): TrafficEntityModule::DriveTowardsTarget @0x8273DFC0,
the console arithmetic of three legs (FX-TRAFFIC4 follow-ups (a) and (b), plus the same NaN class in the
reversing flip).

  (a) 0x8273E5CC..0x8273E5F8 -- the pedal split is vmaxfp128 / vminfp128 on pedal and on -pedal, which
      hands a NaN pedal through to BOTH mfGas and mfBrake; the tree's sign ternaries gave 0 and 0.
  (b) 0x8273E284 / 0x8273E518 -- the unit direction to the target is diff * rsqrt(|diff|^2), unguarded
      (only the distance is vsel-guarded at 0x8273E288); the tree substituted a zero vector on the target.
  (c) 0x8273E6C4 -- mfSteering *= rw::math::fpu::Sgn(GetSpeed()) @0x825BC920 (==0 -> 0, >=0 -> 1,
      else -1: a NaN speed is -1); the tree's ternary gave 0.

Numeric: tests/FxTraffic5Drive.cpp compiles the PRODUCTION DriveTowardsTarget (working tree, or --rev <b5
rev>) with its constants, helpers and the Vehicle / ParamTransform accessors, against recording doubles.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic5_drive.py [--rev <b5 rev>]
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
FIXTURE = "DriveFixture"
NUMERIC_CHECKS = 14

CONSTANTS = [
    "KF_GIVE_UP_RANDOM_CHANCE", "KF_GIVE_UP_SPEED", "KF_DRIVER_FAR_FROM_TARGET_DIST",
    "KF_DRIVER_MIN_PHYSICAL_TIME_TO_RETURN", "KF_DRIVER_RETURN_TO_TRAFFIC_DIST",
    "KF_DRIVER_RETURN_TO_TRAFFIC_DOT", "KF_DRIVER_SWERVE_STEERING_TIME", "KF_DRIVER_REVERSE_TURN_DIST",
    "KF_AVOIDANCE_HANDBRAKE_RISK", "KF_AVOIDANCE_HANDBRAKE",
    "KI_AVOID_HANDBRAKE_DIAG_CAP", "giAvoidHandbrakeDiagLines",
]
HELPERS = [
    "inline VecFloat SplatDrive(f32 lfValue)",
    "inline Vector3 ZeroVector3()",
    "inline f32 AvoidVmxMax(f32 lfA, f32 lfB)",
    "inline f32 AvoidVmxMin(f32 lfA, f32 lfB)",
]
ACCESSORS = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
]
BODY = "void TrafficEntityModule::DriveTowardsTarget("
VEHICLE_BODIES = [
    "VecFloat Vehicle::GetSpeed() const",
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
    body = body_or_empty(tree.read(MODULE_CPP), BODY)
    return [
        ("(a) the pedal split is the vmaxfp/vminfp pair, not a sign ternary (0x8273E5CC..0x8273E5F8)",
         "AvoidVmxMin(AvoidVmxMax(lfPedal, 0.0f), 1.0f)" in body and "AvoidVmxMin(AvoidVmxMax(-lfPedal, 0.0f), 1.0f)" in body),
        ("(b) the unit direction has no zero-vector guard (0x8273E284 / 0x8273E518)",
         body != "" and "ZeroVector3()" not in body),
    ]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts += [definition(module, helper) for helper in HELPERS]
        parts.append("}")
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in ACCESSORS]
        parts.append(definition(module, BODY).replace("TrafficEntityModule::", FIXTURE + "::", 1))
        parts += [definition(tree.read(VEHICLE_CPP), body) for body in VEHICLE_BODIES]
        parts += [definition(tree.read(PARAM_CPP), body) for body in PARAM_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic5Drive.cpp"), "drive.inc",
                           "\n".join(parts), "FxTraffic5Drive", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic5_drive", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
