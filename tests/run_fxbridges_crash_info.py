"""FX-BRIDGES (crash parity 2026-09-24) CC-6: BridgeWorldToDirector @0x823E3AB0 step 13, the PlayerCrashInfo producer.

Console 0x823E4DF0..0x823E4FE4: Construct the record; mbWrecked = IsPlayerWrecked() (lbz 0x28E0); mbHitWater =
HasCrashedIntoWater(GetPlayerActiveRaceCarIndex()); walk the vehicle manager's race-car crash queue (+0x3A0) and, for
every event whose entity word (muId >> 32) is the player's RaceCarState::mEntityId, copy speed (+0x34) / normal (+0x10)
/ contact point (+0x20) (last match wins) and set mbHardstopVsWall on owner 0 or mbHardStopVsAI on owner 1 / 2 (the
flags accumulate); publish 48 bytes into the director input at +0x78E0. PC never wrote the slot, so the director saw a
zeroed record for the whole session: no "Wrecked" treatment / wrecked exit, no drowning fade, no hard-stop verdicts.

Numeric: tests/FxBridgesCrashInfo.cpp compiles the PRODUCTION region of GameBridgeWorldToX.cpp (from
`BrnDirector::Camera::PlayerCrashInfo lPlayerCrashInfo;` through `lpDirectorInput->SetPlayerCrashInfo(&lPlayerCrashInfo);`)
against the real record / event / queue types. A revision without the region cannot build it: every numeric check
then counts as failed.
Wiring: the publish follows the boost leg inside BridgeWorldToDirector; InputBuffer::Construct clears the record
(0x82239470..0x822394B4); the getter returns the typed member.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_crash_info.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, REPO, code_only, definition, compile_and_run, report

BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"
INPUT_CPP = "src/GameSource/Director/DirectorModule/BrnDirectorModuleIOInputBuffer.cpp"
EXTRA_SOURCES = [
    REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",   # RaceCarState::Clear
]
NUMERIC_CHECKS = 17
REGION_START = "BrnDirector::Camera::PlayerCrashInfo lPlayerCrashInfo;"
REGION_END = "lpDirectorInput->SetPlayerCrashInfo(&lPlayerCrashInfo);"


def region(source):
    try:
        start = source.index(REGION_START)
        end = source.index(REGION_END, start) + len(REGION_END)
    except ValueError:
        return None
    return source[start:end] + "\n"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(BRIDGE_CPP)
    try:
        body = squash(definition(source, "void BrnGameModule::BridgeWorldToDirector("))
    except ValueError:
        body = ""
    boost = body.find("lpDirectorInput->SetPlayerBoostPercentage(lfBoostPercentage);")
    publish = body.find("lpDirectorInput->SetPlayerCrashInfo(&lPlayerCrashInfo);")
    yield ("step 13 publishes the crash record after the boost leg (0x823E4DF0 follows 0x823E4DDC)",
           0 <= boost < publish)
    inputs = tree.read(INPUT_CPP)
    try:
        construct = squash(definition(inputs, "    void InputBuffer::Construct()"))
    except ValueError:
        construct = ""
    yield ("InputBuffer::Construct clears the record (0x82239470..0x822394B4)",
           "mPlayerCrashInfo.Construct();" in construct)
    try:
        getter = squash(definition(inputs, "    const BrnDirector::Camera::PlayerCrashInfo* InputBuffer::GetPlayerCrashInfo() const"))
    except ValueError:
        getter = ""
    yield ("GetPlayerCrashInfo returns the typed member @0x78E0", "return&mPlayerCrashInfo;" in getter)


def numeric(tree):
    text = region(tree.read(BRIDGE_CPP))
    if text is None:
        print("NUMERIC: cannot build -- step 13 (the PlayerCrashInfo producer) is absent in this revision")
        return None
    return compile_and_run(Path(__file__).with_name("FxBridgesCrashInfo.cpp"),
                           "fxbridges_crash_info_region.inc", text, "FxBridgesCrashInfo",
                           extra_sources=EXTRA_SOURCES)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_crash_info", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
