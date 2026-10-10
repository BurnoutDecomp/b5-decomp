"""Exact finite-domain and whole-record checks of the production vertex baker."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,definition,compile_and_run,report
p=argparse.ArgumentParser()
p.add_argument('--scalar',action='store_true')
p.add_argument('--bad-clamp',action='store_true')
p.add_argument('--benchmark',action='store_true')
a=p.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
os.environ['BRN_GEOMETRY_NORMAL_LUT']='0' if a.scalar else '1'
os.environ['BRN_NORMAL_BENCH']='1' if a.benchmark else ''
if not a.benchmark:os.environ.pop('BRN_NORMAL_BENCH',None)
tree=Tree();text=tree.read('src/pc/gcm/renderengine/WorldGeometry.cpp')
source='template<bool TB_USE_LOOKUP>\n'+definition(text,'__declspec(noinline) void BakePackedVertexData(')+'\n'+definition(text,'void BakeVertexData(')
shadow={}
if a.bad_clamp:
    path='src/pc/gcm/renderengine/PackedNormal.h'
    text=tree.read(path);needle='liValue <= -512 ? -1.0f :'
    assert text.count(needle)==1;shadow[path]=text.replace(needle,'false ? -1.0f :')
result=compile_and_run(Path(__file__).with_name('PCPackedNormal.cpp'),'pc_packed_normal.inc',source,'PCPackedNormal',shadow=shadow)
raise SystemExit(report('run_pc_packed_normal',[],result,57))
