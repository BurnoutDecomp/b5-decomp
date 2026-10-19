"""Check whole production immediate stream and transactional RW storage owners."""
import argparse
import os
from pathlib import Path
import fxgs_common as common

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path)
args = parser.parse_args()
repo = args.source_root.resolve() if args.source_root else common.REPO
workflow = next(parent for parent in common.REPO.parents if (parent / 'tools/build/msvc_flags.txt').is_file())
common.WORKFLOW = workflow
original_settings = common.settings
def settings(name):
    values = original_settings(name)
    if name != 'msvc_includes.txt': return values
    mapped = []
    for value in values:
        parts = Path(value).parts
        private = len(parts) >= 2 and parts[:2] == ('b5-decomp', 'src')
        private |= len(parts) >= 3 and parts[:3] in (
            ('b5-decomp', 'vendor', 'renderware'), ('b5-decomp', 'vendor', 'PPMalloc'))
        mapped.append(str(repo.joinpath(*parts[1:])) if private else value)
    return mapped
common.settings = settings
sources = [
    'src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.cpp',
    'vendor/renderware/src/rwcore_alloc.cpp', 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp',
    'vendor/coreallocator/source/icoreallocator_interface.cpp', 'vendor/PPMalloc/src/EAGeneralAllocator.cpp']
result = common.compile_and_run(Path(__file__).with_name('PCImRenderBufferStorage.cpp'),
    'unused.inc', '', 'PCImRenderBufferStorage', extra_flags='/Gy /Gw',
    extra_sources=[repo / name for name in sources])
raise SystemExit(common.report('run_pc_im_render_buffer_storage', [], result, 18))
