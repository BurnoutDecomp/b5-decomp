"""Replay the production position overlay at independent simulation/render rates."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, code_only, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
race = tree.read("src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp")
world = tree.read("src/GameSource/World/BrnWorldModule.cpp")
physics = code_only(definition(race, "void RaceCarEntityModule::PostPhysicsUpdate("))
dispatch = code_only(definition(world, "WorldModule::GenerateDispatchLists("))
render = definition(race, "void RaceCarEntityModule::DebugRenderPosition() const")
code = ("constexpr bool kPhysicsSubmits = " + str("DebugRenderPosition();" in physics).lower() + ";\n"
        "constexpr bool kRenderSubmits = " + str("mRaceCarEntityModule.DebugRenderPosition();" in dispatch).lower() + ";\n"
        "namespace BrnWorld {\n" + render + "\n}\n")
call = dispatch.find("mRaceCarEntityModule.DebugRenderPosition();")
wiring = [("one submission in the world render pass after its world-render gate",
           call > dispatch.find("lpDispatchThreadInputBuffer->SetRendererFlags( 0 )") >= 0
           and dispatch.count("mRaceCarEntityModule.DebugRenderPosition();") == 1),
          ("physics no longer submits the transient position overlay", "DebugRenderPosition();" not in physics)]
numeric = compile_and_run(Path(__file__).with_name("PlayerPositionOverlay.cpp"),
                          "player_position_overlay.inc", code, "PlayerPositionOverlay")
raise SystemExit(report("run_player_position_overlay", wiring, numeric, 1))
