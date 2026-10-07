"""Original replay pre-sim driver with actual IO/serialiser/timer bodies.

The genuine ReplayModule producer remains an explicitly observed subsystem
boundary here. --wrong-timer and --drop-actions mutate temporary driver text.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, code_only, compile_and_run, report, REPO, STRSTREAM_CPP
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
os.environ.pop("BRN_IOBUF_ZERO", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--wrong-timer", action="store_true")
parser.add_argument("--drop-actions", action="store_true")
args = parser.parse_args()
tree = Tree()
game = tree.read("src/GameSource/Game/BrnGameModule.cpp")
driver = definition(game, "void BrnGameModule::DoUpdate_ReplaysPreSim(")
if args.wrong_timer:
    driver = driver.replace("mTimerStatusInterface.GetSimTimerStatus(), 24", "mTimerStatusInterface.GetGameTimerStatus(), 24")
if args.drop_actions:
    driver = driver.replace("        lpReplayInput->AppendGameActionQueue(lpGameStateOutput->GetGameActionQueue());\n", "")
replay = tree.read("src/GameSource/Replays/BrnReplayModuleIO.cpp")
input_body = "\n".join(definition(replay, signature) for signature in (
    "void InputBuffer_PreSim::Construct()", "int InputBuffer_PreSim::AppendGameActionQueue(",
    "void InputBuffer_PreSim::SetTimerStatusInterface(",
    "const CgsSystem::TimerStatusInterface* InputBuffer_PreSim::GetTimerStatusInterface() const"))
gs = definition(tree.read("src/GameSource/GameState/BrnGameStateModuleIO.cpp"),
    "const GameActionQueue* OutputBuffer::GetGameActionQueue() const")
body = "namespace BrnReplays { namespace ReplayIO {\n" + input_body + "\n} }\n"
body += "namespace BrnGameState { namespace GameStateModuleIO {\n" + gs + "\n} }\n"
body += "namespace BrnGame {\n" + driver + "\n}\n"
sources = ["GameShared/GameClasses/Module/CgsIOBuffer.cpp", "GameShared/GameClasses/Module/CgsIOBufferStack.cpp",
    "GameSource/Replays/BrnReplayBaseSerialiser.cpp", "GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.cpp",
    "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp", "GameShared/GameClasses/Core/CgsStringUtils.cpp"]
result = compile_and_run(Path(__file__).with_name("GameReplayPreSimDriver.cpp"),
    "game_replay_presim_driver.inc", body, "GameReplayPreSimDriver",
    extra_sources=[REPO / "src" / path for path in sources] + [STRSTREAM_CPP])
wiring = [("actual game module constructs its canonical serialiser before updates",
    "mGameModuleSerialiser.Construct();" in code_only(definition(game, "void BrnGameModule::Construct("))),
    ("actual game module embeds the canonical serialiser type",
    "BrnReplays::GameModuleSerialiser mGameModuleSerialiser;" in code_only(tree.read("src/GameSource/Game/BrnGameModule.hpp")))]
raise SystemExit(report("run_game_replay_presim_driver", wiring, result, 1))
