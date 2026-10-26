"""Compile the production debug-UI delta expression and key-repeat timer."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, code_only, compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
game = code_only(definition(tree.read("src/GameSource/Game/BrnGameModule.cpp"),
                            "bool BrnGameModule::GameMain()"))
delta = re.search(r"mDebugManager\.Update\(([^;]+)\);", game).group(1)
controller = definition(tree.read("src/GameShared/GameClasses/Development/DebugSystem/Controller/CgsDebugController.cpp"),
                        "DebugUI::InputEvent DebugController::GetInputEvent(")
header = tree.read("src/GameShared/GameClasses/Development/DebugSystem/Controller/CgsDebugController.h")
setter = definition(header, "void SetRepeatTimeScalePC(") if "void SetRepeatTimeScalePC(" in header else "void SetRepeatTimeScalePC(float) {}"
scale = re.search(r"SetRepeatTimeScalePC\(([^;]+)\);", game)
configure = "controller.SetRepeatTimeScalePC(" + scale.group(1) + ");" if scale else ""
code = ("float ClockOwner::Delta() const { return " + delta + "; }\n"
        "void ClockOwner::Configure(CgsDev::DebugController& controller) const { " + configure + " }\n"
        "namespace CgsDev {\n" + setter.replace("void SetRepeatTimeScalePC(", "void DebugController::SetRepeatTimeScalePC(") + "\n" + controller + "\n}\n")
numeric = compile_and_run(Path(__file__).with_name("DebugUiFrameTime.cpp"), "debug_ui_frame_time.inc",
                          code, "DebugUiFrameTime", extra_sources=[REPO / "src/GameShared/GameClasses/System/Timer/CgsTimer.cpp"])
wiring = [("repeat scale is supplied before the debug update", bool(scale) and scale.start() < game.index("mDebugManager.Update("))]
raise SystemExit(report("run_debug_ui_frame_time", wiring, numeric, 1))
