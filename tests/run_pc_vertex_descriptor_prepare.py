"""Native declaration creation at resource fixup must preserve published draw state."""
import argparse,os
from pathlib import Path
from fxgs_common import REPO,Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--publish-prewarm',action='store_true')
args=parser.parse_args()
source=Tree().read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
code='namespace renderengine {\n'
code+=definition(source,'struct Vd32Cached')+';\n'
for name in ('sVdCache;','sVdSourceWitness;'):
    code+=next(line.strip() for line in source.splitlines() if line.strip().endswith(name))+'\n'
start=source.index('    bool                    sbLastDeclHasTexcoord0')
end=source.index('    // Set by WorldFallbackShader_MarkInstancedMesh',start)
code+=source[start:end]
for signature in ['inline u64 DeclUsageBit(', 'const char* DeclTypeName(', 'const char* DeclUsageName(',
                  'bool MapXenonDeclType(', 'static void* WorldVd32_GetDeclarationInternal(',
                  'void* WorldVd32_GetDeclaration(', 'bool WorldVd32_PrepareResource(',
                  'void WorldVd32_OnResourceMemoryFreed(', 'void WorldVd32_ReleaseAll(']:
    body=definition(source,signature)
    if args.publish_prewarm and signature.startswith('bool WorldVd32_PrepareResource'):
        assert '&luStride, false)' in body
        body=body.replace('&luStride, false)','&luStride, true)')
    code+=body+'\n'
code+='}\n'
result=compile_and_run(Path(__file__).with_name('PCVertexDescriptorPrepare.cpp'),
    'pc_vertex_descriptor_prepare.inc',code,'PCVertexDescriptorPrepare',
    extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_vertex_descriptor_prepare',[],result,12))
