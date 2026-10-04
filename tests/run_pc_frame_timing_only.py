"""Real frame recorder: clock-read bound, frame pacing and workload evidence."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--keep-section-timers', action='store_true')
parser.add_argument('--keep-coarse-draw-timers', action='store_true')
parser.add_argument('--drop-cycle-deltas', action='store_true')
parser.add_argument('--drop-new-camera-marker', action='store_true')
parser.add_argument('--non-atomic-cycle-failures', action='store_true')
args = parser.parse_args()
source = Tree().read('src/pc/gcm/renderengine/FrameProfilePCLeaf.h')
if args.keep_section_timers:
    needle = 'lbEnabled && !gCapture.mbTimingOnly'
    assert source.count(needle) == 1
    source = source.replace(needle, 'lbEnabled')
if args.keep_coarse_draw_timers:
    needle = '(!gCapture.mbCoarse || IsCoarseSection(leSection))'
    assert source.count(needle) == 1
    source = source.replace(needle, 'true')
if args.drop_cycle_deltas:
    needle = 'mpFrame->maCycles[meSection] += luEnd - muBeginCycles'
    assert source.count(needle) == 1
    source = source.replace(needle, 'mpFrame->maCycles[meSection] += 0')
if args.drop_new_camera_marker:
    needle = 'lpFrame->muNewCameraPublications += lbNew;'
    assert source.count(needle) == 1
    source = source.replace(needle, '(void)lbNew;')
if args.non_atomic_cycle_failures:
    needle = 'InterlockedIncrement(&mpFrame->muCycleReadFailures)'
    assert source.count(needle) == 2
    source = source.replace(needle, '++mpFrame->muCycleReadFailures')
result = compile_and_run(Path(__file__).with_name('PCFrameTimingOnly.cpp'),
                         'pc_frame_timing_only.inc', source, 'PCFrameTimingOnly')
raise SystemExit(report('run_pc_frame_timing_only', [], result, 46))
