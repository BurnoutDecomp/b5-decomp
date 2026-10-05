"""A slow file sink must not hold the game's producer queue lock."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--hold-lock-during-io', action='store_true')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
path = 'src/GameShared/GameClasses/Development/Log/CgsLogFileQueuePC.h'
source = Tree().read(path)
if args.hold_lock_during_io:
    old = '''                ReleaseSRWLockExclusive(&mLock);

                // This is the only call that can block on the file system.
                const bool lbWritten = mpSink(macBatch, luBytes);
                AcquireSRWLockExclusive(&mLock);'''
    assert source.count(old) == 1
    source = source.replace(old, '                const bool lbWritten = mpSink(macBatch, luBytes);')
result = compile_and_run(Path(__file__).with_name('PCLogFileQueue.cpp'),
    'unused.inc', '', 'PCLogFileQueue', shadow={path: source})
raise SystemExit(report('run_pc_log_file_queue', [], result, 16))
