"""Native shadow-atlas receiver sampling with production shader functions and c255 binding."""
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
args = parser.parse_args()
source = Tree().read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
code = definition(source, 'bool ProgramDeclaresFloat4Constant(') + '\n'
bind = definition(source, 'bool WorldPrograms_Bind(')
start = bind.index('static std::unordered_map<const void*, bool> sShadowReceiverPrograms;')
end = bind.index('sbRealProgramsBound = true;', start)
code += 'void BindReceiverDepth(const void* lpVertexPayload) { auto* lpDevice=Dev();\n'
code += bind[start:end] + '\n}\n'
include = WORKFLOW/'tools/nushaders/Source/Bundle/gamedb/burnout5/Include'
fx = '''#include "Shadow.fxh"
float4 TestWorld : register(c200);
float4 TestFaceDepth : register(c201);
struct Output { float4 hPosition:POSITION; float4 LightSpacePos0:TEXCOORD0; float4 LightSpacePos1:TEXCOORD1; };
Output VS_Main(float4 p:POSITION) {
 Output OUT; OUT.hPosition=p;
 CALC_SHADOWMAP_INTERPOLATORS3(TestWorld.xyz,TestFaceDepth.x);
 return OUT;
}
float4 PS_Main(Output IN):COLOR0 { float f=CALC_SHADOW_FACTOR_3(1.0); return float4(f,f,f,1); }
'''
with tempfile.TemporaryDirectory(prefix='reflection_shadow_') as directory:
    path = Path(directory)/'receiver.fx'
    path.write_text(fx)
    declarations = []
    for stage, profile in (('VS','vs_3_0'),('PS','ps_3_0')):
        output = path.with_suffix('.'+stage+'.bin')
        flags = ['/D','D_PC_REFLECTION_SHADOW_DEPTH=1'] if stage=='VS' and not args.original_depth else []
        command = [find_fxc(),'/nologo','/T',profile,'/E',stage+'_Main','/O2','/Zpr','/I',str(include),*flags,'/Fo',str(output),str(path)]
        result = subprocess.run(command,capture_output=True,text=True)
        if result.returncode:
            raise SystemExit(result.stdout+result.stderr)
        data = output.read_bytes()
        declarations.append('const DWORD KAU_'+stage+'_CODE[] = {'+','.join(hex(w) for w in struct.unpack('<%dI'%(len(data)//4),data))+'};')
        for name, regset, register, count in parse_ctab(data):
            if regset==2:
                declarations.append('const u32 KU_'+stage+'_'+name+' = '+str(register)+'u;')
    numeric = compile_and_run(Path(__file__).with_name('PCReflectionShadows.cpp'),
        'pc_reflection_shadows.inc', code, 'PCReflectionShadows',
        extra_files={'pc_reflection_shadow_programs.inc':'\n'.join(declarations)},
        extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_reflection_shadows', [], numeric, 1))
