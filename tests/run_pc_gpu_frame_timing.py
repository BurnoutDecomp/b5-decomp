"""Bounded asynchronous query handling plus actual native D3D9 timestamps."""
import argparse, os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--no-overlay-invalidation', action='store_true')
args = parser.parse_args()
hook = definition(Tree().read('src/pc/gcm/renderengine/device.cpp'),
    'bool renderengine::Device::FrameBeginNoClear(')
if args.no_overlay_invalidation:
    hook = hook.replace('    GpuFrameTimingPC::AbandonCurrentFrame();', '')
result = compile_and_run(Path(__file__).with_name('PCGpuFrameTiming.cpp'),
    'pc_gpu_frame_timing.inc', hook, 'PCGpuFrameTiming',
    extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_gpu_frame_timing', [], result, 16))
