"""FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): TrafficEntityModule::AddVehicleToPhysics @0x827425B0,
the create-queue-full leg 0x82742674..0x8274269C / 0x82742894..0x827428C0.

  The world -> physics hop of the traffic hit-reaction chain (HandleHalfPotentialContact ->
  AddVehicleToPhysics POTENTIAL). The console reads mCreateTrafficEventQueue's miLength (+0x20778) and
  miMaxLength (+0x20774) and, unless length < max, logs "CreateTrafficEventQueue is full" and RETURNS
  without posting and without marking lpCreatedBodies. On PC this was a gate on a stale blocker ("no
  accessor" -- VehicleInputInterface::GetCreateTrafficBodyEvents() is DWARF h:207 and was declared all
  along), and BaseEventQueue::AddEvent appends unconditionally: a 26th create event in one frame was
  written past the 25-slot buffer. The potential-contact route has no budget of its own and re-posts
  every in-contact potential car every frame (lCreatedBodies is PrePhysicsUpdate's per-frame local).

Wiring: the body reads GetCreateTrafficBodyEvents() and returns before the post when full.
Numeric: tests/FxTraffic5CreateQueue.cpp compiles the PRODUCTION AddVehicleToPhysics (working tree, or
--rev <b5 rev>) against a recording VehicleInputInterface double.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic5_create_queue.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP, REPO

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
# VolumeInstanceId::SetEntityIDOwner / SetEntityIDEntityIndex: shared helpers, not under test.
VOLUME_INSTANCE_CPP = REPO / "src/GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.cpp"
FIXTURE = "CreateFixture"
NUMERIC_CHECKS = 9

ACCESSORS = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
]
BODY = "void TrafficEntityModule::AddVehicleToPhysics("
REQUIRED = [("BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput", "FakeVehicleInput* lpVehicleInput")]
OPTIONAL = [("BrnPhysics::Vehicle::VehicleInputInterface::CreateTrafficEventQueue", "FakeCreateQueue")]


def wiring(tree):
    body = body_or_empty(tree.read(MODULE_CPP), BODY)
    read = body.find("GetCreateTrafficBodyEvents()")
    post = body.find("CreatePhysicalTraffic(")
    ret = body.find("return;", read) if read >= 0 else -1
    return [
        ("AddVehicleToPhysics reads GetCreateTrafficBodyEvents() before the post (0x82742674)",
         0 <= read < post),
        ("...and returns when the queue is full, before the post and the SetBit (0x82742894..0x827428C0)",
         0 <= ret < post and "GetLength() < lpCreateQueue->GetMaxLength()" in body),
    ]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        body = definition(module, BODY).replace("TrafficEntityModule::", FIXTURE + "::", 1)
        for old, new in REQUIRED:
            if old not in body:
                raise ValueError("body text changed, cannot substitute: " + old)
            body = body.replace(old, new)
        for old, new in OPTIONAL:
            body = body.replace(old, new)
        parts = ["namespace BrnTraffic {"]
        parts += [definition(module, accessor).replace("TrafficEntityModule::", FIXTURE + "::", 1)
                  for accessor in ACCESSORS]
        parts += [body, "}"]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic5CreateQueue.cpp"), "create_queue.inc",
                           "\n".join(parts), "FxTraffic5CreateQueue",
                           extra_sources=[STRSTREAM_CPP, VOLUME_INSTANCE_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic5_create_queue", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
