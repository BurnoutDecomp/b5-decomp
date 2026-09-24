"""FX-AIBUZZ item 2 (crash parity 2026-09-24): RCEntityActiveRaceCarOutputInterface::IsRaceCarPlayer @0x82681DF0.

The accessor was declaration-only (BrnRaceCarEntityModuleOutputInterface.h), so both of its console callers --
CollisionStateManager::FindEntity (0x826A055C) and TrafficEntityModule::UpdateParams_DoTimeSlicedLogic
(0x827441AC), found by scanning the image for every `bl 0x82681DF0` -- carried a stand-in,
`GetPlayerActiveRaceCarIndex() == idx`, which reads mePlayerActiveRaceCarIndex instead of the flag word. The
console reads bit 1 of maxRaceCarFlags[idx] (E_RACE_CAR_OUTPUT_FLAG_PLAYER): `addi r11, idx, 0x13C0 ; slwi 1 ;
lhzx ; srwi 1 ; clrlwi 31` (0x82681E54..0x82681E64) after the two index asserts.

  1. WIRING -- the body exists; FindEntity stores IsRaceCarPlayer(idx) into mbPlayer and the stand-in is gone.
     (The traffic caller is FX-TRAFFIC5's file; the conductor lands that hunk.)
  2. NUMERIC -- tests/FxAiBuzzRaceCarPlayer.cpp runs the extracted production body on the real interface
     struct: the bit, every slot's own element, and flags that disagree with the player index.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_race_car_player.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, compile_and_run, definition, report

IFACE_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
COLLISION_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
BODY = "bool RCEntityActiveRaceCarOutputInterface::IsRaceCarPlayer("
FIND = "bool CollisionStateManager::FindEntity("
NUMERIC_CHECKS = 19


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    iface = tree.read(IFACE_CPP).replace("\r\n", "\n")
    yield ("BrnRCEntityActiveRaceCarOutputInterface.cpp: IsRaceCarPlayer @0x82681DF0 has a body",
           body_or_empty(iface, BODY) != "")
    find = squash(body_or_empty(tree.read(COLLISION_CPP).replace("\r\n", "\n"), FIND))
    yield ("CollisionStateManager::FindEntity: mbPlayer = IsRaceCarPlayer(idx) (bl 0x82681DF0 @0x826A055C)",
           "lEntity.mbPlayer=lrVehicles.IsRaceCarPlayer(leIndex);" in find)
    yield ("CollisionStateManager::FindEntity: the GetPlayerActiveRaceCarIndex() == idx stand-in is gone",
           "GetPlayerActiveRaceCarIndex()" not in find)


def numeric(tree):
    source = tree.read(IFACE_CPP).replace("\r\n", "\n")
    try:
        body = definition(source, BODY)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxAiBuzzRaceCarPlayer.cpp"), "fxaibuzz_race_car_player.inc",
                           body + "\n", "FxAiBuzzRaceCarPlayer")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxaibuzz_race_car_player", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
