"""CPU original untextured command recording, virtual replay, transforms and packing."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--packed-cpu", action="store_true")
parser.add_argument("--wrong-fence", action="store_true")
args = parser.parse_args()
tree = Tree()
base = "src/GameShared/GameClasses/Graphics/ImmediateMode/"
im = tree.read(base + "ImRenderBuffer/CgsImRenderBufferTemplate.cpp")
buffer = tree.read(base + "ImRenderBuffer/CgsIm3dRenderBuffer.cpp")
renderer = tree.read(base + "CgsIm3dUntex.cpp")
transform = tree.read(base + "CgsIm3d.cpp")
methods = "namespace CgsGraphics {\n"
for source, signatures in (
    (im, ("void ImRenderBuffer<V>::Construct()", "void ImRenderBuffer<V>::Clear()", "void ImRenderBuffer<V>::Swap()",
          "V* ImRenderBuffer<V>::AllocVertices(", "void ImRenderBuffer<V>::BeginRendering()",
          "void ImRenderBuffer<V>::EndRendering()", "void ImRenderBuffer<V>::Render(",
          "void ImRenderBuffer<V>::SetBufferFullRewindToLastEndRender()",
          "const ImCommand* ImRenderBuffer<V>::GetFirstCommand() const", "const ImCommand* ImRenderBuffer<V>::GetNextCommand(")),
    (buffer, ("void Im3dRenderBufferBase<V>::Dispatch(", "bool Im3dRenderBufferBase<V>::HandleCommand(",
              "void Im3dRenderBufferBase<V>::PostCommand3d(", "void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam lViewProjection)",
              "void Im3dRenderBufferBase<V>::SetTransform(Matrix44::InParam lModelToWorld,")),
    (renderer, ("void ImRenderer<V>::BeginRendering()", "void ImRenderer<V>::EndRendering()", "void ImRenderer<V>::Render(",
                "void* ImRenderer<V>::SetTransform(")),
    (transform, ("void Im3dBase<V>::SetTransform(Matrix44 lTransform)", "void Im3dBase<V>::SetTransform(Matrix44 lModelToWorld,"))):
    for sig in signatures:
        body = definition(source, sig)
        if args.packed_cpu and sig == "void ImRenderer<V>::Render(":
            start = body.index("    if (lpOutput != nullptr)")
            end = body.index("    D3DDevice_EndVertices", start)
            body = body[:start] + "    if (lpOutput) std::memcpy(lpOutput, lpVertices, 16u * luCount);\n" + body[end:]
        if args.wrong_fence and sig == "void ImRenderer<V>::Render(":
            body = body.replace("16u * luCount > 0x80000u", "16u * luCount >= 0x80000u")
        methods += "template<class V>\n" + body + "\n"
methods += "}\n"
result = compile_and_run(Path(__file__).with_name("PCUntexturedCommandBuffer.cpp"), "untextured_methods.inc", methods,
    "PCUntexturedCommandBuffer", extra_sources=[STRSTREAM_CPP], extra_flags="/Gy /Gw")
raise SystemExit(report("run_pc_untextured_command_buffer", [], result, 23))
