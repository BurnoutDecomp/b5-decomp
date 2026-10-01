"""Actual fallback selection, reserved constants and GPU position checks."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
os.environ.pop('BRN_WORLD_UVDEBUG',None)
os.environ.pop('BRN_FALLBACK_WVP_EAGER',None)
p=argparse.ArgumentParser()
p.add_argument('--skip-bind-upload',action='store_true')
p.add_argument('--skip-select-upload',action='store_true')
p.add_argument('--skip-active-update',action='store_true')
p.add_argument('--eager',action='store_true',help='control: retain the original unused upload requests')
a=p.parse_args()
if a.eager:os.environ['BRN_FALLBACK_WVP_EAGER']='1'
s=Tree().read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
names=['KPC_FALLBACK_VS','KPC_FALLBACK_PS','KPC_FALLBACK_TEX_VS','KPC_FALLBACK_TEX_PS']
shader_text=[]
for name in names:
    first=s.index('    const char* '+name+' =')
    shader_text.append(s[first:s.index(';\n',first)+1])
signatures=['void WorldFallbackShader_ApplyWvp(', 'bool WorldFallbackShader_Bind(',
            'void WorldFallbackShader_SetWvp(', 'void WorldFallbackShader_SelectForMesh(']
bodies=[definition(s,sig) for sig in signatures]
for enabled,index in [(a.skip_bind_upload,1),(a.skip_select_upload,3),(a.skip_active_update,2)]:
    if enabled:
        assert bodies[index].count('WorldFallbackShader_ApplyWvp();')==1
        bodies[index]=bodies[index].replace('WorldFallbackShader_ApplyWvp();','(void)0;')
inc='\n'.join(shader_text)+'\nnamespace renderengine {\n'+'\n'.join(bodies)+'\n}\n'
result=compile_and_run(Path(__file__).with_name('PCFallbackWvp.cpp'),'fallback_wvp.inc',inc,
    'PCFallbackWvp',extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_fallback_wvp',[],result,17))
