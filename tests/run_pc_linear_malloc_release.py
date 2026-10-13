"""Link whole LinearMalloc/RW/EA providers; distinguish injected state from producers."""
from pathlib import Path
import fxgs_common as common

repo = common.REPO
numeric = common.compile_and_run(Path(__file__).with_name('PCLinearMallocRelease.cpp'),
    'unused.inc', '', 'PCLinearMallocRelease', extra_sources=[
        repo / 'src/GameShared/GameClasses/Memory/CgsLinearMalloc.cpp',
        repo / 'vendor/renderware/src/rwcore_alloc.cpp',
        repo / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp',
        repo / 'vendor/coreallocator/source/icoreallocator_interface.cpp',
        repo / 'vendor/PPMalloc/src/EAGeneralAllocator.cpp'])
raise SystemExit(common.report('run_pc_linear_malloc_release', [], numeric, 20))
