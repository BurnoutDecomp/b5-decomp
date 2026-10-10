"""Native bloom/effects target transitions through the production surface binder.

The real D3D9 textures/surfaces expose colour and raw-depth read/write aliasing.
The production shadow unbinds and raw-depth mask validation are extracted too.
--rev keeps the negative control tied to the old production binder.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
parser.add_argument("--cached-unbind", action="store_true", help="reproduce the reviewed null-shadow defect")
parser.add_argument("--cached-depth-unbind", action="store_true", help="reproduce only the raw-depth null-shadow defect")
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read("src/pc/gcm/renderengine/PostFxRenderTarget.cpp")
methods = "namespace renderengine {\n" + definition(source, "class RenderTargetState\n") + ";\n}\n"
methods += definition(source, "void Device::SetState(const RenderTargetState*").replace(
    "void Device::SetState(", "void renderengine::Device::SetState(")
if args.cached_unbind:
    assert methods.count("lpDevice->SetTexture(luUnit, nullptr);") == 2
    methods = methods.replace("lpDevice->SetTexture(luUnit, nullptr);", "shadow::Device::SetResource(nullptr, luUnit);")
    methods = methods.replace("shadow::Device::SetSamplerTextureShadow(luUnit, nullptr);", "")
elif args.cached_depth_unbind:
    before, depth = methods.split("if (lpState->mpDepthSurface != nullptr)", 1)
    assert depth.count("lpDevice->SetTexture(luUnit, nullptr);") == 1
    depth = depth.replace("lpDevice->SetTexture(luUnit, nullptr);", "shadow::Device::SetResource(nullptr, luUnit);")
    depth = depth.replace("shadow::Device::SetSamplerTextureShadow(luUnit, nullptr);", "")
    methods = before + "if (lpState->mpDepthSurface != nullptr)" + depth
native = tree.read("src/pc/gcm/renderengine/XenonD3D9Shims.cpp")
methods = "namespace { u32 guRawDepthSamplerUnits = 0u;\n" + definition(native, "bool IsRawDepthFormat(") + "\n" + definition(
    native, "bool BoundTextureIsRawDepth(") + "\n}\nnamespace renderengine {\n" + definition(
    native, "u32 PostFxDepthSampler_BoundUnitMask(") + "\n}\n" + methods
device = tree.read("src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp")
methods = "namespace shadow {\n" + definition(device, "void* Device::SetResource(") + "\n" + definition(
    device, "void Device::SetSamplerTextureShadow(") + "\n}\n" + methods
result = compile_and_run(Path(__file__).with_name("PCColourTargetFeedback.cpp"),
    "colour_feedback.inc", methods, "PCColourTargetFeedback",
    extra_flags="d3d9.lib user32.lib")
raise SystemExit(report("run_pc_colour_target_feedback", [], result, 1))
