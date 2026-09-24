"""FX-TAILS-B (crash parity 2026-09-24, item 2): CollisionStateManager::SetCameraInfo @0x8269FFD0.

The console's collision camera takes its aspect ratio from the director camera's CURRENT flag set,
bit 0 = E_FLAG_WIDESCREEN (`ld 0x140 ; clrldi 63`, 0x826A0050..0x826A0094): 16/9
(SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN, unk_83005F30 <- CRT 0x82C63298 from 0x820AA250 = 0x3FE38E39)
when set, 4/3 (_NORMAL, unk_830085F0 <- CRT 0x82C63278 from 0x820AA254 = 0x3FAAAAAB) when clear. The
PC copied the camera's own mfAspectRatio (+0x5C), which the console never reads here. And the cosine
of the half FOV is the DOUBLE cos rounded once (`fmuls ; bl cos @0x82C096A0 ; frsp`), where the PC
called the single-precision std::cos(f32) -- 47,681 of the 1.14e9 float FOVs in [0, 360] round to a
neighbouring float (e.g. 93.00 -> 0x3F303801, cosf 0x3F303802).

Numeric: tests/FxTailsBCameraInfo.cpp compiles the PRODUCTION SetCameraInfo and the two static
definitions (when the revision has them) against a fixture camera with the real CameraState.
--rev reads a b5 revision (the RED side: <fix>~1): its old body compiles and runs against the
same checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsb_camera_info.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
MANAGER_H = "src/GameSource/Sound/Collision/BrnCollisionStateManager.h"
SET_CAMERA_INFO = "void CollisionStateManager::SetCameraInfo("
NUMERIC_CHECKS = 13


def static_definitions(manager):
    """The two class-static definitions `VecFloat CollisionStateManager::SKF32_CAMERA_ASPECT_RATIO_*
    = {...};`, or '' when the revision has none."""
    found = []
    for name in ("NORMAL", "WIDESCREEN"):
        match = re.search(r"VecFloat\s+CollisionStateManager::SKF32_CAMERA_ASPECT_RATIO_" + name + r"\s*=\s*\{[^;]*\};",
                          manager)
        if match is None:
            return ""
        found.append(match.group(0))
    return "#define FXTAILSB_HAVE_STATICS 1\n" + "\n".join(found) + "\n"


def wiring(tree):
    body = body_or_empty(tree.read(MANAGER_CPP), SET_CAMERA_INFO)
    header = code_only(tree.read(MANAGER_H))
    return [
        ("SetCameraInfo picks the aspect ratio from E_FLAG_WIDESCREEN of the camera's current flags "
         "(0x826A0050 ld 0x140 ; clrldi 63) and never reads the camera's own mfAspectRatio",
         re.search(r"IsFlagSet\(\s*BrnDirector::Camera::CameraState::E_FLAG_WIDESCREEN\s*\)", body) is not None and
         "SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN" in body and "SKF32_CAMERA_ASPECT_RATIO_NORMAL" in body and
         re.search(r"lrCamera\s*\.\s*mfAspectRatio", body) is None),
        ("CollisionStateManager declares SKF32_CAMERA_ASPECT_RATIO_NORMAL / _WIDESCREEN as static VecFloats "
         "(DWARF BrnCollisionStateManager.cpp:82 / :83)",
         re.search(r"static\s+VecFloat\s+SKF32_CAMERA_ASPECT_RATIO_NORMAL\s*;", header) is not None and
         re.search(r"static\s+VecFloat\s+SKF32_CAMERA_ASPECT_RATIO_WIDESCREEN\s*;", header) is not None),
    ]


def numeric(tree):
    manager = tree.read(MANAGER_CPP)
    try:
        body = definition(manager, SET_CAMERA_INFO)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTailsBCameraInfo.cpp"), "fxtailsb_set_camera_info.inc", body,
                           "FxTailsBCameraInfo",
                           extra_files={"fxtailsb_aspect_statics.inc": static_definitions(manager)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsb_camera_info", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
