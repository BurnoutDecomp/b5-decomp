"""Real composite sampler binds must replace prior world sampling on units1/2.

The extracted production unit1/2 bind block feeds the unchanged composite
pixel programs. --rev exercises the actual earlier consumer on the same GPU.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
shader = tree.read("src/GameSource/Graphics/PostFx/BrnPostFxShader.cpp")
render = definition(shader, "void BrnPostFxShader::Render(")
start = render.index("    // Units 1 and 2:")
end = render.index("    // Units 3 and 4:", start)
bind = render[start:end]
shadow = tree.read("src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp")
shim = tree.read("src/pc/gcm/renderengine/XenonD3D9Shims.cpp")
methods = definition(shim, "void ApplyPostFxSourceSamplerState(") + "\n"
methods += 'extern "C" {\n' + definition(shim, "void* SetSamplerStateLowLevel(") + "\n}\n"
methods += "namespace shadow {\n" + definition(shadow, "void* Device::SetState(void* lpState,") + "\n"
methods += definition(shadow, "void* Device::SetResource(void* lpTexture,") + "\n}\n"
methods += "void BindAuxiliarySamplers(renderengine::Texture* lpBloomTexture, renderengine::Texture* lpDofTexture) {\n" + bind + "}\n"
numeric = compile_and_run(Path(__file__).with_name("PCPostFxAuxSamplers.cpp"),
    "postfx_aux_samplers.inc", methods, "PCPostFxAuxSamplers",
    extra_sources=[REPO / "src/pc/gcm/renderengine/PostFxProgramsPC.cpp"],
    extra_flags="/Gy /Gw d3d9.lib user32.lib")
raise SystemExit(report("run_pc_postfx_aux_samplers", [], numeric, 1))
