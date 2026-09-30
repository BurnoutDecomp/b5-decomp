"""Exercise actual scratch allocators, hash maps and budgeted byte copies.

--old-streams restores the previous constant-success gather/scatter methods.
"""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, definition, compile_and_run, report, STRSTREAM_CPP

parser=argparse.ArgumentParser()
parser.add_argument('--old-streams',action='store_true')
args=parser.parse_args()
base='src/GameShared/GameClasses/'
resource=base+'System/Resource/'
tree=Tree()
source=tree.read(resource+'CgsResourceScratchPool.cpp')
if args.old_streams:
    old=Tree('e2d854af').read(resource+'CgsResourceScratchPool.cpp')
    for name in ['UpdateGather','UpdateScatter']:
        signature=f'bool ScratchPool::{name}('
        source=source.replace(definition(source,signature),definition(old,signature))
numeric=compile_and_run(Path(__file__).with_name('PCResourceScratch.cpp'),
    'pc_resource_scratch_unused.inc','', 'PCResourceScratch',
    extra_files={'pc_resource_scratch.cpp':source},
    extra_sources=['pc_resource_scratch.cpp', STRSTREAM_CPP,
        REPO/resource/'CgsResourceImportHashTable.cpp', REPO/resource/'CgsSmallResource.cpp',
        *[REPO/base/'Memory'/name for name in ['CgsLinearMalloc.cpp','CgsDistributionStream.cpp',
             'CgsGatherStream.cpp','CgsScatterStream.cpp']],
        REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_pc_resource_scratch',[],numeric,28))
