"""Original direct shading publication and bank locks; --rev exercises the old path."""
from pathlib import Path
import argparse
import os
import sys
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, code_only, compile_and_run, report
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser=argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--no-publication',action='store_true')
args=parser.parse_args()
tree=Tree(args.rev)
source = tree.read("src/GameSource/Graphics/BrnRendererModule.cpp")
swap = definition(source, "void BrnRendererModule::SwapBuffers()")
try: publish = definition(source, "void BrnRendererModule::PublishSkyConstantsBringUp(")
except ValueError: publish=''
constructor=definition(source,'void BrnRendererModule::Construct()')
start=constructor.index('    maShaderConstantsFrames[0].Construct();')
end=constructor.index('    // ---- The display class',start)
construct=constructor[start:end]
header=tree.read('src/GameSource/Graphics/BrnRendererModule.h')
try:
    complete=definition(header,'void CompleteWorldDispatchFramePC(').replace(
        'void CompleteWorldDispatchFramePC(', 'void BrnRendererModule::CompleteWorldDispatchFramePC(')
except ValueError:
    complete='void BrnRendererModule::CompleteWorldDispatchFramePC(bool valid) { maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameExternal]=valid; }'
try:
    getter=definition(header,'const BrnShaderConstantsFrame* GetPublishedShaderConstantsFramePC() const').replace(
        'const BrnShaderConstantsFrame* GetPublishedShaderConstantsFramePC()',
        'const BrnShaderConstantsFrame* BrnRendererModule::GetPublishedShaderConstantsFramePC()')
except ValueError:
    getter='const BrnShaderConstantsFrame* BrnRendererModule::GetPublishedShaderConstantsFramePC() const { return maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameInternal] ? &maShaderConstantsFrames[mu8ShaderConstantsFrameInternal] : nullptr; }'
consumers = [code_only(definition(source, signature)) for signature in (
    "void BrnRendererModule::RenderWorldPasses(", "void BrnRendererModule::Render(")]
wiring = [("render consumers use only published shading data", all(
    "PublishSkyConstantsBringUp(" not in c and "gBrnWorldShaderConstantsFrameBringUp" not in c
    and "gBrnSkyCameraBringUp" not in c and "maShaderConstantsFrames[mu8ShaderConstantsFrameExternal]" not in c
    for c in consumers)),
    ('Swap publishes the world-filled bank without a global recopy',
     'PublishSkyConstantsBringUp(' not in code_only(swap))]
if args.no_publication:
    swap=swap.replace('mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameExternal;',
                      'mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameInternal;')
result = compile_and_run(Path(__file__).with_name("PCShaderFramePublication.cpp"),
    "shader_publication.inc", publish + "\n" + complete + "\n" + getter + "\n" + swap, "PCShaderFramePublication",
    extra_sources=[REPO / "src/GameSource/Graphics/BrnShaderConstantsFrame.cpp", STRSTREAM_CPP],
    extra_flags="/Gy /Gw",extra_files={'shader_construct.inc':construct})
raise SystemExit(report("run_pc_shader_frame_publication", wiring, result, 15))
