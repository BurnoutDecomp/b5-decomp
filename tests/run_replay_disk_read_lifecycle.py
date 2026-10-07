"""Real replay disk lifecycle with observed device/OS boundaries; CPU only.

Controls mutate temporary extracted code, not an old revision.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode=True
from fxgs_common import STRSTREAM_CPP,Tree,compile_and_run

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--truncate-size',action='store_true')
parser.add_argument('--cancel-closes',action='store_true')
args=parser.parse_args()
source=Tree().read('src/GameSource/Replays/Stream/BrnReplayDiskReadStream.cpp')
if args.truncate_size:
    source=source.replace('muLastReadSize = luSize;','muLastReadSize = static_cast<u32>(luSize);',1)
if args.cancel_closes:
    start=source.index('void DiskReadStream::OnClose(')
    source=source[:start]+source[start:].replace('meStatus = E_STATUS_OPEN;',
                                               'meStatus = E_STATUS_CLOSED;',1)
numeric=compile_and_run(Path(__file__).with_name('ReplayDiskReadLifecycle.cpp'),
    'replay_disk_read_lifecycle.inc',source,'ReplayDiskReadLifecycle',extra_sources=[STRSTREAM_CPP])
if numeric is None:
    raise SystemExit(1)
checks,failures=numeric
print(f'run_replay_disk_read_lifecycle: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
