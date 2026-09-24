"""FX-TRAFFIC3 item 1c (crash parity wave 5, 2026-09-24): the GIVE_UP manoeuvre.

  UpdateGiveUpManoeuvre @0x8273EB60 was a GenerateDriverInputs gate (0x82749528), although three
  routes start the manoeuvre: the ten-second no-driving latch (0x8274979C) and
  CheckIfPhysicalVehicleIsStuck's both-ends arm (item 1b), both at phase 1, and DriveTowardsTarget's
  slam give-up at phase 0. A given-up car kept the zero-control record for ever. On the console it
  brakes against its motion (steering 0.5) until below 1 m/s, waits 4 s, waits again while touching
  at both ends (> 0.5 s, KF_VEHICLE_IS_STUCK_TIME), and otherwise -- unless the stuck test keeps it
  in GIVE_UP -- switches its indicators off, clears mfTimeNotDriving and becomes a NORMAL physical car.

Wiring: the GIVE_UP arm (jump-table case 2) calls UpdateGiveUpManoeuvre(liVehicle, &lControls).
Numeric: tests/FxTraffic3GiveUp.cpp compiled against the PRODUCTION bodies and constants (working
tree, or --rev <b5 rev>), with CheckIfPhysicalVehicleIsStuck and every accessor they reach.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic3_give_up.py [--rev <b5 rev>]
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
FIXTURE = "GiveUpFixture"
NUMERIC_CHECKS = 13

MODULE_BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32 luVehicle)",
    "    bool TrafficEntityModule::NeedToTakeActionAgainstJunctionFUP()",
    "bool TrafficEntityModule::CheckIfPhysicalVehicleIsStuck(u32 luVehicle)",
    "void TrafficEntityModule::UpdateGiveUpManoeuvre(",
]
PLAIN_BODIES = [   # not fixture members: extracted as they stand
    "bool TrafficPhysicsInfo::IsStuckFront() const",
    "bool TrafficPhysicsInfo::IsStuckBack() const",
]
CONSTANTS = [
    "KF_MIN_TIME_FOR_STUCK", "KF_VEHICLE_STUCK_REVERSE_CHANCE", "KF_STUCK_REVERSE_ROLL_MAX",
    "KI_STUCK_DIAG_CAP", "KI_STUCK_NONE_DIAG_CAP", "giStuckDiagLines", "giStuckNoneDiagLines",
    "giStuckBelowDiagLines", "gfStuckBelowDiagHigh", "gbStuckDispatchLogged",
    "giStuckRollCount", "KI_STUCK_ROLL_CENSUS_PERIOD",
    "KF_VEHICLE_IS_STUCK_TIME", "KI_GIVE_UP_DIAG_CAP", "giGiveUpDiagLines",
]
VEHICLE_BODIES = [
    "void Vehicle::SetCurrentManoeuvre(Manoeuvre leManoeuvre)",
    "void Vehicle::SetCurrentManoeuvrePhase(s8 liPhase)",
    "s32 Vehicle::GetCurrentManoeuvrePhase() const",
    "Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const",
    "void Vehicle::StartGiveUpManoeuvre()",
    "void Vehicle::SetHeadlightsFlashed(bool lbOn)",
    "void Vehicle::SetLeftIndicatorOn(bool lbOn)",
    "void Vehicle::SetRightIndicatorOn(bool lbOn)",
    "bool Vehicle::IsIndicatingLeft() const",
    "bool Vehicle::IsIndicatingRight() const",
    "void Vehicle::SetIndicatingLeft(bool lbOn)",
    "void Vehicle::SetIndicatingRight(bool lbOn)",
    "void Vehicle::SetPhysicalReason(s8 liReason)",
    "VecFloat Vehicle::GetSpeed() const",
    "f32 Vehicle::GetManoeuvreTime() const",
    "void Vehicle::ResetManoeuvreTime(f32 lfTime)",
]
RANDOM_BODIES = ["    f32 Random::RandomFloat(f32 lfMin, f32 lfMax)"]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def wiring(tree):
    inputs = body_or_empty(tree.read(MODULE_CPP), "void TrafficEntityModule::GenerateDriverInputs(")
    arm = inputs.find("else if (leManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP)")
    call = inputs.find("UpdateGiveUpManoeuvre(static_cast<u32>(liVehicle), &lControls);", arm) if arm >= 0 else -1
    unknown = inputs.find('"Unknown manoeuvre"', arm) if arm >= 0 else -1
    return [("GenerateDriverInputs' GIVE_UP arm calls UpdateGiveUpManoeuvre(liVehicle, &lControls) (0x82749528)",
             0 <= arm < call < unknown)]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", "namespace {"]
        parts += [constant_line(module, name) for name in CONSTANTS]
        parts.append("}")
        parts += [definition(module, body) for body in PLAIN_BODIES]
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in MODULE_BODIES]
        parts += [definition(tree.read(VEHICLE_CPP), body) for body in VEHICLE_BODIES]
        parts.append("}")
        parts.append("namespace CgsNumeric {")
        parts += [definition(tree.read(RANDOM_CPP), body) for body in RANDOM_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic3GiveUp.cpp"), "give_up.inc",
                           "\n".join(parts), "FxTraffic3GiveUp", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic3_give_up", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
