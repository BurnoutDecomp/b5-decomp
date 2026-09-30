"""Check the actual sky publisher, frame flip, and production consumer wiring."""
from pathlib import Path
import os
import sys
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, code_only, compile_and_run, report
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
source = Tree().read("src/GameSource/Graphics/BrnRendererModule.cpp")
swap = definition(source, "void BrnRendererModule::SwapBuffers()")
publish = definition(source, "void BrnRendererModule::PublishSkyConstantsBringUp(")
consumers = [code_only(definition(source, signature)) for signature in (
    "void BrnRendererModule::RenderWorldPasses(", "void BrnRendererModule::Render(")]
wiring = [("render consumers use only published shading data", all(
    "PublishSkyConstantsBringUp(" not in c and "gBrnWorldShaderConstantsFrameBringUp" not in c
    and "gBrnSkyCameraBringUp" not in c and "maShaderConstantsFrames[mu8ShaderConstantsFrameExternal]" not in c
    for c in consumers))]
if "--no-publication" in sys.argv:
    swap = swap.replace("PublishSkyConstantsBringUp(&maShaderConstantsFrames[mu8ShaderConstantsFrameExternal]);", "(void)0;")
result = compile_and_run(Path(__file__).with_name("PCShaderFramePublication.cpp"),
    "shader_publication.inc", publish + "\n" + swap, "PCShaderFramePublication",
    extra_sources=[REPO / "src/GameSource/Graphics/BrnShaderConstantsFrame.cpp", STRSTREAM_CPP],
    extra_flags="/Gy /Gw")
raise SystemExit(report("run_pc_shader_frame_publication", wiring, result, 9))
