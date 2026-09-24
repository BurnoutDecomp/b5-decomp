"""FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): TrafficEntityModule::UpdateCollidableVehicles @0x827302C8.

  The first link of the traffic hit-reaction chain: which traffic cars are SOLID (a scene volume, so
  the broad phase can pair them with the player) and which are cached for the avoidance steering.
  Four divergences from the ARTIST body, all fixed together:
    (D1) 0x82731A54..0x82731D18 -- a car of the half of the pool NOT re-evaluated this frame stays in
         the avoidance cache when mVehiclesAvoidableLastFrame (+0x72578) remembers it, and the bit is
         consumed (andc @0x82731D14). PC treated every such car as not avoidable, so the cache held
         only the evaluated half and alternated (FX-TRAFFIC4's 2/5-packet observation);
    (D2) 0x82730CB0..0x82730CE0 -- flag 27 of mCameraLastFrame's current flags
         (E_FLAG_ROAD_FOLLOWING_CAM, DWARF BrnCameraState.h:36) makes the camera's Pos row a source,
         appended after the average. It was a gate on an "unnamed flag";
    (D3) 0x82730C80..0x82730CAC -- vrefp + Newton step with no zero test: no source -> NaN centre.
         PC had an invented guard;
    (D4) the "[PC SAFETY]" ~alive & collidable sweep is not in the binary; KillDyingVehicleEntities
         @0x82741E40 runs just before in the same block and owns that teardown.

Wiring: no sweep, no camera-source gate log, the carry-over leg reads and clears the bit.
Numeric: tests/FxTraffic5Collidable.cpp compiles the PRODUCTION UpdateCollidableVehicles (working
tree, or --rev <b5 rev>) with its file-local helpers and accessors against IO doubles.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic5_collidable.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP, REPO

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
VEHICLE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.cpp"
CAMERA_CPP = "src/GameSource/Director/Camera/Camera.cpp"
# VolumeInstanceId::SetEntityIDEntityIndex (@0x822B0E70): a shared helper, not under test.
VOLUME_INSTANCE_CPP = REPO / "src/GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.cpp"
FIXTURE = "CollidableFixture"
NUMERIC_CHECKS = 26

HELPER_BLOCK = ("namespace\n{\n    // NAMED LEG GATE, file-local. NOT IN THE X360 BINARY.\n"
                "    inline void LogMissingLeg_T4")
VOLUME_ID = "    inline CgsSceneManager::VolumeInstanceId MakeTrafficVolumeInstanceId(u32 luVehicle)"
ACCESSORS = [
    "    Vehicle* TrafficEntityModule::GetVehicle(u32 luIndex)",
    "    Matrix44Affine TrafficEntityModule::GetVehicleTransform(u32 luIndex) const",
    "    const VehicleTypeRuntime* TrafficEntityModule::GetVehicleTypeRuntime(u32 luVehicleType) const",
]
BODY = "void TrafficEntityModule::UpdateCollidableVehicles("
VEHICLE_BODIES = [
    "VecFloat Vehicle::GetSpeed() const",
    "void Vehicle::SetCollidable(bool lbCollidable,",
]
# The IO types of the body, swapped for the fixture's doubles.
REPLACEMENTS = [
    ("const BrnTrafficIO::InputBuffer_PreScene* lpInput", "const FakeInput* lpInput"),
    ("BrnTrafficIO::OutputBuffer_PreScene* lpOutput", "FakeOutput* lpOutput"),
    ("BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface", "FakeRaceCars"),
    ("BrnPhysics::Vehicle::RaceCarState", "FakeRaceCarState"),
]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:const[ \t]+)?[\w:]+[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    if match is None:
        raise ValueError("constant absent: " + name)
    return match.group(0).strip()


def wiring(tree):
    body = body_or_empty(tree.read(MODULE_CPP), BODY)
    carry = body.find("mVehiclesAvoidableLastFrame.IsBitSet(luVehicle)")
    return [
        ("no ~alive & collidable sweep (KillDyingVehicleEntities @0x82741E40 owns that teardown)",
         body != "" and "SetInverse(mVehicleSoaData.mAliveVehicles)" not in body and "RemoveForCollision" in body
         and body.count("RemoveForCollision") == 1),
        ("the camera source is live: E_FLAG_ROAD_FOLLOWING_CAM appends mCameraLastFrame's position (0x82730CB0)",
         "E_FLAG_ROAD_FOLLOWING_CAM" in body and "camera collision-source" not in body),
        ("the non-candidate leg reads and consumes mVehiclesAvoidableLastFrame (0x82731A54..0x82731D18)",
         carry >= 0 and "mVehiclesAvoidableLastFrame.UnSetBit(luVehicle)" in body[carry:]),
        ("no zero-count guard on mAveragePhysicalCentre (0x82730C80..0x82730CAC)",
         body != "" and "if (lfSourceCount > 0.0f)" not in body),
    ]


def numeric(tree):
    module = tree.read(MODULE_CPP)
    try:
        body = definition(module, BODY).replace("TrafficEntityModule::", FIXTURE + "::", 1)
        for old, new in REPLACEMENTS:
            if old not in body:
                raise ValueError("body text changed, cannot substitute: " + old)
            body = body.replace(old, new)
        parts = ["namespace BrnTraffic {", "namespace {", constant_line(module, "KU8_TRAFFIC_ENTITY_OWNER"),
                 definition(module, VOLUME_ID), "}", definition(module, HELPER_BLOCK)]
        parts += [definition(module, accessor).replace("TrafficEntityModule::", FIXTURE + "::", 1)
                  for accessor in ACCESSORS]
        parts.append(body)
        parts += [definition(tree.read(VEHICLE_CPP), signature) for signature in VEHICLE_BODIES]
        parts.append("}")
        parts += ["namespace BrnDirector { namespace Camera {",
                  definition(tree.read(CAMERA_CPP), "Vector3 Camera::GetPosition() const"), "} }"]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTraffic5Collidable.cpp"), "collidable.inc",
                           "\n".join(parts), "FxTraffic5Collidable",
                           extra_sources=[STRSTREAM_CPP, VOLUME_INSTANCE_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic5_collidable", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
