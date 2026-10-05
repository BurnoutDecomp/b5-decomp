"""Draw through production static geometry uploads, reuse and streaming retirement."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,definition,compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
p=argparse.ArgumentParser();p.add_argument('--skip-up-invalidation',action='store_true');p.add_argument('--skip-assert-invalidation',action='store_true')
p.add_argument('--skip-draw-record',action='store_true',help='negative: leave warm geometry without its completed draw metadata')
p.add_argument('--skip-release-retirement',action='store_true',help='negative: retain stale draw records after full geometry release')
p.add_argument('--unaligned',action='store_true',help='exercise the previous allocation policy as an exact-pixel control')
p.add_argument('--truncate-pool-index',action='store_true',help='negative: truncate the native index-buffer run offset to 16 bits')
a=p.parse_args()
if a.unaligned:os.environ['BRN_GEOMETRY_ALIGN_STRIDE']='0'
else:os.environ.pop('BRN_GEOMETRY_ALIGN_STRIDE',None)
shadow={}
if a.skip_draw_record:
    path='src/pc/gcm/renderengine/WorldGeometryPCLeaf.cpp';text=Tree().read(path)
    before='    const GeometryFrontCacheEntry lEntry{suGeometryGeneration, lVertexKey, lIndexKey, *lpOutDraw};';assert text.count(before)==1
    shadow[path]=text.replace(before,before.replace('*lpOutDraw','WorldGeometryDraw{}'))
if a.skip_release_retirement:
    path='src/pc/gcm/renderengine/WorldGeometryPCLeaf.cpp';text=shadow.get(path,Tree().read(path))
    before=definition(text,'void WorldGeometry_ReleaseAll()');assert before.count('RetireFrontCache();')==1
    shadow[path]=text.replace(before,before.replace('RetireFrontCache();',''))
if a.skip_up_invalidation:
    path='src/pc/gcm/renderengine/GeometryBindingsPCLeaf.h';text=Tree().read(path)
    for indexed in ('false','true'):
        before=f'gCache.InvalidateUP(lpDevice, {indexed});';assert text.count(before)==1
        text=text.replace(before,'')
    shadow[path]=text
if a.skip_assert_invalidation:
    path='src/pc/gcm/renderengine/AssertFramePCLeaf.h';text=Tree().read(path)
    before='GeometryBindingsPC::gCache.Invalidate();';assert text.count(before)==1
    shadow[path]=text.replace(before,'')
if a.truncate_pool_index:
    path='src/pc/gcm/renderengine/WorldGeometryPCLeaf.cpp';text=shadow.get(path,Tree().read(path))
    before='lpOutDraw->muIndexStart     = lrIndex.muIndexStart;';assert text.count(before)==1
    shadow[path]=text.replace(before,'lpOutDraw->muIndexStart     = static_cast<u16>(lrIndex.muIndexStart);')
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCWorldGeometryBuffers.cpp','unused.inc','',
                       'PCWorldGeometryBuffers',extra_flags='d3d9.lib user32.lib d3dcompiler.lib',shadow=shadow)
raise SystemExit(report('run_pc_world_geometry_buffers',[],result,100 if a.unaligned else 104))
