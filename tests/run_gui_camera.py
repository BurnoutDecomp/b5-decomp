"""Run the real GetGuiCamera plus Camera construction/LookAt against raw-data references.

--swap-selectors reverses the two original mode arms. --default-projection
replaces only each authored four-argument Construct with the generic no-arg reset.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--swap-selectors", action="store_true")
parser.add_argument("--default-projection", action="store_true")
args = parser.parse_args()
tree = Tree()
shared = tree.read("src/GameShared/GameClasses/Gui/CgsGuiShared.cpp")
signature = "CgsGraphics::Camera GetGuiCamera()"
body = definition(shared, signature)
if args.swap_selectors:
    body = body.replace("case E_GUICAMERA_NORMAL:", "case GUI_TEST_TEMP:", 1).replace(
        "case E_GUICAMERA_FULLSCREENMAP:", "case E_GUICAMERA_NORMAL:", 1).replace(
        "case GUI_TEST_TEMP:", "case E_GUICAMERA_FULLSCREENMAP:", 1)
if args.default_projection:
    body, count = re.subn(r"lCamera\.Construct\((?:0\.9f|0\.02f), KF_GUI_ASPECT_RATIO, KF_GUI_NEAR_CLIP, KF_GUI_FAR_CLIP\);",
                          "lCamera.Construct();", body)
    if count != 2:
        raise SystemExit("negative control requires both authored constructor calls")
shared = shared.replace(definition(shared, signature), body, 1)
camera = tree.read("src/GameShared/GameClasses/Graphics/CgsCamera.cpp")
constants = "\n".join(line for line in camera.splitlines() if line.startswith("const f32 KF_DEFAULT_"))
methods = "\n".join(definition(camera, signature) for signature in (
    "Camera::Camera(const Camera&", "Camera& Camera::operator=(",
    "void Camera::Construct()", "void Camera::Construct(f32",
    "void Camera::SetFovHorizontal(", "void Camera::UpdatePerspectiveProjectionMatrix()",
    "void Camera::UpdateViewProjectionMatrix()", "void Camera::LookAt(",
))
numeric = compile_and_run(Path(__file__).with_name("GuiCamera.cpp"), "gui_camera.inc",
    constants + "\nnamespace CgsGraphics {\n" + methods + "\n}\n" + shared, "GuiCamera")
if numeric is None:
    raise SystemExit(1)
checks, failures = numeric
print(f"run_gui_camera: {checks - failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
