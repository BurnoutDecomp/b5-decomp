"""Current canonical CPU32 ABI with unchanged native particle24 writer.

Extract the two actual consumer bind blocks; --sizeof-cpu-control restores the
pre-fix sizeof(CPU vertex) selection while preserving the actual packed writer.
This control is specific to the ABI restored during this campaign, not HEAD's
formerly simplified CPU24 record. --cpu-only does not create a native device.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree,definition,code_only,compile_and_run,report,REPO

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--sizeof-cpu-control',action='store_true')
parser.add_argument('--cpu-only',action='store_true')
args=parser.parse_args()
tree=Tree()
methods='#define NATIVE_STRIDE_CPU_ONLY '+str(int(args.cpu_only))+'\nnamespace BrnParticle {\n'
methods+=definition(tree.read('src/GameSource/Effects/Particles/Native/BrnNativeParticleVertex.cpp'),
    'void NativeParticleVertex::VertexIterator::Write(')+'\n}\n'
for name,file,signature in (
    ('Spark','BrnSparkRenderer_Render.cpp','void SparkRenderer::Dispatch('),
    ('Simple','BrnSimpleParticleRenderer.cpp','void BrnSimpleParticleRenderer::Dispatch(')):
    fn=code_only(definition(tree.read('src/GameSource/Effects/Particles/Native/'+file),signature))
    start=fn.index('const u32 luVertexStride')
    first_call=fn.index('D3DDevice_SetStreamSource(',start)
    second_call=fn.index('D3DDevice_SetStreamSource(',first_call+1)
    end=fn.index(';',second_call)+1
    block=fn[start:end]
    if args.sizeof_cpu_control:
        block=block.replace('NativeParticleVertex::GetStride()',
            'static_cast<u32>(sizeof(CgsGraphics::BasicColouredTexturedVertex))')
    methods+='void Bind'+name+'(NativeBuffer* lpVertexBuffer) {\n'+block+'\n}\n'
result=compile_and_run(Path(__file__).with_name('PCNativeParticleStride.cpp'),
    'native_particle_stride.inc',methods,'PCNativeParticleStride',
    extra_flags='/Gy /Gw d3d9.lib user32.lib',extra_sources=(
        REPO/'src/pc/gcm/renderengine/Im3dProgramsPC.cpp',))
raise SystemExit(report('run_pc_native_particle_stride',[],result,53 if args.cpu_only else 57))
