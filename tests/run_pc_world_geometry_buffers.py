"""Draw through production static geometry uploads, reuse and streaming retirement."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,definition,compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
p=argparse.ArgumentParser();p.add_argument('--skip-up-invalidation',action='store_true');p.add_argument('--skip-assert-invalidation',action='store_true')
p.add_argument('--skip-draw-record',action='store_true',help='negative: leave warm geometry without its completed draw metadata')
p.add_argument('--skip-release-retirement',action='store_true',help='negative: retain stale draw records after full geometry release')
a=p.parse_args()
shadow={}
if a.skip_draw_record:
    path='src/pc/gcm/renderengine/WorldGeometryPCLeaf.cpp';text=Tree().read(path)
    before='    lrSlot.mDraw        = *lpOutDraw;';assert text.count(before)==1
    shadow[path]=text.replace(before,'')
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
here=Path(__file__).resolve().parent
result=compile_and_run(here/'PCWorldGeometryBuffers.cpp','unused.inc','',
                       'PCWorldGeometryBuffers',extra_flags='d3d9.lib user32.lib d3dcompiler.lib',shadow=shadow)
raise SystemExit(report('run_pc_world_geometry_buffers',[],result,55))
