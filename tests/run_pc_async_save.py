"""Real native save IO with controlled blocking/failure and immutable queued snapshots."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--synchronous', action='store_true')
parser.add_argument('--unbounded', action='store_true')
args = parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ.pop('BRN_HARNESS_SLOT', None)
source = Tree().read('src/GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.cpp')
if args.synchronous:
    old = 'return GetAsyncWriter().Submit(std::move(lWrite));'
    assert source.count(old) == 1
    source = source.replace(old, '''WriteContainerAtPath(lWrite->mDirectory.c_str(), lWrite->mPath.c_str(),
        lWrite->mImage.data(), static_cast<u32>(lWrite->mImage.size()), lWrite->mMugshots.data(),
        static_cast<u32>(lWrite->mMugshots.size()), lWrite->mTitle.c_str(), lWrite->mDescription.c_str());
        return 0;''')
if args.unbounded:
    old = ' || mWrites.size() >= KU_PENDING_LIMIT'
    assert source.count(old) == 1
    source = source.replace(old, '')
result = compile_and_run(Path(__file__).with_name('PCAsyncSave.cpp'), 'async_save_source.inc',
                         source, 'PCAsyncSave')
raise SystemExit(report('run_pc_async_save', [], result, 27))
