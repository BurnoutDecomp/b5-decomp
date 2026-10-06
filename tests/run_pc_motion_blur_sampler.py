"""Original 4x/16x motion-blur minification must survive D3D9 MAG capabilities."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
parser.add_argument("--min-only", action="store_true", help="exercise the game log's missing MAG capability")
parser.add_argument("--no-aniso", action="store_true", help="exercise the fully unsupported device fallback")
args = parser.parse_args()
source = Tree(args.rev).read("src/pc/gcm/renderengine/XenonD3D9Shims.cpp")
method = definition(source, "void ApplyPostFxSourceSamplerState(")
assert method.count("lpDevice->GetDeviceCaps(&lCaps)") == 1
method = method.replace("lpDevice->GetDeviceCaps(&lCaps)", "QueryMotionCaps(lpDevice, &lCaps)")
method = "#define MIN_ONLY " + str(int(args.min_only)) + "\n#define NO_ANISO " + str(int(args.no_aniso)) + "\n" + method
result = compile_and_run(Path(__file__).with_name("PCMotionBlurSampler.cpp"),
    "motion_sampler.inc", method, "PCMotionBlurSampler", extra_flags="d3d9.lib user32.lib")
raise SystemExit(report("run_pc_motion_blur_sampler", [], result, 1))
