"""FX-TRAFFIC4 item 1 (crash parity wave 5, 2026-09-24): GenerateDriverInputs' static (parked) section.

  A parked car (the static pool, index >= KU_MAX_STANDARD_TRAFFIC = 400) that the player hits IS
  promoted on PC -- by the same chain as any traffic car (UpdateCollidableVehicles -> overlap pair ->
  HandleHalfPotentialContact -> AddVehicleToPhysics POTENTIAL -> physics crash / slam ->
  HandleExternalResponses -> RecordTrafficVehicleIsPhysical) -- but GenerateDriverInputs' second
  section @0x82749B48 was a gate: the loop `continue`d past every index >= 400. On the console the
  first such index ENDS the standard loop (0x82749224 bge), the alive & physical set is rebuilt in
  place, the iterator re-validated against it (IsBitSet, else ++), and every remaining car gets
  TryClearupOffscreenTraffic (0x8274A1E8) with the result ignored -- the offscreen valve that
  removes a promoted parked car once it is 150 m from the camera and not rendered, and the only
  writer of its mPhysicalVehiclesFarFromPlayer bit. No driver record is sent for a parked car.

Wiring: the standard loop `break`s at >= 400, the iterator outlives it, no gate log remains.
Numeric: tests/FxTraffic4StaticSection.cpp compiles the PRODUCTION GenerateDriverInputs (working tree,
or --rev <b5 rev>) with its constants and accessors against recording doubles for the arms and the
clear-up valve.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic4_static_section.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
FIXTURE = "DriverFixture"
NUMERIC_CHECKS = 9

# The GenerateDriverInputs constants block (KF_STUCK_*, KI_DRIVER_EVENT_TYPE_TRAFFIC, the diag globals,
# LogMissingLeg_T3Drive): the whole anonymous namespace that holds them, verbatim.
CONSTANTS_BLOCK = "namespace\n{\n    // UpdateVehicleStuckTimers' own RODATA"
MODULE_BODIES = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
    "    TrafficPhysicsInfo* TrafficEntityModule::GetTrafficPhysicsInfoForVehicl(u32 luVehicle)",
    "void TrafficEntityModule::GenerateDriverInputs(",
]
VEHICLE_BODIES = [
    "void Vehicle::AddPhysicalTime(f32 lfDelta)",
    "void Vehicle::AddManoeuvreTime(f32 lfDelta)",
    "s32 Vehicle::GetPhysicalReason() const",
    "EntityId Vehicle::GetSympatheticCrashTarget() const",
    "bool Vehicle::IsSympatheticallyCrashing() const",
    "bool Vehicle::IsRecoveringFromSlam() const",
    "bool Vehicle::IsExtremeSwerving() const",
    "bool Vehicle::IsBeingChecked() const",
    "bool Vehicle::IsNormalPhysical() const",
    "Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const",
    "void Vehicle::SetCurrentManoeuvre(Manoeuvre leManoeuvre)",
    "void Vehicle::SetCurrentManoeuvrePhase(s8 liPhase)",
]


def wiring(tree):
    inputs = body_or_empty(tree.read(MODULE_CPP), "void TrafficEntityModule::GenerateDriverInputs(")
    test = inputs.find("if (liVehicle >= static_cast<s32>(KU_MAX_STANDARD_TRAFFIC))")
    brace = inputs.find("{", test) if test >= 0 else -1
    close = inputs.find("}", brace) if brace >= 0 else -1
    arm = inputs[brace:close] if 0 <= brace < close else ""
    loop = inputs.find("for (; lIterator != lPhysicalAliveVehicles.End(); ++lIterator)")
    return [
        ("the first index >= KU_MAX_STANDARD_TRAFFIC ENDS the standard loop (0x82749224 bge -> 0x82749B48)",
         "break;" in arm and "continue;" not in arm),
        ("the iterator is declared before the standard loop, so the static section continues from it",
         0 <= inputs.find("Iterator lIterator =") < loop),
        ("no missing-leg gate log for the static pool remains",
         inputs != "" and "static/parked pool" not in inputs and "LogMissingLeg_T3Drive(sbLoggedStaticPool" not in inputs),
    ]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        parts = ["namespace BrnTraffic {", definition(module, CONSTANTS_BLOCK)]
        parts += [definition(module, body).replace("TrafficEntityModule::", FIXTURE + "::", 1) for body in MODULE_BODIES]
        parts += [definition(tree.read(VEHICLE_CPP), body) for body in VEHICLE_BODIES]
        parts.append("}")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic4StaticSection.cpp"), "static_section.inc",
                           "\n".join(parts), "FxTraffic4StaticSection", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic4_static_section", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
