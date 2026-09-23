"""FX-GS2 (crash parity 2026-09-23): BrnGameState::PaybackManager -- the arms that poll
BrnPhysics::Vehicle::CrashingRaceCarInterface::IsCrashing.

  G12-D1   HandleWaitForPaybackAggressorToCrash @0x823977F0: the pinned `false` becomes the
           console's IsCrashing(player) read (0x82397848 lbzx), so aggressor state 1 -> 2.

Numeric: tests/FxGs2Payback.cpp compiled against the extracted PRODUCTION bodies (Update and every
body it dispatches), driven through the real PaybackManager / GameStateToGuiInterface /
NetworkToGameStateInterface / GameStateToNetworkInterface types. A body the revision does not have
is replaced by a labelled empty stand-in (the defect state: the console calls it, the revision does
nothing), so an old revision still builds and FAILS the checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs2_payback.py [--rev <b5 rev>]
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
NUMERIC_CHECKS = 11

# Bodies every revision under test has: Update and everything it reaches.
REQUIRED = [
    "    void\n    PaybackManager::ChangeState(EPaybackAggressorState",
    "    void\n    PaybackManager::UpdateCountdown(",
    "    void\n    PaybackManager::ChangeState(EPaybackVictimState",
    "    bool\n    PaybackManager::IsCountdownComplete(",
    "    void\n    PaybackManager::RemoveCountdown(",
    "    void\n    PaybackManager::DirtyTrickTriggered(",
    "    void\n    PaybackManager::DirtyTrickEnding(",
    "    void\n    PaybackManager::SendNetworkDirtyTrickMessage(",
    "    void\n    PaybackManager::HandleWaitForPaybackAggressorToCrash(",
    "    void\n    PaybackManager::HandleHavingPayback(",
    "    void\n    PaybackManager::HandleTriggeringPayback(",
    "    void\n    PaybackManager::HandleActivePayback(",
    "    void\n    PaybackManager::HandleCrashDueToPayback(",
    "    void\n    PaybackManager::HandleSurvivingPayback(",
    "    void\n    PaybackManager::ProcessTakedownEvents(",
    "    void\n    PaybackManager::ProcessDirtyTrickEventQueue(",
    "    void\n    PaybackManager::Update(",
]
# Bodies a fix adds: (signature, labelled empty stand-in used when the revision lacks it).
OPTIONAL = []
GUI_REQUIRED = [
    "void GameStateToGuiInterface::Construct()",
    "void GameStateToGuiInterface::AddDirtyTrickTriggered(",
    "void GameStateToGuiInterface::AddDirtyTrickEnding(",
]
GUI_OPTIONAL = []


def wiring(tree):
    source = tree.read(PAYBACK_CPP)
    update = body_or_empty(source, "    void\n    PaybackManager::Update(")
    arm = re.search(r"case\s+E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK\s*:\s*"
                    r"HandleWaitForPaybackAggressorToCrash\(\s*lpVehicleOutputInterface\s*\)\s*;\s*break\s*;",
                    update)
    yield ("Update aggressor case 1 calls HandleWaitForPaybackAggressorToCrash(vehicle output) "
           "(0x8239AC44: mr r4, r21)", arm is not None)
    wait = body_or_empty(source, "    void\n    PaybackManager::HandleWaitForPaybackAggressorToCrash(")
    yield ("D1 HandleWaitForPaybackAggressorToCrash tests IsCrashing(player), not a pinned bool "
           "(0x82397848 lbzx)", "IsCrashing(" in wait and "lbPlayerIsCrashing" not in wait)


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
    return compile_and_run(Path(__file__).with_name("FxGs2Payback.cpp"), "payback2_methods.inc", inc,
                           "FxGs2Payback", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs2_payback", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
