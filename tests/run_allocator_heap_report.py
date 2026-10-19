"""Link and exercise the entire production PC allocator reporting owner."""
import argparse
import os
from pathlib import Path
import tempfile
import fxgs_common as common

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path, help='allocator checkout; default is this test checkout')
args = parser.parse_args()
repo = args.source_root.resolve() if args.source_root else common.REPO
workflow = next(parent for parent in common.REPO.parents if (parent / 'tools/build/msvc_flags.txt').is_file())
common.WORKFLOW = workflow
original_settings = common.settings

def settings(name):
    values = original_settings(name)
    if name != 'msvc_includes.txt':
        return values
    mapped = []
    for value in values:
        parts = Path(value).parts
        private = len(parts) >= 2 and parts[:2] == ('b5-decomp', 'src')
        private |= len(parts) >= 3 and parts[:3] in (
            ('b5-decomp', 'vendor', 'renderware'), ('b5-decomp', 'vendor', 'PPMalloc'))
        mapped.append(str(repo.joinpath(*parts[1:])) if private else value)
    return mapped

common.settings = settings
source = repo / 'vendor/PPMalloc/src/EAGeneralAllocator.cpp'
result = common.compile_and_run(Path(__file__).with_name('AllocatorHeapReport.cpp'),
    'unused.inc', '', 'AllocatorHeapReport', extra_sources=[source])
raise SystemExit(common.report('run_allocator_heap_report', [], result, 18))
