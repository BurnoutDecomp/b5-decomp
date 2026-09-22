"""FX-AIMOD G08-D2 + G07-D3: ResetOnTrackManager::Update copies the player's camera and
PlayerIsLookingBackwards reads it -- replayed from the production bodies.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_camera.py [--rev <b5 rev>]
A pre-fix Update (three parameters, no camera) is compiled with the camera parameter appended and
ignored, so the RED side reports what the old body did with the camera: nothing.
"""
import re
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, REPO

STRATEGIES = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
MANAGER = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.cpp"
CAMERA = "src/GameSource/Director/Camera/Camera.cpp"


def update_body(manager):
    body = definition(manager, "    void ResetOnTrackManager::Update(")
    head, brace, rest = body.partition("{")
    if "lCamera" not in head:
        head = re.sub(r"f32\s+lfTime\s*\)", "f32 lfTime, BrnDirector::Camera::Camera)", head)
    return head + brace + rest


def main():
    tree = Tree(parse_args().rev)
    strategies, manager, camera = tree.read(STRATEGIES), tree.read(MANAGER), tree.read(CAMERA)
    copy = re.search(r"^Camera::Camera\(const Camera& lrOther\) = default;", camera, re.M)[0]
    chunks = ["namespace BrnDirector { namespace Camera {",
              copy,
              definition(camera, "Vector3 Camera::GetDirection() const"),
              "} }",
              "namespace BrnAI {",
              definition(manager, "    AICar* ResetOnTrackManager::GetAICar("),
              update_body(manager),
              definition(strategies, "namespace\n{"),
              definition(strategies, "bool ResetOnTrackManager::PlayerIsLookingBackwards()"),
              "}"]
    sys.exit(compile_and_run("AIModCamera.cpp", chunks,
                             [REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"],
                             prefix="brn_aimod_camera_"))


if __name__ == "__main__":
    main()
