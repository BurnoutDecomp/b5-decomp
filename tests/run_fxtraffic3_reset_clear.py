"""FX-TRAFFIC3 (crash parity wave 5, 2026-09-24, CC-1): the post-wreck traffic clear.

  TrafficEntityModule::HandleResetRaceCarEvents @0x82742CE8 had no body and PostPhysicsUpdate's
  RUNNING arm (meState == 1 -> 0x8274EA20) never made the console's call at 0x8274EA5C, between
  HandleExternalResponses (0x8274EA50) and HandleContactPoints (0x8274EA68). The producer (the
  vehicle manager's RaceCarResetEvent queue, +0x5B0) was live, so after every player wreck the
  console cleared all traffic -- parked cars included -- within 75 m of the reset point (12 m
  online, 10 m half-height), and PC left it standing.

Wiring: the RUNNING arm calls HandleResetRaceCarEvents(lpInput) after HandleExternalResponses and
before HandleContactPoints. Numeric: tests/FxTraffic3ResetClear.cpp compiled against the PRODUCTION
HandleResetRaceCarEvents + constants, KillAllTrafficInCylinder, GetVehicle, GetVehicleTransform and
InputBuffer_PostPhysics::GetVehicleManagerOutputInterface (working tree, or --rev <b5 rev>);
RemoveVehicle is a recorder. Expectations are the ARTIST values (75.0 / 12.0 / 10.0, r6 = 1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic3_reset_clear.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
GETTERS_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO_InputBuffer_Getters.cpp"
FIXTURE = "ResetFixture"
NUMERIC_CHECKS = 18

BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
    "void TrafficEntityModule::KillAllTrafficInCylinder(",
    "void TrafficEntityModule::HandleResetRaceCarEvents(",
]
CONSTANTS = [
    "KF_RESET_ON_TRACK_KILL_RADIUS", "KF_RESET_ON_TRACK_KILL_HALFHEIGHT", "KF_RESET_ON_TRACK_KILL_RADIUS_ONLINE",
    "KI_RESET_DIAG_CAP", "giResetDiagLines",
]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def running_arm(module):
    post = body_or_empty(module, "void TrafficEntityModule::PostPhysicsUpdate(")
    start = post.find("case E_STATE_RUNNING:")
    end = post.find("case E_STATE_TEARING_DOWN:", start)
    return post[start:end] if 0 <= start < end else ""


def wiring(tree):
    arm = running_arm(tree.read(MODULE_CPP))
    external = arm.find("HandleExternalResponses(lpInput);")
    reset = arm.find("HandleResetRaceCarEvents(lpInput);")
    contacts = arm.find("HandleContactPoints(lpInput);")
    return [("PostPhysicsUpdate's RUNNING arm calls HandleResetRaceCarEvents(lpInput) after HandleExternalResponses "
             "and before HandleContactPoints (0x8274EA50 -> 0x8274EA5C -> 0x8274EA68)",
             0 <= external < reset < contacts)]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts.append("}")
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in BODIES]
        parts.append("namespace BrnTrafficIO {")
        parts.append(definition(tree.read(GETTERS_CPP),
                                "    const InputBuffer_PostPhysics::VehicleManagerOutputInterface* "
                                "InputBuffer_PostPhysics::GetVehicleManagerOutputInterface() const"))
        parts.append("}")
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic3ResetClear.cpp"), "reset_clear.inc",
                           "\n".join(parts), "FxTraffic3ResetClear", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic3_reset_clear", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
