"""Validate the production debug-text ABI, TextObject setup and visibility gate."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args=parser.parse_args()
tree=Tree(args.rev)
render=tree.read('src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp')
font=tree.read('src/GameShared/GameClasses/Graphics/Font/CgsFontRenderer.cpp')
vector=tree.read('src/GameShared/GameClasses/Development/VectorFont/CgsVectorFont.cpp')
methods=definition(render,'int MaybeDrawText(')+'\n'
methods+='namespace CgsGraphics {\nstatic const RGBA KU_DEFAULT_COLOUR=0xffffffff;\n'
for sig in ('void TextObject::Construct(', 'void TextRenderer::Construct()'):
    methods+=definition(font,sig)+'\n'
methods+='}\nnamespace CgsDev {\nusing namespace CompressedFontData;\n'
methods+=definition(vector,'void VectorFont::Construct()')+'\n'
methods+=definition(vector,'void VectorFont::SetSize(')+'\n'
for sig in ('void Debug2DImmediateRender::Construct(', 'bool Debug2DImmediateRender::Is2DPointOnScreen(',
            'void Debug2DImmediateRender::DrawText(const char* lpcText, f32 lfX',
            'void Debug2DImmediateRender::DrawText(const char* lpcText, Vector2'):
    methods+=definition(render,sig)+'\n'
methods+='}\n'
result=compile_and_run(Path(__file__).with_name('Debug2DText.cpp'),'debug_2d_text.inc',methods,'Debug2DText')
raise SystemExit(report('run_debug_2d_text',[],result,17))
