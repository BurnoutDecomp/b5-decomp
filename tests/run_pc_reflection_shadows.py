"""Native shadow-atlas receivers with production shaders and CTAB-gated inputs."""
from pathlib import Path
import argparse
import os
import struct
import subprocess
import sys
import tempfile
from fxgs_common import WORKFLOW, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
sys.path.insert(0, str(WORKFLOW/'tools/assets/shaders'))
from convert_shaders_bundle import find_fxc
from shader_transcode import parse_ctab

parser = argparse.ArgumentParser()
parser.add_argument('--original-depth', action='store_true')
parser.add_argument('--original-bounds', action='store_true')
parser.add_argument('--receiver', choices=('all','3csm','2csm','1csm','select','select-vs','vehicle','road'), default='all')
args = parser.parse_args()
if args.receiver == 'all':
    failures=0
    controls=(['--original-depth'] if args.original_depth else []) + (['--original-bounds'] if args.original_bounds else [])
    for receiver in ('3csm','2csm','1csm','select','select-vs','vehicle','road'):
        result=subprocess.run([sys.executable,__file__,'--receiver',receiver,*controls])
        failures += result.returncode != 0
    raise SystemExit(1 if failures else 0)
source = Tree().read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
code = definition(source, 'bool ProgramDeclaresFloat4Constant(') + '\n'
bind = definition(source, 'bool WorldPrograms_Bind(')
start = bind.index('static std::unordered_map<const void*, bool> sShadowReceiverPrograms;')
end = bind.index('sbRealProgramsBound = true;', start)
code += 'void BindReceiverDepth(const void* lpVertexPayload, const void* lpPixelPayload) { auto* lpDevice=Dev();\n'
code += bind[start:end] + '\n}\n'
code += '''void ConfigureReceiverBounds(bool active) {
 SetShadowReceiverAtlasSizePC(128,192);
 SetShadowReceiverReflectionPC(active);
}\n'''
include = WORKFLOW/'tools/nushaders/Source/Bundle/gamedb/burnout5/Include'
receivers = {
    '3csm': (3,'CALC_SHADOWMAP_INTERPOLATORS3(TestWorld.xyz,TestFaceDepth.x);','CALC_SHADOW_FACTOR_3(1.0)'),
    '2csm': (2,'CALC_SHADOWMAP_INTERPOLATORS2(TestWorld.xyz,TestFaceDepth.x);','CALC_SHADOW_FACTOR_2(1.0)'),
    '1csm': (1,'CALC_SHADOWMAP_INTERPOLATORS1(TestWorld.xyz,TestFaceDepth.x);','CALC_SHADOW_FACTOR_1(1.0)'),
    'select': (4,'CALC_SHADOWMAP_INTERPOLATORS2_SELECT(TestWorld.xyz,TestFaceDepth.x);','CALC_SHADOW_FACTOR_2_SELECT(1.0)'),
    'select-vs': (5,'CALC_SHADOWMAP_INTERPOLATORS2_SELECT_VS(TestWorld.xyz,TestFaceDepth.x,TestWorld.w);','CALC_SHADOW_FACTOR_2_SELECT(1.0)'),
    'vehicle': (6,'CALC_SHADOWMAP_INTERPOLATORS2_SELECT(TestWorld.xyz,TestFaceDepth.x);','CALC_SHADOW_FACTOR_2_SELECT_VEHICLE_DAMAGED(1.0,0.4)'),
    'road': (7,'CALC_SHADOWMAP_INTERPOLATORS3(TestWorld.xyz,TestFaceDepth.x);','CALC_SHADOW_FACTOR_3(1.0)'),
}
kind, positions, factor = receivers[args.receiver]
fx = '''#include "Shadow.fxh"
float4 TestWorld : register(c200);
float4 TestFaceDepth : register(c201);
struct Output { float4 hPosition:POSITION; float4 LightSpacePos0:TEXCOORD0; float4 LightSpacePos1:TEXCOORD1; };
Output VS_Main(float4 p:POSITION) {
 Output OUT=(Output)0; OUT.hPosition=p;
 POSITION_FUNCTION
 return OUT;
}
float4 PS_Main(Output IN):COLOR0 { float f=FACTOR_FUNCTION; return float4(f,f,f,1); }
'''.replace('POSITION_FUNCTION',positions).replace('FACTOR_FUNCTION',factor)
if args.receiver == 'road':
    fx = '#define D_ROAD_X360 1\n#define SHADOW_ROAD_X360_USER 1\n#define SHADOW_APPLY_FADE_ROAD 1\n'+fx
with tempfile.TemporaryDirectory(prefix='reflection_shadow_') as directory:
    path = Path(directory)/'receiver.fx'
    path.write_text(fx)
    declarations = ['const u32 KU_RECEIVER_KIND='+str(kind)+'u;']
    for stage, profile in (('VS','vs_3_0'),('PS','ps_3_0')):
        output = path.with_suffix('.'+stage+'.bin')
        flags = ['/D','D_PC_REFLECTION_SHADOW_DEPTH=1'] if stage=='VS' and not args.original_depth else []
        if not args.original_bounds:
            flags += ['/D','D_PC_REFLECTION_SHADOW_BOUNDS=1']
        command = [find_fxc(),'/nologo','/T',profile,'/E',stage+'_Main','/O2','/Zpr','/I',str(include),*flags,'/Fo',str(output),str(path)]
        result = subprocess.run(command,capture_output=True,text=True)
        if result.returncode:
            raise SystemExit(result.stdout+result.stderr)
        data = output.read_bytes()
        declarations.append('const DWORD KAU_'+stage+'_CODE[] = {'+','.join(hex(w) for w in struct.unpack('<%dI'%(len(data)//4),data))+'};')
        names=set()
        for name, regset, register, count in parse_ctab(data):
            if regset==2:
                names.add(name)
                declarations.append('const u32 KU_'+stage+'_'+name+' = '+str(register)+'u;')
        for name in ('ShadowMap_Constants','ShadowMap_Constants2','ShadowMap_ObjectCsmSelect'):
            if name not in names:
                declarations.append('const u32 KU_'+stage+'_'+name+' = 199u;')
    numeric = compile_and_run(Path(__file__).with_name('PCReflectionShadows.cpp'),
        'pc_reflection_shadows.inc', code, 'PCReflectionShadows',
        extra_files={'pc_reflection_shadow_programs.inc':'\n'.join(declarations)},
        extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_reflection_shadows_'+args.receiver, [], numeric, 1))
