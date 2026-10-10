"""Exercise GPU-page allocation with controlled asynchronous completion and failures."""
from pathlib import Path
import argparse
from fxgs_common import Tree,compile_and_run,report
p=argparse.ArgumentParser()
p.add_argument('--ignore-alignment',action='store_true')
p.add_argument('--keep-stale-prefix',action='store_true')
p.add_argument('--skip-equal-range',action='store_true')
a=p.parse_args()
shadow={}
path='src/pc/gcm/renderengine/GeometryBufferPool.h'
text=Tree().read(path)
if a.ignore_alignment:
    before='16ull / std::gcd(16u, luAlignment) * luAlignment;';assert text.count(before)==1
    text=text.replace(before,'16ull;')
if a.keep_stale_prefix:
    before='lrPage.muSearchStart = lrPage.muSkippedMax = 0;';assert text.count(before)==1
    text=text.replace(before,'/* negative control: stale after coalescing */')
if a.skip_equal_range:
    before='luReserved > lrPage.muSkippedMax';assert text.count(before)==1
    text=text.replace(before,'luReserved >= lrPage.muSkippedMax')
shadow[path]=text
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCGeometryBufferPool.cpp','unused.inc','','PCGeometryBufferPool',shadow=shadow)
raise SystemExit(report('run_pc_geometry_buffer_pool',[],result,17239))
