"""FX-GS2 (crash parity 2026-09-23): BrnGameState::PaybackManager -- the arms that poll
BrnPhysics::Vehicle::CrashingRaceCarInterface::IsCrashing.

  G12-D1   HandleWaitForPaybackAggressorToCrash @0x823977F0: the pinned `false` becomes the
           console's IsCrashing(player) read (0x82397848 lbzx), so aggressor state 1 -> 2.
  G12-D5   HandleWaitingToAwardPayback @0x823978B0 (Update arm 0x8239AC2C[2] = 0x8239AC54): once the
           player is no longer crashing, ChangeState(3) -- state 2 -> 3.
  G12-D6   HandleAwardingPayback @0x82397970 (arm [3] = 0x8239AC64) + DirtyTrickAwarded (inlined at
           0x82397A40..64) + GameStateToGuiInterface::AddNewDirtyTrick: the timer >= 1.0 award (RandomInt
           draw, gui+4 record, AVAILABLE network message through Update's tail Append) and the
           independent crash -> PaybackLostAction (0xD3) -> idle arm.

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
RANDOM_CPP = "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
NUMERIC_CHECKS = 38

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
OPTIONAL = [
    ("    void\n    PaybackManager::HandleWaitingToAwardPayback(",
     "void PaybackManager::HandleWaitingToAwardPayback(const BrnPhysics::Vehicle::VehicleOutputInterface*) {}"),
    ("    void\n    PaybackManager::DirtyTrickAwarded(",
     "void PaybackManager::DirtyTrickAwarded(GameStateModuleIO::OutputBuffer*, ::EActiveRaceCarIndex,"
     " ::EActiveRaceCarIndex, BrnNetwork::EPaybackType) {}"),
    ("    void\n    PaybackManager::HandleAwardingPayback(",
     "void PaybackManager::HandleAwardingPayback(GameStateModuleIO::OutputBuffer*,"
     " const BrnPhysics::Vehicle::VehicleOutputInterface*, GameStateModuleIO::EGameModeType) {}"),
]
GUI_REQUIRED = [
    "void GameStateToGuiInterface::Construct()",
    "void GameStateToGuiInterface::AddDirtyTrickTriggered(",
    "void GameStateToGuiInterface::AddDirtyTrickEnding(",
]
GUI_OPTIONAL = [
    ("void GameStateToGuiInterface::AddNewDirtyTrick(",
     "void GameStateToGuiInterface::AddNewDirtyTrick(::EActiveRaceCarIndex, ::EActiveRaceCarIndex,"
     " BrnNetwork::EPaybackType) {}"),
]


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
    arm = re.search(r"case\s+E_PAYBACK_AGGRESSOR_STATE_AWARD_DT\s*:\s*"
                    r"HandleWaitingToAwardPayback\(\s*lpVehicleOutputInterface\s*\)\s*;\s*break\s*;", update)
    yield ("D5 Update aggressor case 2 calls HandleWaitingToAwardPayback(vehicle output) "
           "(0x8239AC54: mr r4, r21)", arm is not None)
    arm = re.search(r"case\s+E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER\s*:\s*"
                    r"HandleAwardingPayback\(\s*lpOutput\s*,\s*lpVehicleOutputInterface\s*,\s*leGameModeType\s*\)"
                    r"\s*;\s*break\s*;", update)
    yield ("D6 Update aggressor case 3 calls HandleAwardingPayback(out, vehicle output, mode) "
           "(0x8239AC64: r4 = r22, r5 = r21, r6 = r29)", arm is not None)


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
    # The award's draw is the production RNG (RandomInt -> RandomUInt, CgsRandom.cpp), extracted
    # from the same revision with the LCG multiplier constant it steps by.
    random = tree.read(RANDOM_CPP)
    multiplier = re.search(r"static const u64 KU_RANDOM_LCG_MULTIPLIER\s*=\s*[^;]+;", random)
    if multiplier is None:
        print("NUMERIC: cannot build -- CgsRandom.cpp's KU_RANDOM_LCG_MULTIPLIER is absent")
        return None
    rng = ("namespace CgsNumeric {\n" + multiplier.group(0) + "\n"
           + definition(random, "    u32 Random::RandomUInt()") + "\n"
           + definition(random, "    s32 Random::RandomInt(") + "\n}\n")
    inc = ("namespace BrnGameState {\n" + "\n".join(parts) + "\n}\n"
           + "namespace BrnGameState { namespace GameStateModuleIO {\n" + "\n".join(gui_parts) + "\n} }\n"
           + definition(timer, "void\nCgsSystem::TimerStatusInterface::Clear()") + "\n" + rng)
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
