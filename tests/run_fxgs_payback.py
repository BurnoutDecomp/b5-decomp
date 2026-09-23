"""FX-GS (crash parity 2026-09-23): BrnGameState::PaybackManager regressions.

  G12-D2   HandleTriggeringPayback @0x82397C08 writes the gui+0x40 "dirty trick triggered" record.
  G12-D3   ProcessDirtyTrickEventQueue @0x82383CA8 drains the inbound network queue (landed by the
           network wave, b5 e11310bd; kept here so it cannot regress).
  G12-D8   HandleActivePayback @0x82397CC8 (Update victim arm [2]).
  G12-D9   HandleCrashDueToPayback @0x82397D80 (arm [3]).
  G12-D10  HandleSurvivingPayback @0x82397EA0 (arm [4]).
  G12-D12  Destruct @0x8236D110 (+ ResetState) and its GameStateModule::Destruct call @0x823755A0.

Numeric: tests/FxGsPayback.cpp compiled against the extracted production bodies. A body the
revision does not have is replaced by a labelled empty stand-in (the defect state: the console
calls it, the revision does nothing), so an old revision still builds and FAILS the checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_payback.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

PAYBACK_CPP = "src/GameSource/GameState/PaybackManager/BrnPaybackManager.cpp"
GUI_CPP = "src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp"
TIMER_CPP = "src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp"
GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
NUMERIC_CHECKS = 42

# Bodies every revision under test has (Update and what it calls).
REQUIRED = [
    "    void\n    PaybackManager::UpdateCountdown(",
    "    void\n    PaybackManager::SendNetworkDirtyTrickMessage(",
    "    void\n    PaybackManager::HandleWaitForPaybackAggressorToCrash(",
    "    void\n    PaybackManager::HandleHavingPayback(",
    "    void\n    PaybackManager::HandleTriggeringPayback(",
    "    void\n    PaybackManager::ProcessTakedownEvents(",
    "    void\n    PaybackManager::ProcessDirtyTrickEventQueue(",
    "    void\n    PaybackManager::Update(",
]
# Bodies this fix adds: (signature, labelled empty stand-in used when the revision lacks it).
OPTIONAL = [
    ("    void\n    PaybackManager::ResetState(", "void PaybackManager::ResetState() {}"),
    ("    void\n    PaybackManager::Destruct(", "void PaybackManager::Destruct() {}"),
    ("    void\n    PaybackManager::ChangeState(EPaybackVictimState",
     "void PaybackManager::ChangeState(EPaybackVictimState) {}"),
    ("    bool\n    PaybackManager::IsCountdownComplete(",
     "bool PaybackManager::IsCountdownComplete() { return false; }"),
    ("    void\n    PaybackManager::RemoveCountdown(", "void PaybackManager::RemoveCountdown() {}"),
    ("    void\n    PaybackManager::DirtyTrickTriggered(",
     "void PaybackManager::DirtyTrickTriggered(GameStateModuleIO::OutputBuffer*, ::EActiveRaceCarIndex,"
     " ::EActiveRaceCarIndex, BrnNetwork::EPaybackType) {}"),
    ("    void\n    PaybackManager::DirtyTrickEnding(",
     "void PaybackManager::DirtyTrickEnding(GameStateModuleIO::OutputBuffer*, ::EActiveRaceCarIndex,"
     " ::EActiveRaceCarIndex, BrnNetwork::EPaybackType, bool) {}"),
    ("    void\n    PaybackManager::HandleActivePayback(",
     "void PaybackManager::HandleActivePayback(GameStateModuleIO::OutputBuffer*) {}"),
    ("    void\n    PaybackManager::HandleCrashDueToPayback(",
     "void PaybackManager::HandleCrashDueToPayback(GameStateModuleIO::OutputBuffer*) {}"),
    ("    void\n    PaybackManager::HandleSurvivingPayback(",
     "void PaybackManager::HandleSurvivingPayback(GameStateModuleIO::OutputBuffer*) {}"),
]
# Aggressor arms [2]/[3] and victim arm [1] (FX-GS2 G12-D5/D6/D7, 7da0efff / b66d479f / 5a13b69c): Update
# has called them since those fixes. Their behaviour -- with the production RNG and every callee -- is
# covered by run_fxgs2_payback.py; this runner keeps its 42 checks on the arms it was written for, so the
# three are ALWAYS empty stand-ins here (not extracted).
ALWAYS_STAND_IN = [
    "void PaybackManager::HandleWaitingToAwardPayback(const BrnPhysics::Vehicle::VehicleOutputInterface*) {}",
    "void PaybackManager::HandleAwardingPayback(GameStateModuleIO::OutputBuffer*,"
    " const BrnPhysics::Vehicle::VehicleOutputInterface*, GameStateModuleIO::EGameModeType) {}",
    "void PaybackManager::HandleReceivingPayback(GameStateModuleIO::OutputBuffer*,"
    " const BrnPhysics::Vehicle::VehicleOutputInterface*) {}",
]
GUI_REQUIRED = ["void GameStateToGuiInterface::Construct()"]
GUI_OPTIONAL = [
    ("void GameStateToGuiInterface::AddDirtyTrickEnding(",
     "void GameStateToGuiInterface::AddDirtyTrickEnding(::EActiveRaceCarIndex, ::EActiveRaceCarIndex,"
     " BrnNetwork::EPaybackType, bool) {}"),
    ("void GameStateToGuiInterface::AddDirtyTrickTriggered(",
     "void GameStateToGuiInterface::AddDirtyTrickTriggered(::EActiveRaceCarIndex, ::EActiveRaceCarIndex,"
     " BrnNetwork::EPaybackType) {}"),
]


def wiring(tree):
    destruct = body_or_empty(tree.read(GSM_CPP), "void GameStateModule::Destruct()")
    call = destruct.find("mpPaybackManager->Destruct()")
    base = destruct.find("CgsModule::ModuleSingleBuffered::Destruct()")
    yield ("D12 GameStateModule::Destruct calls PaybackManager::Destruct (0x823755A0) before "
           "ModuleSingleBuffered::Destruct (0x82375694)", 0 <= call < base)
    update = body_or_empty(tree.read(PAYBACK_CPP), "    void\n    PaybackManager::Update(")
    for state, handler, address in (("ACTIVE", "HandleActivePayback", "0x8239AD4C"),
                                    ("YOU_CRASHED", "HandleCrashDueToPayback", "0x8239AD5C"),
                                    ("YOU_SURVIVED", "HandleSurvivingPayback", "0x8239AD6C")):
        arm = re.search(r"case\s+E_PAYBACK_VICTIM_STATE_" + state + r"\s*:\s*" + handler
                        + r"\(\s*lpOutput\s*\)\s*;\s*break\s*;", update)
        yield (f"Update victim case {state} calls {handler}(lpOutput) ({address})", arm is not None)


def numeric(tree):
    source = tree.read(PAYBACK_CPP)
    gui = tree.read(GUI_CPP)
    parts, missing, stood_in = [], [], []
    try:
        parts.append(definition(source, "    namespace\n    {"))
    except ValueError:
        missing.append("the anonymous-namespace records")
    for signature in REQUIRED:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            missing.append(signature.split("::", 1)[1].strip())
    for signature, stand_in in OPTIONAL:
        try:
            parts.append(definition(source, signature))
        except ValueError:
            parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    for stand_in in ALWAYS_STAND_IN:
        parts.append("// [stand-in: covered by run_fxgs2_payback.py]\n" + stand_in)
    gui_parts = []
    for signature in GUI_REQUIRED:
        try:
            gui_parts.append(definition(gui, signature))
        except ValueError:
            missing.append(signature)
    for signature, stand_in in GUI_OPTIONAL:
        try:
            gui_parts.append(definition(gui, signature))
        except ValueError:
            gui_parts.append("// [stand-in: body absent in this revision]\n" + stand_in)
            stood_in.append(signature.split("::", 1)[1].strip())
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return None
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    timer = tree.read(TIMER_CPP)
    inc = ("namespace BrnGameState {\n" + "\n".join(parts) + "\n}\n"
           + "namespace BrnGameState { namespace GameStateModuleIO {\n" + "\n".join(gui_parts) + "\n} }\n"
           + definition(timer, "void\nCgsSystem::TimerStatusInterface::Clear()") + "\n")
    return compile_and_run(Path(__file__).with_name("FxGsPayback.cpp"), "payback_methods.inc", inc,
                           "FxGsPayback", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs_payback", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
