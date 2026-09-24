"""FX-AIBUZZ item 1 (crash parity 2026-09-24): free-roam buzz-by placement.

BrnAI::BuzzBy::MaintainAheadOrBehind @0x82766C40 had NO BODY (declaration only), so
RaceCarEntityModule::PlaceRaceCarOnLoad's ARM A1 (0x822CE780..0x822CE7FC) -- a free-roam rival that streams
in within 250 m of the player -- was a named park that requested nothing and left the car in E_STATE_WAITING.
The function is a static leaf (request in r3, five vectors in v1..v5); PlaceRaceCarOnLoad calls it with the
car's pose, the player's pose / velocity / heading, then RequestResetOnTrack(+4 speed, +0xC type, +8 distance).
The same wave removed the function's invented guards (the console tests none: lpRaceCar, lpActiveRaceCar x4,
the player index, miOpponentCount) and put the DWARF names on BuzzBy's two .bss speeds
(KF_ON_COMING_RESET_SPEED = flt_8300DBEC, KF_FASTER_THAN_PLAYER = flt_8300D7F4; PS3 0x9CE24C / 0x9CE620).

  1. WIRING -- the header declares the leaf static, the body exists, ARM A1 calls it in the console's order
     and resets with the request's fields, the park and the guards are gone, ChooseAheadOrBehind names its
     speeds as the PS3 twin does.
  2. NUMERIC -- tests/FxAiBuzzMaintain.cpp runs the extracted production body (constants + IsZero helper)
     through the real ResetOnTrackRequest::Construct against the asm's four stores, bit for bit.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_maintain.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, body_or_empty, code_only, compile_and_run, definition, report

BUZZBY_CPP = "src/GameSource/World/AI/BrnAIBuzzBy.cpp"
BUZZBY_H = "src/GameSource/World/AI/BrnAIBuzzBy.h"
RCEM_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
REQUEST_CPP = REPO / "src/GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.cpp"
PLACE = "void RaceCarEntityModule::PlaceRaceCarOnLoad("
NUMERIC_CHECKS = 51


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    header = squash(tree.read(BUZZBY_H))
    yield ("BrnAIBuzzBy.h: MaintainAheadOrBehind is static (no `this`: the request is r3, 0x822CE7CC)",
           "staticvoidMaintainAheadOrBehind(" in header)

    buzz = tree.read(BUZZBY_CPP).replace("\r\n", "\n")
    yield ("BrnAIBuzzBy.cpp: MaintainAheadOrBehind @0x82766C40 has a body",
           body_or_empty(buzz, "    void BuzzBy::MaintainAheadOrBehind(") != "")
    choose = squash(body_or_empty(buzz, "    void BuzzBy::ChooseAheadOrBehind("))
    yield ("ChooseAheadOrBehind: BEHIND speed is KF_FASTER_THAN_PLAYER + speed, -60 (0x827719DC; PS3 0x9CE394)",
           "KF_FASTER_THAN_PLAYER+lfPlayerSpeed,-60.0f," in choose)
    yield ("ChooseAheadOrBehind: AHEAD is KF_ON_COMING_RESET_SPEED at 200 (0x827719AC; PS3 0x9CE2FC)",
           "KF_ON_COMING_RESET_SPEED,200.0f," in choose)

    place = squash(body_or_empty(tree.read(RCEM_CPP).replace("\r\n", "\n"), PLACE))
    yield ("PlaceRaceCarOnLoad A1: MaintainAheadOrBehind(&req, carPos, carDir, playerPos, playerVel, playerDir) "
           "(v1..v5 @0x822CE7D0..0x822CE7E0)",
           "BrnAI::BuzzBy::MaintainAheadOrBehind(&lRequest,lCarPosition,lCarDirection,lPlayerPosition,"
           "lPlayerVelocity,lPlayerDirection);" in place)
    getters = ["lPlayerDirection=lpPlayerCar->GetDirection();", "lPlayerVelocity=lpPlayerCar->GetVelocity();",
               "lPlayerPosition=lpPlayerCar->GetPosition();", "lCarDirection=lpRaceCar->GetDirection();",
               "lCarPosition=lpRaceCar->GetPosition();"]
    yield ("PlaceRaceCarOnLoad A1: the five inputs are the console's getters (ActiveRaceCar player x3, "
           "RaceCar car x2, 0x822CE784..0x822CE7C4)",
           all(getter in place for getter in getters))
    yield ("PlaceRaceCarOnLoad A1: RequestResetOnTrack(+4 speed, +0xC type, +8 distance) (0x822CE7E8..0x822CE7FC)",
           "lpRaceCar->RequestResetOnTrack(lRequest.GetResetSpeed(),lRequest.GetResetType(),"
           "lRequest.GetResetDistance());" in place)
    yield ("PlaceRaceCarOnLoad A1: the named park is gone", "A1-buzzBy-PARKED" not in place)
    yield ("PlaceRaceCarOnLoad: no lpRaceCar null test / early-out (the console uses r4 straight away)",
           "lpRaceCar==0" not in place and "lpRaceCar!=0" not in place)
    yield ("PlaceRaceCarOnLoad: no lpActiveRaceCar null test (r29 unguarded: 0x822CE684, 0x822CE9BC)",
           "lpActiveRaceCar!=0" not in place and "lpActiveRaceCar==0" not in place)
    yield ("PlaceRaceCarOnLoad: no player-index INVALID test (lwzx r4, 0x182F8 straight into GetActiveRaceCar)",
           "E_ACTIVE_RACE_CAR_INDEX_INVALID" not in place and "lpPlayerCar==0" not in place)
    yield ("PlaceRaceCarOnLoad B3: the miOpponentCount divide is unguarded (fdivs @0x822CE944)",
           "miOpponentCount!=0" not in place and "miOpponentCount==0" not in place)


def numeric(tree):
    source = tree.read(BUZZBY_CPP).replace("\r\n", "\n")
    parts = []
    for name in ("KF_FASTER_THAN_PLAYER", "KF_ON_COMING_RESET_SPEED"):
        match = re.search(r"^[ \t]*const f32 " + name + r"\s*=[^;]+;", source, re.M)
        if match is None:
            print(f"NUMERIC: cannot build -- no {name} in {BUZZBY_CPP}")
            return None
        parts.append(match.group(0))
    try:
        parts.append(definition(source, "    static inline bool IsZeroVmx("))
        parts.append(definition(source, "    void BuzzBy::MaintainAheadOrBehind("))
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxAiBuzzMaintain.cpp"), "fxaibuzz_maintain.inc",
                           "\n".join(parts) + "\n", "FxAiBuzzMaintain", extra_sources=(REQUEST_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxaibuzz_maintain", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
