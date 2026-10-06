"""Verify full graphics-camera delivery through the real GUI input and view IO bodies."""
from pathlib import Path
import argparse
import re
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, extract, code_only, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    camera, m0 = extract(tree, "src/GameShared/GameClasses/Graphics/CgsCamera.cpp",
                          ("Camera::Camera(const Camera&", "Camera& Camera::operator=("))
    inputs, m1 = extract(tree, "src/GameShared/GameClasses/Gui/CgsGuiModuleIO_InputBuffer.cpp",
                         ("void InputBuffer::SetCamera(", "void InputBuffer::SetImRenderers("))
    views, m2 = extract(tree, "src/GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.cpp",
                        ("void InputBuffer::SetImRenderers(",))
    view = code_only(tree.read("src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp"))
    copy = re.search(r"mImRenderers\.mCamera\s*=\s*lrRenderers\.mCamera\s*;", view)
    if m0 or m1 or m2 or not copy:
        print("Missing camera delivery bodies", m0 + m1 + m2)
        numeric = None
    else:
        numeric = compile_and_run(Path(__file__).with_name("PlaytestGuiCameraDelivery.cpp"),
            "playtest_gui_camera_copy.inc", "\n".join(camera), "PlaytestGuiCameraDelivery",
            extra_files={"playtest_gui_camera_input.inc": "\n".join(inputs),
                         "playtest_gui_camera_view_input.inc": "\n".join(views),
                         "playtest_gui_camera_view_copy.inc": copy[0]})
    bridge = code_only(tree.read("src/GameSource/Game/GameBridgeDirectorToX.cpp"))
    wiring = [("director converts the graphics camera and hands it to GUI input",
               "CopyToCgsCamera(&lGuiCamera)" in bridge and "SetCamera(lGuiCamera)" in bridge)]
    return report("run_playtest_gui_camera_delivery", wiring, numeric, 5)

if __name__ == "__main__":
    sys.exit(main())
