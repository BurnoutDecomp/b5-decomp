"""ARTIST road-text VMX basis, command units and packed colour regression."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree,definition,compile_and_run,report

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--rev'); args=ap.parse_args()
    tree=Tree(args.rev)
    source=tree.read('src/GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.cpp')
    construct=definition(source,'void CrashNavIconRenderer::Construct(')
    tail=construct[construct.index('    const f32 KF_SCREEN_TO_NDC_X'):]
    tail='void CrashNavIconRenderer::ConstructTextTransform() {\n'+tail
    road=definition(source,'void CrashNavIconRenderer::RenderRoadSign(')
    begin=road.index('    const CgsGraphics::RGBA8 lTextRgba')
    end=road.index('    const f32 lfTextAnchorX',begin)
    colour='u32 RoadTextColour(const Vector4& lrv4SignColour) {\n'+road[begin:end]+'return lTextColour;\n}'
    submit=[line for line in road.splitlines() if 'lpRenderBuffer->SetTransform(' in line and 'mTextTransform' in line]
    assert len(submit)==1
    submit='void CrashNavIconRenderer::SubmitTextTransform(FixtureBuffer* lpRenderBuffer) {\n'+submit[0]+'\n}'
    helpers='\n'.join(definition(source,sig) for sig in ('u8 ColourLaneToByte(', 'CgsGraphics::RGBA8 PackVertexColour('))
    adapter_source=tree.read('src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.cpp')
    adapter=definition(adapter_source,'Im2dTransform Im2dTransformToLogicalPC(')
    numeric=compile_and_run(Path(__file__).with_name('PlaytestFullMapText.cpp'),
        'playtest_full_map_text_bodies.inc','\n'.join((helpers,tail,submit,colour)),
        'PlaytestFullMapText',extra_files={'playtest_full_map_text_adapter.inc':adapter})
    return report('run_playtest_full_map_text',[],numeric,16)
if __name__=='__main__':sys.exit(main())
