"""Production world/text3D command replay into real D3D9 shaders and pixels.

--packed-cpu reproduces the former 24-byte CPU-copy assumption while preserving
the recovered producer ABI, so its negative proof reaches the native draw.
"""
from pathlib import Path
import argparse
import os
import re
from fxgs_common import Tree, definition, compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--packed-cpu", action="store_true")
parser.add_argument("--omit-3d-budget", action="store_true")
parser.add_argument("--budget-only", action="store_true")
args = parser.parse_args()
tree = Tree(None)
renderer = tree.read("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp")
module = tree.read("src/GameSource/Graphics/BrnRendererModule.cpp")
methods = "#define IM3D_BUDGET_ONLY " + str(int(args.budget_only)) + "\nnamespace {\n"
methods += "rw::LinearResourceAllocator sWorldDispatchAllocator; bool sbWorldDispatchAllocatorReady=false;\n"
methods += "bool MeshPreparationEnabledPC() { return true; }\n"
methods += "\n".join(re.findall(r"    const u32 KU_PC_(?:DISPATCH|GDL|IM2D|IM3D)[^;]*;", module)) + "\n"
budget = definition(module, "bool EnsureWorldDispatchAllocator()")
if args.omit_3d_budget:
    start = budget.index("                              // Every prepared3D buffer")
    end = budget.index("                              + (4u * 4096u)", start)
    budget = budget[:start] + "                              + 8u * 128u\n" + budget[end:]
methods += budget + "\n" + definition(module, "bool DispatchStorageAvailablePC(") + "\n}\n"
methods += "namespace CgsGraphics {\n" + definition(tree.read(
    "src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.cpp"), "void DispatchBin::Construct(") + "\n"
for sig in ("void ImRenderer<V>::BeginRendering()", "void ImRenderer<V>::EndRendering()",
            "void* ImRenderer<V>::SetTransform(", "void ImRenderer<V>::Render(",
            "void Im3dBase<V>::SetTransform(Matrix44 lTransform)",
            "void Im3dBase<V>::SetTransform(Matrix44 lModelToWorld,"):
    body = definition(renderer, sig)
    if args.packed_cpu and sig == "void ImRenderer<V>::Render(":
        start = body.index("    if (lpOutput != nullptr)")
        end = body.index("    D3DDevice_EndVertices", start)
        body = body[:start] + "    if (lpOutput) std::memcpy(lpOutput, lpVertices, 24u*luCount);\n" + body[end:]
    methods += "template<class V>\n" + body + "\n"
font = tree.read("src/GameShared/GameClasses/Graphics/Font/CgsFontRenderer.cpp")
for sig in ("void TextRenderer::Construct()", "TextRenderer::Im2dVertex* TextRenderer::RenderBufferRenderStart(",
            "void TextRenderer::RenderBufferSetTextureState(", "void TextRenderer::RenderBufferRenderEnd(",
            "void TextRenderer::RenderDropShadow("):
    methods += definition(font, sig) + "\n"
untextured = tree.read("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dUntex.cpp")
methods += "template<>\n" + definition(untextured, "void ImRenderer<V>::Render(").replace(
    "ImRenderer<V>", "ImRenderer<BasicColouredVertex>").replace("const V*", "const BasicColouredVertex*") + "\n"
methods += """
template void ImRenderer<BasicColouredVertex>::BeginRendering();
template void ImRenderer<BasicColouredVertex>::EndRendering();
template void ImRenderer<BasicColouredVertex>::Render(renderengine::PrimitiveType,const BasicColouredVertex*,u32);
template void Im3dBase<BasicColouredVertex>::SetTransform(Matrix44);
template void Im3dBase<BasicColouredVertex>::SetTransform(Matrix44,Matrix44);
template void ImRenderer<BasicColouredTexturedVertex>::BeginRendering();
template void ImRenderer<BasicColouredTexturedVertex>::EndRendering();
template void ImRenderer<BasicColouredTexturedVertex>::Render(renderengine::PrimitiveType,const BasicColouredTexturedVertex*,u32);
template void* ImRenderer<BasicColouredTexturedVertex>::SetTransform(const void*);
template void Im3dBase<BasicColouredTexturedVertex>::SetTransform(Matrix44);
template void Im3dBase<BasicColouredTexturedVertex>::SetTransform(Matrix44,Matrix44);
}
"""
im = REPO / "src/GameShared/GameClasses/Graphics/ImmediateMode"
sources = [im / "ImRenderBuffer/CgsImRenderBufferTemplate.cpp",
           im / "ImRenderBuffer/CgsIm3dRenderBuffer.cpp",
           REPO / "src/pc/gcm/renderengine/Im3dProgramsPC.cpp",
           REPO / "src/pc/gcm/renderengine/DepthStencilState.cpp",
           REPO / "src/pc/gcm/renderengine/RasterizerState.cpp",
           REPO / "src/SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.cpp",
           REPO / "vendor/renderware/src/rwcore_alloc.cpp",
           REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp",
           REPO / "vendor/coreallocator/source/icoreallocator_interface.cpp",
           REPO / "vendor/PPMalloc/src/EAGeneralAllocator.cpp"]
result = compile_and_run(Path(__file__).with_name("PCIm3dBuffer.cpp"), "im3d_buffer.inc",
                         methods, "PCIm3dBuffer", extra_sources=sources,
                         extra_flags="/Gy /Gw d3d9.lib user32.lib")
prepare = definition(module, "void BrnRendererModule::PrepareMeshFramePC(")
wiring = [("allocation validity is tested before mesh Reset", prepare.index("DispatchStorageAvailablePC(") < prepare.index("->Reset()"))]
raise SystemExit(report("run_pc_im3d_buffer", wiring, result, 1))
