"""Acquire a GUI-owned texture through production GameData pool requests.

--rev selects the old request consumer for a negative control; native pool
filtering, resource handles, and event queues remain production code.
"""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, STRSTREAM_CPP, definition, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
base = 'src/GameShared/GameClasses/System/Resource/'
pool = Tree().read(base + 'CgsResourcePool.cpp')
module = Tree(args.rev).read(base + 'CgsResourcePoolModule.cpp')
code = 'namespace CgsResource {\n'
for signature in ['s32 Pool::FindResourceIndex(', 'Entry* Pool::FindResource(']:
    code += definition(pool, signature) + '\n'
for signature in ['Pool* PoolModule::GetPool(', 'void PoolModule::DoAcquireResourceRequest(']:
    code += definition(module, signature) + '\n'
code += '}\n'
result = compile_and_run(Path(__file__).with_name('PCGuiPoolAcquire.cpp'),
                         'pc_gui_pool_acquire.inc', code, 'PCGuiPoolAcquire', extra_flags='/Gy',
                         extra_sources=[STRSTREAM_CPP, REPO / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_pc_gui_pool_acquire', [], result, 27))
