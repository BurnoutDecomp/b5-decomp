"""Actual pool-driver marshalling at explicit strategy boundaries."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP, REPO

parser=argparse.ArgumentParser()
parser.add_argument('--old-driver',action='store_true')
args=parser.parse_args()
base='src/GameShared/GameClasses/'
tree=Tree('e2d854af' if args.old_driver else None)
source=tree.read(base+'System/Resource/CgsPoolModule.cpp')
code='\n'.join(definition(source,f'void PoolModule::{name}(').replace(f'PoolModule::{name}',f'Driver::{name}')
    for name in ['UpdateAllocating','UpdateIntelliFrag'])
numeric=compile_and_run(Path(__file__).with_name('PCResourceDefragParams.cpp'),
    'pc_resource_defrag_params.inc',code,'PCResourceDefragParams',extra_sources=[STRSTREAM_CPP,
        REPO/base/'Module/CgsIOBuffer.cpp',REPO/base/'System/Resource/CgsPoolModuleIO_OutputBuffer.cpp'])
raise SystemExit(report('run_pc_resource_defrag_params',[],numeric,11))
