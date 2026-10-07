"""CPU regression for canonical native-width GUI renderer sets and original camera semantics.

Negative controls --truncate-pointers and --zero-camera reproduce the former 20-byte
pointer copy and camera-zeroing defects without modifying the production tree.
--overwrite-camera removes only the module setter's original final camera restore.
"""
import argparse
import os
from pathlib import Path

from fxgs_common import Tree, definition, compile_and_run, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--truncate-pointers", action="store_true")
parser.add_argument("--zero-camera", action="store_true")
parser.add_argument("--overwrite-camera", action="store_true")
args = parser.parse_args()
tree = Tree()

camera_source = tree.read("src/GameShared/GameClasses/Graphics/CgsCamera.cpp")
defaults = "\n".join(line for line in camera_source.splitlines()
                     if line.startswith("const f32 KF_DEFAULT_"))
camera = "\n".join(definition(camera_source, signature) for signature in (
    "Camera::Camera(const Camera&", "Camera& Camera::operator=(",
    "void Camera::Construct()", "void Camera::Construct(f32",
    "void Camera::SetFovHorizontal(", "void Camera::UpdatePerspectiveProjectionMatrix()",
    "void Camera::UpdateViewProjectionMatrix()",
))
custom_source = tree.read("src/GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.cpp")
construct = definition(custom_source, "void ImRendererSet::Construct()")
if args.zero_camera:
    construct = construct.replace("mCamera.Construct();", "std::memset(&mCamera, 0, sizeof(mCamera));", 1)

module_source = tree.read("src/GameShared/GameClasses/Gui/CgsGuiModuleIO_InputBuffer.cpp")
setter = definition(module_source, "void InputBuffer::SetImRenderers(")
if args.truncate_pointers:
    fields = ("mpIm2dRenderBuffer", "mpIm3dRenderBuffer", "mpIm3dRenderBufferUntex",
              "mpIm3dRenderBufferRacePosition", "mpIm3dRenderBufferMenusAndHud")
    for field in fields:
        setter = setter.replace(f"mRendererSet.{field} = lrRenderers.{field};", "", 1)
    setter = setter.replace("// (3) assign", "std::memcpy(&mRendererSet.mpIm2dRenderBuffer, "
        "&lrRenderers.mpIm2dRenderBuffer, 20);\n        // (3) assign", 1)
if args.overwrite_camera:
    setter = setter.replace("mRendererSet.mCamera = lSavedCamera;", "", 1)
module = "\n".join((definition(module_source, "void InputBuffer::Construct()"),
    definition(module_source, "void InputBuffer::SetCamera("), setter,
    definition(module_source, "const ImRendererSet& InputBuffer::GetImRenderers() const")))
view_source = tree.read("src/GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.cpp")
view = "\n".join(definition(view_source, signature) for signature in (
    "void InputBuffer::SetImRenderers(", "const ImRendererSet& InputBuffer::GetImRenderers() const"))
base_source = tree.read("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
bodies = (defaults + "\nnamespace CgsGraphics {\n" + camera + "\n}\n"
    + base_source + "\nnamespace CgsGui {\n" + construct
    + "\nnamespace CgsGuiModuleIO {\n" + module + "\n} namespace ViewIO {\n" + view + "\n} }\n")
result = compile_and_run(Path(__file__).with_name("GuiImRendererSet.cpp"),
    "gui_im_renderer_set.inc", bodies, "GuiImRendererSet", extra_sources=[STRSTREAM_CPP])
if result is None:
    raise SystemExit(1)
checks, failures = result
print(f"run_gui_im_renderer_set: {checks - failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
