"""Real GS GUI queue and scoring layout prerequisite; no device or game launch.

Executes original initialization blocks/accessors and actual checkpoint methods.
Other GameState OutputBuffer interfaces remain outside this isolated fixture.
Controls change temporary production text only, and never invoke a poisoned queue.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--omit-queue", action="store_true")
parser.add_argument("--omit-mode", action="store_true")
args = parser.parse_args()
tree = Tree()
source = tree.read("src/GameSource/GameState/BrnGameStateModuleIO.cpp")
construct = definition(source, "void OutputBuffer::Construct()")
queue = "    mGuiEventQueue.Construct();"
start = construct.index("    std::memset(&mScoringOutputInterfaceStorage")
stop = construct.index("    //     8 x s32 = -1", start)
scoring = construct[start:stop]
if args.omit_queue:
    queue = ""
if args.omit_mode:
    scoring = scoring.replace(
        "    reinterpret_cast<ScoringOutputInterface*>(&mScoringOutputInterfaceStorage)->meGameModeType = E_MODE_NONE;", "")
initialize = """void InitializeGuiScoring(OutputBuffer* output)
{
    auto& mGuiEventQueue = output->mGuiEventQueue;
    auto& mScoringOutputInterfaceStorage = output->mScoringOutputInterfaceStorage;
""" + queue + "\n" + scoring + "\n}\n"
getters = "\n".join(definition(source, signature) for signature in (
    "OutputBufferGuiEventQueue* OutputBuffer::GetGuiEventQueue()",
    "const OutputBufferGuiEventQueue* OutputBuffer::GetGuiEventQueue() const",
    "\nScoringOutputInterface* OutputBuffer::GetScoringOutputInterface()\n",
    "const ScoringOutputInterface* OutputBuffer::GetScoringOutputInterface() const"))
body = tree.read("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
body += "\nnamespace BrnGameState { namespace GameStateModuleIO {\n" + initialize + getters + "\n} }\n"
result = compile_and_run(Path(__file__).with_name("GameStateGuiIO.cpp"),
    "game_state_gui_io.inc", body, "GameStateGuiIO", extra_sources=[STRSTREAM_CPP,
    REPO / "src/GameSource/GameState/BrnGameStateSharedIO.cpp"])
raise SystemExit(report("run_game_state_gui_io", [], result, 1))
