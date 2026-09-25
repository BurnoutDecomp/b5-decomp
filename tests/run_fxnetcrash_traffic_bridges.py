"""crash parity FX-NETCRASH (2026-09-25): leg 13's traffic -> sound and traffic -> director records.

WorldModule::BridgeEntityModulesToOutput_PostPhysics copies three traffic interfaces into the world update-output
buffer, one after the other:
  0x827AF0D4..0x827AF0E4  SetTrafficNetworkOutputInterface(trafficOut->GetNetworkInterface())            (33e843c6)
  0x827AF0F8..0x827AF108  SetTrafficSoundOutputInterface(trafficOut->GetTrafficSoundOutputInterface())
  0x827AF11C..0x827AF12C  SetTrafficDirectorOutputInterface(trafficOut->GetTrafficDirectorOutputInterface())
The records are this frame's nearby traffic (TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults).
BrnGameModule::BridgeWorldToSound @0x823CD580 hands the world's sound copy to the sound module, where
CollisionStateManager::FindEntity @0x826A0398 / MapEntityIdToMaterial @0x826A0CF8 look traffic cars up in it.
Without the two siblings the sound module read an empty list: no traffic car was ever found.

NUMERIC: the production leg-13 statements are compiled into FxNetcrashTrafficBridges.cpp against a fake traffic
output holding the real record types and a fake world output whose setters copy like the real ones.
WIRING: the leg order in the bridge, the producer's call, the sound hop, and the director array's const accessor.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_traffic_bridges.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

BRIDGE_CPP = "src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp"
TRAFFIC_IO_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.cpp"
MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
GAME_BRIDGE_CPP = "src/GameSource/Game/GameBridgeWorldToX.cpp"
SOUND_INTERFACES_CPP = REPO / "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.cpp"

BRIDGE_SIGNATURE = "void BridgeEntityModulesToOutput_PostPhysics("
FIRST = "lpOutputBuffer->SetTrafficNetworkOutputInterface("
ACCESSOR_SIGNATURE = ("const Array<TrafficDirectorEntity, 32u>& "
                      "TrafficDirectorOutputInterface::GetTrafficDirectorEntityArray() const")
NUMERIC_CHECKS = 9


def leg13(bridge):
    body = definition(bridge, BRIDGE_SIGNATURE)
    start = body.index(FIRST)
    end = body.rindex("}")
    return body[start:end]


def numeric(tree):
    bridge = tree.read(BRIDGE_CPP).replace("\r\n", "\n")
    try:
        block = leg13(bridge)
    except ValueError as error:
        print("NUMERIC: cannot build -- production statements absent: " + str(error))
        return None
    io = tree.read(TRAFFIC_IO_CPP).replace("\r\n", "\n")
    try:
        accessor = definition(io, ACCESSOR_SIGNATURE)
    except ValueError:
        accessor = "// (this revision has no out-of-line body of the const accessor)\n"
    return compile_and_run(Path(__file__).with_name("FxNetcrashTrafficBridges.cpp"), "leg13.inc", block,
                           "FxNetcrashTrafficBridges", extra_sources=[STRSTREAM_CPP, SOUND_INTERFACES_CPP],
                           extra_files={"director_accessor.inc": accessor})


def position(text, pattern):
    match = re.search(pattern, text)
    return match.start() if match else -1


def wiring(tree):
    bridge = tree.read(BRIDGE_CPP).replace("\r\n", "\n")
    try:
        block = code_only(leg13(bridge))
    except ValueError:
        block = ""
    network_at = position(block, r"lpOutputBuffer->SetTrafficNetworkOutputInterface\(\s*"
                                 r"lpTrafficOutput_PostPhysics->GetNetworkInterface\(\)\s*\)")
    sound_at = position(block, r"lpOutputBuffer->SetTrafficSoundOutputInterface\(\s*"
                               r"lpTrafficOutput_PostPhysics->GetTrafficSoundOutputInterface\(\)\s*\)")
    director_at = position(block, r"lpOutputBuffer->SetTrafficDirectorOutputInterface\(\s*"
                                  r"lpTrafficOutput_PostPhysics->GetTrafficDirectorOutputInterface\(\)\s*\)")
    try:
        post = code_only(definition(tree.read(MODULE_CPP).replace("\r\n", "\n"),
                                    "void TrafficEntityModule::PostPhysicsUpdate("))
    except ValueError:
        post = ""
    try:
        to_sound = code_only(definition(tree.read(GAME_BRIDGE_CPP).replace("\r\n", "\n"),
                                        "void BrnGameModule::BridgeWorldToSound("))
    except ValueError:
        to_sound = ""
    io = tree.read(TRAFFIC_IO_CPP).replace("\r\n", "\n")
    try:
        accessor = code_only(definition(io, ACCESSOR_SIGNATURE))
    except ValueError:
        accessor = ""
    return [
        ("leg 13: network, then SetTrafficSoundOutputInterface(GetTrafficSoundOutputInterface()) 0x827AF0F8..0x827AF108, "
         "then SetTrafficDirectorOutputInterface(GetTrafficDirectorOutputInterface()) 0x827AF11C..0x827AF12C",
         0 <= network_at < sound_at < director_at),
        ("the producer runs in PostPhysicsUpdate: ProcessNearbyTrafficSceneQueryResults(lpInput, lpOutput)",
         "ProcessNearbyTrafficSceneQueryResults(lpInput, lpOutput)" in post),
        ("the sound hop exists: BridgeWorldToSound installs the world's traffic sound records (0x823CD5D0..0x823CD5E0)",
         re.search(r"SetTrafficOutputInterface\([^;]*lpWorldOutputBuffer->GetTrafficSoundOutputInterface\(\)",
                   to_sound, re.S) is not None),
        ("TrafficDirectorOutputInterface::GetTrafficDirectorEntityArray() const has a body (DWARF :98, the "
         "interface + 0x10)", "return maActiveEntityArray;" in accessor),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_traffic_bridges", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
