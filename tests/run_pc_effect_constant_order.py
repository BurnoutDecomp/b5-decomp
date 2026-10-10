"""Debris transform writes must be visible to each native draw, including batches.

Extract the production immediate constant stage, debris setter and draw shims.
The fixture checks the real D3D9 constant file at the world draw boundary. --rev
tests the same producer/consumer chain against the previous revision.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
stage = tree.read("src/pc/gcm/renderengine/ImmediateMode.cpp")
start = stage.index("    const u32 KU_MAX_STAGED_ROWS")
end = stage.index(";", stage.index("    f32       safDiscardRow", start)) + 1
methods = stage[start:end] + "\n"
methods += definition(stage, "void* RenderEngineDeviceBeginShaderStates(")
methods += "\nnamespace renderengine {\n" + definition(stage, "void ImShaderConstants_Flush(") + "\n}\n"
setter = definition(tree.read("src/GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.cpp"),
    "void Im3dTexPlusLighting::SetTransformArray(")
methods += setter.replace("Im3dTexPlusLighting::", "DebrisSetterFixture::")
shims = tree.read("src/pc/gcm/renderengine/XenonD3D9Shims.cpp")
methods += definition(shims, "void D3DDevice_DrawIndexedVertices(")
methods += definition(shims, "void D3DDevice_DrawVertices(")
result = compile_and_run(Path(__file__).with_name("PCEffectConstantOrder.cpp"),
    "effect_constants.inc", methods, "PCEffectConstantOrder", extra_flags="d3d9.lib user32.lib")
raise SystemExit(report("run_pc_effect_constant_order", [], result, 1))
