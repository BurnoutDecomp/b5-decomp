"""Exercise the production buffered logger with actual temporary Win32 files.

--sink-rev changes only the char sink in the recovered lifecycle. It isolates the
old text-loss bug; it does not claim the old revision supplied Open/Flush/Close.
"""
import argparse
from pathlib import Path
import sys

import fxgs_common as common

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sink-rev')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
common.WORKFLOW = next(p for p in repo.parents if (p / 'tools/build/msvc_flags.txt').exists())
relative = 'src/GameShared/GameClasses/Development/Log/CgsLogFileBuffered.cpp'
source = (repo / relative).read_text(encoding='utf-8-sig')
if args.sink_rev:
    signature = 'StrStreamBase& LogFileBuffered::operator<<(const char* lpcText)'
    previous = common.Tree(args.sink_rev).read(relative)
    source = source.replace(common.definition(source, signature),
                            common.definition(previous, signature), 1)
    print('Negative control: previous char sink only; recovered lifecycle retained.')
result = common.compile_and_run(
    repo / 'tests/DebugMenuResourceLog.cpp', 'ResourceLog.inc', source,
    'ResourceLog', extra_flags=f'/I"{repo / "src"}"',
    extra_sources=(repo / 'src/GameShared/GameClasses/Development/CgsStrStream.cpp',))
if result is None:
    raise SystemExit(1)
checks, failures = result
raise SystemExit(bool(failures))
