"""Complete original GS GUI bridge; actual IO/data, observed translator boundaries.

Controls alter extracted source only: omitted source append, checkpoint arm or
the wrong NaN polarity previously caught during the assembly audit.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report, REPO, STRSTREAM_CPP
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--drop-source", action="store_true")
parser.add_argument("--drop-checkpoints", action="store_true")
parser.add_argument("--wrong-nan", action="store_true")
args = parser.parse_args()
tree = Tree()
body = definition(tree.read("src/GameSource/Game/BrnGameModule.cpp"), "void BrnGameModule::BridgeGameStateToGui(")
if args.drop_source:
    body = body.replace("        lpGuiInput->GetGuiEvents()->Append<18432, 16>(*lpGameStateOutput->GetGuiEventQueue());\n", "")
if args.drop_checkpoints:
    body = body.replace("lStatus.miNumRemainingCheckpoints = lpScoring->maCarCheckpointData[liRunner]\n"
                        "                .GetAllRemainingCheckpointIndexes(lStatus.maiRemainingCheckpointIndexes);",
                        "lStatus.miNumRemainingCheckpoints = 0;")
if args.wrong_nan:
    body = body.replace("lpScoring->mfModeTimeRemaining >= 0.0f\n"
                        "                    ? lpScoring->mfModeTimeRemaining : 0.0f;",
                        "lpScoring->mfModeTimeRemaining < 0.0f\n"
                        "                    ? 0.0f : lpScoring->mfModeTimeRemaining;")
getters_source = tree.read("src/GameSource/GameState/BrnGameStateModuleIO.cpp")
getters = "\n".join(definition(getters_source, signature) for signature in (
    "const OutputBufferGuiEventQueue* OutputBuffer::GetGuiEventQueue() const",
    "const ScoringOutputInterface* OutputBuffer::GetScoringOutputInterface() const",
    "const OnlineScoringOutputInterface* OutputBuffer::GetOnlineScoringOutputInterface() const",
    "const TakedownEventOutputQueueType* OutputBuffer::GetTakedownEventOutputQueue() const",
    "const GameStateToGuiInterface* OutputBuffer::GetGameStateToGuiInterface() const",
    "bool OutputBuffer::GetSetUpAllEventStartsInterfaceIsValid() const",
    "const SetUpAllEventStartsInterface& OutputBuffer::GetSetUpAllEventStartsInterface() const",
    "bool OutputBuffer::GetSpecificGameModeEventInterfaceIsValid() const",
    "const SpecificGameModeEventInterface& OutputBuffer::GetSpecificGameModeEventInterface() const"))
body = "namespace BrnGame {\n" + body + "\n}\n"
body += "namespace BrnGameState { namespace GameStateModuleIO {\n" + getters + "\n} }\n"
sources = ["GameSource/GameState/BrnGameStateSharedIO.cpp",
    "GameShared/GameClasses/Gui/CgsGuiModuleIO.cpp", "GameShared/GameClasses/Module/CgsIOBuffer.cpp",
    "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp"]
result = compile_and_run(Path(__file__).with_name("GameStateGuiBridge.cpp"),
    "game_state_gui_bridge.inc", body,
    "GameStateGuiBridge", extra_sources=[REPO / "src" / path for path in sources] + [STRSTREAM_CPP])
raise SystemExit(report("run_game_state_gui_bridge", [], result, 1))
