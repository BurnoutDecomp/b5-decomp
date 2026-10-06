"""Interleaved render-only frames must not erase the wheel strip timeout step."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--zero-step-control", action="store_true")
parser.add_argument("--double-clock-control", action="store_true",
    help="hypothesis control: inject retained-step accumulation into real EndOfFrame")
args = parser.parse_args()
tree=Tree(None)
module=tree.read("src/GameSource/Effects/Particles/ParticleModule.cpp")
body=definition(module,"void ParticleModule::EndOfFrame(")
body=body.replace("ParticleModule::EndOfFrame", "Publication::EndOfFrame",1)
if args.zero_step_control:
    body=body.replace("        if (mRenderData.mfCurrentTimeStep != 0.0f)\n", "")
native=tree.read("src/GameSource/Effects/Particles/Native/BrnTrailSystem.cpp")
if args.double_clock_control:
    original=definition(native,"void TrailSystem::EndOfFrame(")
    injected=original.replace("{", "{\n        mfCurrentTime += mfCurrentTimeStep;",1)
    native=native.replace(original,injected,1)
# Keep the entire production simulation; this CPU cadence fixture does not
# bind/draw the GPU, so omit only the uncalled Render body and its dependencies.
native=native.replace(definition(native,"void TrailSystem::Render("),"")
methods=native+"\nnamespace BrnParticle {\n"+body+"\n}\n"
result=compile_and_run(Path(__file__).with_name("PCTrailCadence.cpp"),
    "trail_cadence.inc", methods, "PCTrailCadence",
    extra_flags="/Gy /Gw")
raise SystemExit(report("run_pc_trail_cadence",[],result,1))
