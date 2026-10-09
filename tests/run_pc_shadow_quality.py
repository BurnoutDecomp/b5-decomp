"""Native atlas allocation, hardware PCF banding and production slope-bias scope."""
from pathlib import Path
import argparse,os,struct,subprocess,sys,tempfile
from fxgs_common import Tree,WORKFLOW,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
sys.path.insert(0,str(WORKFLOW/'tools/assets/shaders'))
from convert_shaders_bundle import find_fxc
parser=argparse.ArgumentParser()
parser.add_argument('--omit-compensation',action='store_true')
args=parser.parse_args()
source=Tree().read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
setter=definition(source,'void D3DDevice_SetRenderState_SlopeScaleDepthBias(')
if args.omit_compensation:
    setter=setter.replace('renderengine::ShadowSlopeBiasForPassPC(luFloatAsDword, sbShadowPassActive)','luFloatAsDword')
code=setter+'\nnamespace renderengine {\n'
code+=definition(source,'void ShadowSampler_ApplyState(')+'\n'
code+=definition(source,'ShadowAtlasSizePC ChooseShadowAtlasSizePC(')+'\n'
for target,signature,marker in [('BeginShadowBiasTest','void PCSurfaceBracket_Save(','sbShadowPassActive = true;'),
                                ('EndShadowBiasTest','void PCSurfaceBracket_Restore(','sbShadowPassActive = false;')]:
    body=definition(source,signature)
    start=body.index(marker)
    end=body.index('\n\n',start)
    code+='void '+target+'() { auto* lpDevice=Dev();\n'+body[start:end]+'\n}\n'
code+='}\n'
with tempfile.TemporaryDirectory(prefix='shadow_quality_') as directory:
    fx=Path(directory)/'compare.fx';out=fx.with_suffix('.fxo')
    fx.write_text('sampler2D Shadow:register(s15); float4 Params:register(c0);\n'
                  'float4 Main(float2 uv:TEXCOORD0):COLOR0 {'
                  'float u=0.13+0.713*uv.x; float z=Params.x+Params.y*u; '
                  'float v=tex2Dproj(Shadow,float4(u,Params.z,z,1)).r; return float4(v,v,v,1); }')
    subprocess.run([find_fxc(),'/nologo','/T','ps_3_0','/E','Main','/O2','/Fo',str(out),str(fx)],check=True,capture_output=True)
    data=out.read_bytes();words=struct.unpack('<%dI'%(len(data)//4),data)
    program='const DWORD kauShadowQualityProgram[] = {'+','.join('0x%08x'%w for w in words)+'};\n'
    result=compile_and_run(Path(__file__).with_name('PCShadowQuality.cpp'),'pc_shadow_quality.inc',code,
        'PCShadowQuality',extra_files={'pc_shadow_quality_program.inc':program},extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_shadow_quality',[],result,1))
