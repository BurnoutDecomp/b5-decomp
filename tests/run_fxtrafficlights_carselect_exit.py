"""crash parity FX-TRAFFICLIGHTS (2026-09-25): the junkyard exit tells the traffic where the car comes out.

CarSelectManager::UpdateExitState @0x82398C20 posts game action 77, GameStateModuleIO::CarSelectExitAction (32 bytes:
the spawn vector at +0x00, mbOnlineCarSelect at +0x10). The console fills it from the exit spawn location:
  0x82398D40  lwz r29, 0x38(r30)            maSpawnLocations[4]
  0x82398D6C  lvx128 v0, r0, r29            its first 16 bytes, SpawnLocation::mPosition -> record +0x00
  0x82398D84  stb r31 (== 0) -> +0x10       mbOnlineCarSelect = false
  0x82398D70 / 0x82398D74 / 0x82398D88      AddEvent(record, 77, 32)
The PC posted 32 ZERO bytes (an "opaque AllowBoostEarningAction"). The traffic's HandleExternalRequests arm 77
(0x8274C068) clears the traffic within 150 m of mExitSpawnLocation, so on the PC it cleared the world origin instead of
the junkyard exit.

NUMERIC (FxTrafficLightsCarSelectExit.cpp): the PRODUCTION UpdateExitState and the file-local helpers it reads
(GameActionQueueImpl / AsActionQueue / KAC_CSM_FILE / KF_TIMER_RESET / the EGameAction id table /
KI_PAUSE_REASON_CAR_SELECT), compiled as members of a fixture carrying the manager's members with their real types and a
REAL VariableEventQueue<13312,16>. It checks the record's bytes, its id / size / position in the post order, and a
second exit from another spawn point.
WIRING: the post goes through CarSelectExitAction with mExitSpawnLocation = lpExitSpawnLocation->mPosition.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtrafficlights_carselect_exit.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

MANAGER_CPP = "src/GameSource/GameState/CarSelect/BrnCarSelectManager.cpp"
FIXTURE = "ExitFixture"
NUMERIC_CHECKS = 16


def statement(source, pattern):
    match = re.search(pattern, source, re.S)
    if match is None:
        raise ValueError("absent: " + pattern)
    return match[0]


def numeric(tree):
    source = tree.read(MANAGER_CPP).replace("\r\n", "\n")
    try:
        locals_text = "\n".join([
            statement(source, r"typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;"),
            definition(source, "static inline GameActionQueueImpl* AsActionQueue("),
            statement(source, r"static const char\* const KAC_CSM_FILE =[^;]*;"),
            statement(source, r"static const f32 KF_TIMER_RESET\s*=[^;]*;"),
            definition(source, "enum EGameAction") + ";",
            statement(source, r"static const s32 KI_PAUSE_REASON_CAR_SELECT\s*=[^;]*;"),
        ])
        body = definition(source, "void CarSelectManager::UpdateExitState(").replace(
            "CarSelectManager::", FIXTURE + "::", 1)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTrafficLightsCarSelectExit.cpp"), "exit_state_body.inc",
                           body, "FxTrafficLightsCarSelectExit", extra_sources=[STRSTREAM_CPP],
                           extra_files={"exit_state_locals.inc": locals_text})


def wiring(tree):
    source = tree.read(MANAGER_CPP).replace("\r\n", "\n")
    try:
        exit_state = code_only(definition(source, "void CarSelectManager::UpdateExitState("))
    except ValueError:
        exit_state = ""
    return [
        ("UpdateExitState posts action 77 as a GameStateModuleIO::CarSelectExitAction whose mExitSpawnLocation is the "
         "exit spawn location's mPosition (0x82398D6C) and whose mbOnlineCarSelect is false (0x82398D84)",
         "GameStateModuleIO::CarSelectExitAction" in exit_state
         and re.search(r"\.mExitSpawnLocation\s*=\s*lpExitSpawnLocation->mPosition;", exit_state) is not None
         and re.search(r"\.mbOnlineCarSelect\s*=\s*false;", exit_state) is not None),
        ("the 32 zero bytes of the old post are gone", "lacAllowBoost" not in exit_state),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtrafficlights_carselect_exit", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
