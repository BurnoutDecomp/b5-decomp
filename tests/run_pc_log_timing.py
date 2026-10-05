"""Native log timing: file delay, contended sink, bounded records, default off."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, STRSTREAM_CPP, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--enabled', action='store_true')
parser.add_argument('--misattribute-file', action='store_true')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ['BRN_LOG_PROFILE'] = '1' if args.enabled else '0'
os.environ['BRN_LOG_ASYNC'] = '0'  # exercise attribution on the synchronous control path
source = Tree().read('src/GameShared/GameClasses/Development/Log/CgsLog.cpp')
if args.misattribute_file:
    old = 'if (lbProfile) QueryPerformanceCounter(&lFileEnd);'
    assert source.count(old) == 1
    source = source.replace(old, 'if (lbProfile) lFileEnd = lLocked;')
result = compile_and_run(Path(__file__).with_name('PCLogTiming.cpp'),
    'pc_log_timing_source.inc', source, 'PCLogTiming',
    extra_flags='/Gy /Gw user32.lib', extra_sources=[STRSTREAM_CPP])
raise SystemExit(report('run_pc_log_timing', [], result, 12 if args.enabled else 4))
