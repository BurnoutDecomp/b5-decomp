"""FX-FLOW (crash parity 2026-09-24): the drive-thru PAINT SHOP stop -- "Can not instance resource
pointer - it has no main memory resource" (CgsResourcePtr.h) then an access violation reading +0x20,
every time the player drove through a paint shop (scratch/bugtest/runs/fxtraffic_crash_release/
20260923_152313, BrnGame.log:59737).

Cause: GameStateModule's DWARF member mpPlayerCarColours (+0x456F0, BrnGameStateModule.h:845) did
not exist and Prepare's stages 11/12 were a log line, so the DONE stage handed
DriveThruManager::Prepare a default-constructed (null) ResourcePtr and ProcessDriveThru's paint arm
(the one arm that reads it: maPalettes[2].miNumColours, @0x8239BD0C) dereferenced null.
  GameStateModule::Prepare @0x8239E578 case 11 @0x8239E9D4: AcquireResource(&mReceiverQueue, 0, 5,
  "CarColours") on the output buffer's RequestInterface<3072> + Clear; case 12 @0x8239EA34: wait for a
  reply, CreateFromHandle(mpPlayerCarColours, reply+0x18); DONE @0x8239EC6C..0x8239EC88: the member
  goes to DriveThruManager::Prepare through the ResourcePtr copy constructor @0x82368FC8.

Numeric: tests/FxFlowCarColours.cpp compiles the extracted stage-11/12 text against the real request
interface, receiver queue, ResourcePtr and HashString bodies, with a fake pool reply.
Wiring: the member exists, the DONE stage passes it, and the paint arm has the console's unsigned
modulo with no invented guard.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_car_colours.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, REPO, STRSTREAM_CPP

GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
GSM_H = "src/GameSource/GameState/BrnGameStateModule.h"
DRIVETHRU_CPP = "src/GameSource/GameState/Offences/BrnDriveThruManager.cpp"
EXTRA = [STRSTREAM_CPP] + [REPO / "src/GameShared/GameClasses" / p for p in (
    "Module/CgsBaseEventReceiverQueue.cpp",
    "Module/VariableEventQueue_3072_16.cpp",
    "System/Resource/CgsResourcePtr.cpp",
    "System/Resource/CgsBaseResourcePtr.cpp",
    "System/Resource/CgsResourceID.cpp",
    "Core/CgsStringUtils.cpp",
)]
NUMERIC_CHECKS = 11

PREPARE = "bool GameStateModule::Prepare("
FIRST_CASE = "case E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS:"
NEXT_CASE = "case E_PREPARESTAGE_MODEMANAGER:"


def stage_text(tree):
    """Prepare's text from the stage-11 label up to (not including) the stage-13 label, comments out."""
    try:
        prepare = code_only(definition(tree.read(GSM_CPP), PREPARE))
    except ValueError:
        return None
    start = prepare.find(FIRST_CASE)
    end = prepare.find(NEXT_CASE, start)
    if start < 0 or end < 0:
        return None
    return prepare[start:end]


def wiring(tree):
    header = re.sub(r"\s+", " ", code_only(tree.read(GSM_H)))
    yield ("GameStateModule has the DWARF member ResourcePtr<BrnWorld::GlobalColourPalette> "
           "mpPlayerCarColours (+0x456F0, BrnGameStateModule.h:845)",
           "CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mpPlayerCarColours;" in header)
    try:
        prepare = re.sub(r"\s+", "", code_only(definition(tree.read(GSM_CPP), PREPARE)))
    except ValueError:
        prepare = ""
    yield ("the DONE stage hands DriveThruManager::Prepare the bound member, not a default-constructed "
           "ResourcePtr (0x8239EC6C..0x8239EC88)",
           "mDriveThruManager.Prepare(mTriggerQueryManager.GetTriggerData(),CgsResource::ResourcePtr<"
           "BrnWorld::GlobalColourPalette>(mpPlayerCarColours.GetResourceHandle()));" in prepare)
    try:
        process = re.sub(r"\s+", "", code_only(definition(tree.read(DRIVETHRU_CPP),
                                                          "void DriveThruManager::ProcessDriveThru(")))
    except ValueError:
        process = ""
    yield ("the paint arm advances the colour with the console's UNSIGNED modulo over "
           "maPalettes[2].miNumColours, no invented count guard (divwu/twllei @0x8239BD1C..2C)",
           "maPalettes[2].miNumColours" in process
           and "(static_cast<u32>(liColourIndex)+1u)%luNumColours" in process
           and "colourcount>0" not in process.lower())


def numeric(tree):
    stages = stage_text(tree)
    if stages is None:
        print("NUMERIC: cannot build -- Prepare's stage-11 / stage-13 labels are absent")
        return None
    inc = ("bool FixtureGameStateModule::Stages(FixtureOutputBuffer* lpOutputBuffer)\n{\n"
           "    switch (mePrepareStage)\n    {\n" + stages +
           "\n    case E_PREPARESTAGE_MODEMANAGER:\n        return true;\n    default:\n        break;\n    }\n"
           "    (void)lpOutputBuffer;\n    return false;\n}\n")
    return compile_and_run(Path(__file__).with_name("FxFlowCarColours.cpp"), "car_colours.inc", inc,
                           "FxFlowCarColours", extra_sources=EXTRA)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_car_colours", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
