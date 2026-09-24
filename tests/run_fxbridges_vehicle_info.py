"""FX-BRIDGES (crash parity 2026-09-24): BridgeWorldToDirector @0x823E3AB0's per-car VehicleInfo build.

CC-5 -- the HARDEST-IMPACT leg (0x823E4884 seed, 0x823E4A18..0x823E4B24 walk): mHardestNormalStressNormal
seeded (0,1,0,0) from 0x82181510, stress 0, impact 0.0 (flt_82001CC0); while the world contact spy is bound,
this car's run (GetRaceCarContactRunList -> GetRunDataWithEntityID(mEntityId)) is walked and the contact
with the strictly greatest SQUARED |mNormalStress| (vmsum3fp128 + vcmpgtfp128 + vsel) publishes its
squared magnitude, normal and stress. PC published zeros for every car on every frame.

Numeric: tests/FxBridgesVehicleInfo.cpp compiles the PRODUCTION region of GameBridgeWorldToX.cpp (from
`BrnDirector::Camera::VehicleInfo lVehicleInfo;` up to the `lpDirectorInput->SetRaceCarInfo(` publish)
against the real VehicleInfo / RaceCarState / ContactSpyInterface / ContactSpyData.
Wiring: lpCarContacts is taken from the bound spy BEFORE the per-car loop (DWARF :87, 0x823E4010..0x823E4034).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_vehicle_info.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, REPO, code_only, definition, compile_and_run, report

BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"
EXTRA_SOURCES = [
    REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",        # RaceCarState Clear / operator=
    REPO / "src/GameSource/Physics/ContactSpies/BrnContactSpyInterface.cpp",             # GetRaceCarContactRunList
]
NUMERIC_CHECKS = 13
REGION_START = "BrnDirector::Camera::VehicleInfo lVehicleInfo;"
REGION_END = "lpDirectorInput->SetRaceCarInfo("


def region(source):
    """The per-car VehicleInfo build: the text from the lVehicleInfo declaration's line up to the publish."""
    try:
        start = source.index(REGION_START)
        end = source.index(REGION_END, start)
    except ValueError:
        return None
    start = source.rindex("\n", 0, start) + 1
    return source[start:end]


def wiring(tree):
    source = tree.read(BRIDGE_CPP)
    try:
        body = re.sub(r"\s+", "", code_only(definition(source, "void BrnGameModule::BridgeWorldToDirector(")))
    except ValueError:
        body = ""
    contacts = body.find("lpCarContacts=lpContactSpy->GetRaceCarContacts();")
    gate = body.find("if(lpContactSpy->IsValid()){lpCarContacts=")
    loop = body.find("for(s32liSlot=0;liSlot<E_ACTIVE_RACE_CAR_INDEX_COUNT;++liSlot)")
    yield ("lpCarContacts comes from the bound spy (IsValid gate) before the per-car loop "
           "(DWARF :87, 0x823E4010..0x823E4034)", 0 <= gate < contacts < loop)


def numeric(tree):
    text = region(tree.read(BRIDGE_CPP))
    if text is None:
        print("NUMERIC: cannot build -- the VehicleInfo region is absent in this revision")
        return None
    return compile_and_run(Path(__file__).with_name("FxBridgesVehicleInfo.cpp"),
                           "fxbridges_vehicle_info_region.inc", text, "FxBridgesVehicleInfo",
                           extra_sources=EXTRA_SOURCES)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_vehicle_info", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
