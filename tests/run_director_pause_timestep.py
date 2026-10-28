"""Issue #58: original pre/post-scene camera clock gates and the zoom divisor."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/GameSource/Director/BrnMainDirector.cpp")
helper = definition(source, "void MainDirector::BuildBehaviourSharedInfo(")
clock = helper[:helper.index("lSharedInfo.mUsedRaceCars")] + "}\n"
declaration = helper[:helper.index("{")].replace("MainDirector::", "") + ";\n"
for phase in ("Pre", "Post"):
    body = definition(source, "void MainDirector::UpdateCameraBehaviours" + phase + "Scene(")
    call = re.search(r"BuildBehaviourSharedInfo\([^;]+;", body).group(0)
    clock += ("void MainDirector::" + phase + "Clock(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex, "
              "Camera::BehaviourSharedInfo& lSharedInfo, ICE::CameraSpaceHandler& lCameraSpaces) { " + call + " }\n")
clock = "namespace BrnDirector {\n" + clock + "}\n"
zoom = definition(tree.read("src/GameSource/Director/Camera/Utils/BrnLooker.cpp"), "void Looker::Zoom(")
zoom = zoom[zoom.index("CGS_ASSERT(!rw::math::fpu::IsZero("):zoom.index("if (mbForceZoomTargetUpdate)")]
timer = REPO / "src/GameShared/GameClasses/System/Timer"
numeric = compile_and_run(Path(__file__).with_name("DirectorPauseTimestep.cpp"), "director_pause_clock.inc",
                          clock, "DirectorPauseTimestep",
                          extra_files={"director_pause_decl.inc": declaration, "director_pause_zoom.inc": zoom},
                          extra_sources=[timer / "CgsTimer.cpp", timer / "CgsTimerStatusInterface.cpp"])
raise SystemExit(report("run_director_pause_timestep", [], numeric, 1))
