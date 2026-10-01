"""Exercise GPU-page allocation with controlled asynchronous completion and failures."""
from pathlib import Path
import argparse
from fxgs_common import Tree,compile_and_run,report
p=argparse.ArgumentParser();p.add_argument('--ignore-alignment',action='store_true');a=p.parse_args()
shadow={}
if a.ignore_alignment:
    path='src/pc/gcm/renderengine/GeometryBufferPoolPCLeaf.h'
    text=Tree().read(path)
    before='16ull / std::gcd(16u, luAlignment) * luAlignment;';assert text.count(before)==1
    shadow[path]=text.replace(before,'16ull;')
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCGeometryBufferPool.cpp','unused.inc','','PCGeometryBufferPool',shadow=shadow)
raise SystemExit(report('run_pc_geometry_buffer_pool',[],result,334))
