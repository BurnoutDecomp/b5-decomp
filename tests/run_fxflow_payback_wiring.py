"""FX-FLOW (crash parity 2026-09-24, NEW-PAYBACK-WIRING): the payback manager's input and round-end
wiring.

  (1) SendModeStopMessages @0x8234BEC0 `mr r4, r29` (== !lbOnlineLobbyHandover) @0x8234C6DC ->
      GameStateModule::OnModeEnd(bool) @0x823767E0, which hands r4 on untouched to
      MugshotManager::OnRoundEnd(bool) (0x823767F8) and PaybackManager::OnRoundEnd(bool) (0x82376800)
      before TakedownManager::ClearRaceCarData. The PC OnModeEnd took no argument and called neither.
  (3) PreWorldUpdate @0x823A5328 -> GameStateModule::CopyInputDataToPaybackManager @0x8239AA78 at
      0x823A572C, straight after DriveThruManager::Update: the frame's timer block (inlined
      PaybackManager::SetTimerInterface) and the controller's dirty-trick press (inlined
      SetDirtyTrickButtonState: press edge in aggressor state 4 with a trick -> ChangeState(5)).
      None of the three had a body, so the payback timer copy stayed at Clear() (aggressor timer
      frozen, frame-count reseed 0) and the dirty-trick button never reached the FSM.
  (4) PaybackManager::Update's tail vcall is slot 0 of PaybackDebugComponent's vtable @0x820CDDCC ==
      0x8284CB38, the ICF-folded bare `blr`: behaviour-identical to leave out (documented in Update).

Numeric: tests/FxFlowPaybackWiring.cpp compiles the extracted CopyInputDataToPaybackManager and the
two setters (labelled empty stand-ins where a revision lacks them) with the extracted aggressor
ChangeState / ResetState / OnRoundStart against the real PaybackManager and pre-world buffer types.
Wiring: OnModeEnd's two round ends, the caller's bool, and the PreWorldUpdate call position.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_payback_wiring.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report, REPO, STRSTREAM_CPP

PAYBACK_CPP = "src/GameSource/GameState/PaybackManager/BrnPaybackManager.cpp"
GTD_CPP = "src/GameSource/GameState/GameStateModule_gTD_00.cpp"
GUI_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
START_CPP = "src/GameSource/GameState/ModeManager/BrnModeManager_Start.cpp"
EXTRA = [STRSTREAM_CPP] + [REPO / p for p in (
    "src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp",
    "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",
    "src/GameShared/GameClasses/Module/VariableEventQueue_18432_16.cpp",
)]
NUMERIC_CHECKS = 11

REQUIRED = [
    "    void\n    PaybackManager::ChangeState(EPaybackAggressorState",
    "    void\n    PaybackManager::ResetState(",
    "    void\n    PaybackManager::OnRoundStart(",
]
OPTIONAL = [
    ("    void\n    PaybackManager::SetTimerInterface(",
     "void PaybackManager::SetTimerInterface(const CgsSystem::TimerStatusInterface*) {}"),
    ("    void\n    PaybackManager::SetDirtyTrickButtonState(",
     "void PaybackManager::SetDirtyTrickButtonState(bool) {}"),
]
COPY = "void GameStateModule::CopyInputDataToPaybackManager("
COPY_STAND_IN = ("void GameStateModule::CopyInputDataToPaybackManager("
                 "const GameStateModuleIO::PreWorldInputBuffer*) {}")


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    on_mode_end = squash(body_or_empty(tree.read(GTD_CPP), "void GameStateModule::OnModeEnd("))
    mug = on_mode_end.find("mpMugshotManager->OnRoundEnd(lbResetState);")
    pay = on_mode_end.find("mpPaybackManager->OnRoundEnd(lbResetState);")
    clear = on_mode_end.find("ClearTakedownRaceCarData();")
    yield ("OnModeEnd(bool) runs MugshotManager::OnRoundEnd then PaybackManager::OnRoundEnd with its "
           "bool, before ClearRaceCarData (0x823767F8 / 0x82376800 / 0x82376808)",
           on_mode_end.startswith("voidGameStateModule::OnModeEnd(boollbResetState)") and 0 <= mug < pay < clear)
    start = squash(tree.read(START_CPP))
    yield ("SendModeStopMessages passes !lbOnlineLobbyHandover to OnModeEnd (`mr r4, r29` @0x8234C6DC)",
           "mpGameStateModule->OnModeEnd(!lbOnlineLobbyHandover);" in start)
    bring_up = squash(body_or_empty(tree.read(GUI_CPP), "void GameStateModule::PreWorldUpdateStuntBringUp("))
    drive = bring_up.find("mDriveThruManager.Update(")
    copy = bring_up.find("CopyInputDataToPaybackManager(mpPreWorldInputBuffer);")
    rumble = bring_up.find("mRumbleManager.Update(")
    yield ("PreWorldUpdate calls CopyInputDataToPaybackManager(pre-world buffer) right after the drive-thru "
           "tick and before the rumble producers (0x823A5708 -> 0x823A572C -> 0x823A5800)",
           0 <= drive < copy < rumble)


def numeric(tree):
    payback = tree.read(PAYBACK_CPP)
    parts, stood_in = [], []
    try:
        parts.append(definition(payback, "    namespace\n    {"))
    except ValueError:
        print("NUMERIC: cannot build -- the anonymous-namespace records are absent")
        return None
    for signature in REQUIRED:
        try:
            parts.append(definition(payback, signature))
        except ValueError:
            print("NUMERIC: cannot build -- production body absent: " + signature.strip())
            return None
    for signature, stand_in in OPTIONAL:
        try:
            parts.append(definition(payback, signature))
        except ValueError:
            parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    try:
        copy = definition(tree.read(GTD_CPP), COPY)
    except ValueError:
        copy = "// [stand-in: body absent in this revision]\n" + COPY_STAND_IN
        stood_in.append("CopyInputDataToPaybackManager")
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    inc = "namespace BrnGameState {\n" + "\n".join(parts) + "\n" + copy + "\n}\n"
    return compile_and_run(Path(__file__).with_name("FxFlowPaybackWiring.cpp"), "payback_wiring.inc", inc,
                           "FxFlowPaybackWiring", extra_sources=EXTRA)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_payback_wiring", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
