"""Issue 71: compile the production sign discovery and first-move/event transport paths."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP, code_only

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
prop = "src/GameSource/World/EntityModules/PropEntityModule/"
render = tree.read(prop + "BrnPropEntityModule_Render.cpp")
tail = render.split("lpInput->UnlockForRead();")[-1].split("// ---- [DIAG prop-render")[0]
pre_scene = definition(tree.read(prop + "BrnPropEntityModule_PreScene.cpp"), "PropEntityModule::PreSceneUpdate(")
tick_match = re.search(r"if\s*\(\s*miFramesUntilUpdateVisibleSigns\s*>\s*0\s*\)", pre_scene)
tick = definition(pre_scene, tick_match.group(0)) if tick_match else ""
zone = tree.read(prop + "BrnPropZoneManager.cpp")
move = definition(zone, "if (lbVelocityExceedsThreshold &&")
bridge = tree.read("src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp")
bridge = bridge.split("*lpRecordHitPropQueue,")[1].split("// [DIAG]")[0]
bridge = bridge[bridge.index(");") + 2:]
score = definition(tree.read("src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp"),
                   "void CrashModeScoring::DealWithHitOverheadSign()")
code = ("void Picker::Tick() {\n" + tick + "\n}\n"
        "void Picker::Discover(bool lbRenderCoronas) { const u32 luNumEntities = lrVisibleEntities.GetLength();\n"
        + tail + "\n}\nvoid Move(bool lbVelocityExceedsThreshold, BrnWorld::PropEntityInstance* lpProp, "
        "const BrnPhysics::Props::PropTypeData* lpType, u32 luPropIndex, PropOutput* lpOutput) {\n"
        + move + "\n}\nvoid Bridge(const PropOutput* lpPropOutput_PostPhysics, WorldOutput* lpOutputBuffer) {\n"
        + bridge + "\n}\n" + score)
result = compile_and_run(Path(__file__).with_name("ShowtimeOverheadSigns.cpp"),
                         "showtime_overhead_signs.inc", code, "ShowtimeOverheadSigns",
                         extra_sources=[STRSTREAM_CPP])
wiring = [("the sign countdown advances in PreSceneUpdate and not the render pass",
           bool(tick) and "--miFramesUntilUpdateVisibleSigns" not in code_only(tail))]
raise SystemExit(report("run_showtime_overhead_signs", wiring, result, 1))
