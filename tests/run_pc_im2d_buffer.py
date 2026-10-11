"""Record and replay production APT/FLAPT buffers on native D3D9 at three sizes."""
from pathlib import Path
import os
import sys
import tempfile
import re
from fxgs_common import compile_and_run, report, REPO, Tree, definition

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
here = Path(__file__).resolve().parent
im = REPO / "src/GameShared/GameClasses/Graphics/ImmediateMode"
sources = [im / "ImRenderBuffer/CgsImRenderBufferTemplate.cpp",
           im / "ImRenderBuffer/CgsIm2dRenderBuffer.cpp", im / "CgsIm2d.cpp",
           REPO / "vendor/renderware/src/rwcore_alloc.cpp",
           REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp",
           REPO / "vendor/coreallocator/source/icoreallocator_interface.cpp",
           REPO / "vendor/PPMalloc/src/EAGeneralAllocator.cpp",
           REPO / "src/GameShared/GameClasses/Gui/PC/CgsAptRenderBackendPC.cpp"]
tree=Tree(None)
debug=tree.read('src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp')
vector=tree.read('src/GameShared/GameClasses/Development/VectorFont/CgsVectorFont.cpp')
font=tree.read('src/GameShared/GameClasses/Graphics/Font/CgsFontRenderer.cpp')
methods='namespace CgsGraphics {\n'+definition(font,'void TextRenderer::Construct()')+'\n}\nnamespace CgsDev {\nusing namespace CompressedFontData;\n'
for sig in ('void VectorFont::Construct()', 'void VectorFont::SetSize('):
    methods+=definition(vector,sig)+'\n'
for sig in ('void Debug2DImmediateRender::Construct(', 'void Debug2DImmediateRender::SetRenderBuffer(',
            'void Debug2DImmediateRender::Begin()', 'void Debug2DImmediateRender::End()',
            'void Debug2DImmediateRender::SetDebugRenderStates()', 'void Debug2DImmediateRender::DispatchVertices()',
            'void Debug2DImmediateRender::AddVertex(', 'void Debug2DImmediateRender::EmitQuad(',
            'void Debug2DImmediateRender::DrawBox(f32', 'void Debug2DImmediateRender::DrawLine(',
            'bool Debug2DImmediateRender::Is2DPointOnScreen(', 'bool Debug2DImmediateRender::Is2DLineOnScreen(',
            'bool Debug2DImmediateRender::Is2DBoxOnScreen(', 'bool Debug2DImmediateRender::Is2DPolygonOnScreen(',
            'void Debug2DImmediateRender::DrawSolidConvexPolygon('):
    methods+=definition(debug,sig)+'\n'
methods+='}\n'
methods=re.sub(r'\bVector2\b','rw::math::vpu::Vector2',methods)
with tempfile.TemporaryDirectory(prefix="brn_im2d_negative_") as directory:
    if "--mask-baseline" in sys.argv:
        # Campaign baseline keeps the original one-mask consumer; current commands
        # and native pixel assertions expose its lost parent alpha and pop state.
        source = Tree("954c0155").read("src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.cpp")
        sources[0] = Path(directory) / "CgsImRenderBufferTemplate.cpp"
        sources[0].write_text(source, encoding="utf-8")
    if "--drop-batches" in sys.argv:
        # Negative proof: reproduce the former missing-opcode behavior while
        # retaining the current producer/ABI so this fails on pixels, not compile.
        source = sources[0].read_text(encoding="utf-8")
        needle = "            // ARTIST 0x827F9EBC..0x827F9F10:"
        assert needle in source
        source = source.replace(needle, "            continue; // negative control: old skipped batch\n" + needle, 1)
        sources[0] = Path(directory) / "CgsImRenderBufferTemplate.cpp"
        sources[0].write_text(source, encoding="utf-8")
    result = compile_and_run(here / "PCIm2dBuffer.cpp", "debug_2d_primitives.inc", methods, "PCIm2dBuffer",
                             extra_sources=sources, extra_flags="/Gy /Gw d3d9.lib user32.lib")
raise SystemExit(report("run_pc_im2d_buffer", [], result, 125))
