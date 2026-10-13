"""Link entire production RW/EA owners and exercise native bounds and transactions."""
import argparse
import os
from pathlib import Path
import fxgs_common as common

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path)
parser.add_argument('--legacy-adoption-only', action='store_true',
                    help='Run shared behavior cases against the old instance Initialize API.')
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
flags = '/DLEGACY_ADOPTION_ONLY /DNATIVE_ADOPT_METHOD=Initialize' if args.legacy_adoption_only else ''
result = common.compile_and_run(Path(__file__).with_name('ResourceAllocatorStorage.cpp'),
    'unused.inc', '', 'ResourceAllocatorStorage', extra_flags=flags,
    extra_sources=[repo / 'vendor/renderware/src/rwcore_alloc.cpp',
                   repo / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp',
                   repo / 'vendor/coreallocator/source/icoreallocator_interface.cpp',
                   repo / 'vendor/PPMalloc/src/EAGeneralAllocator.cpp'])
raise SystemExit(common.report('run_resource_allocator_storage', [], result,
                              9 if args.legacy_adoption_only else 14))
