"""FX-LASTFIX item 1 (crash parity 2026-09-26; REVIEW-K DELTA 2, highest-risk item 2): BehaviourIceAnim::Update's heading space eases 20% a
frame, as the console's words do.

  BehaviourIceAnim::Update @0x82247108 builds the look-at of its secondary vehicle's flattened heading and blends its
  own heading space (this+0x610, mHeadingSpaceTransform) towards it: `bl 0x82247354` is rw::math::vpu::SLerp @0x82216858
  with r4 = the heading space (FROM), r5 = the look-at (TO), r6 = an angle-out slot and v1 = v127 = the splat
  unk_82FAA6F0 -- the class-static VecFloat KF_HEADING_SPACE_2_SLERP_AMOUNT (DWARF BrnBehaviourIceAnim.h:181), which the
  CRT thunk 0x82C49580 fills from flt_82004744 == 0x3E4CCCCD == 0.2. The PC called a pointer-amount overload that does
  not exist on the console, whose mounted link stub (DirectorLinkStubs.cpp) returned `to`, with an amount of 1.0f: the
  space SNAPPED to the look-at every frame.

  1. WIRING -- the arm calls the four-argument SLerp with KF_HEADING_SPACE_2_SLERP_AMOUNT and an angle-out slot; the
     constant is 0.2f; no pointer-amount overload is declared (BrnBehaviourIceAnim.cpp) or stubbed
     (DirectorLinkStubs.cpp).
  2. NUMERIC -- tests/FxLastfixIceHeadingSlerp.cpp compiles the revision's PRODUCTION arm, constant and helpers (and, on
     a revision that has them, the overload's declaration and its stub) against the real SLerp, and replays
     tests/FxLastfixIceHeadingSlerpData.h: 7 chains, 64 frames of the console's window 0x8224725C..0x8224737C run on
     emu64 (scratch/CRASHPARITY_0922/fixes/FX-LASTFIX.emu/gen_iceanim_heading_data.py) -- a steady turn, a crash tumble,
     a slow drift (the lerp arm), a spin then hold, an about-turn (the half-turn arms), a vertical forward (CreateLookAt's
     fallback axis) and a position w lane. Two checks a frame: the look-at calls, and the heading space bit for bit.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxlastfix_ice_heading_slerp.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

ICEANIM_CPP = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp"
STUBS_CPP = "src/GameSource/Director/DirectorLinkStubs.cpp"
SLERP_HEADER = "vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h"
ATAN = "src/SDKs/XboxMath/XMVectorATan.h"
HEADERS = (
    "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h",
    "src/GameSource/Director/Camera/Utils/CameraUtils.h",
)
# VehicleInfo embeds a RaceCarState whose constructor calls Clear() (BrnVehicleEvents.cpp).
SOURCES = ("src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",)
UPDATE = "bool BehaviourIceAnim::Update("
SEED = "if (!mbIsPrepared)"
SLERP_CALL = "rw::math::vpu::SLerp("
HELPERS = ("inline rw::math::vpu::Matrix44Affine CreateHeadingSpaceLookAt(const void* lpVehicle)",
           "inline rw::math::vpu::Vector3 GetVehicleWorldPosition(const void* lpVehicle)")
CONSTANT = r"static\s+const\s+f32\s+KF_HEADING_SPACE_2_SLERP_AMOUNT\s*=\s*[^;]+;"
POINTER_DECL = r"Matrix44Affine\s+SLerp\s*\(\s*const\s+Matrix44Affine\s*&\s*\w*\s*,\s*const\s+Matrix44Affine\s*&\s*\w*\s*,\s*const\s+(?:f32|float)\s*\*\s*\w*\s*\)\s*;"
POINTER_STUB = "Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,"
DATA = Path(__file__).with_name("FxLastfixIceHeadingSlerpData.h")


def numeric_total():
    text = DATA.read_text(encoding="utf-8")
    frames = text.split("kaIceHeadingFrames[] = {", 1)[1]
    return 2 * len(re.findall(r"^\s*\{ \d+, \{", frames, re.M))


def update_body(source):
    try:
        return code_only(definition(source, UPDATE))
    except ValueError:
        return ""


def arm_text(update):
    """Update's heading-space arm: from the seed `if (!mbIsPrepared)` to the end of the SLerp statement."""
    start = update.find(SEED)
    call = update.find(SLERP_CALL, start if start >= 0 else 0)
    if start < 0 or call < 0:
        return None
    end = update.find(";", call)
    return update[start:end + 1] if end >= 0 else None


def overload_text(tree):
    """The pointer-amount overload's declaration (BrnBehaviourIceAnim.cpp) and link stub (DirectorLinkStubs.cpp), where
    the revision still has them -- the old arm cannot compile or link without them."""
    parts = []
    decl = re.search(POINTER_DECL, code_only(tree.read(ICEANIM_CPP)))
    if decl:
        parts.append(decl.group(0))
    stubs = tree.read(STUBS_CPP)
    if POINTER_STUB in code_only(stubs):
        parts.append(definition(stubs, POINTER_STUB))
    return "\n".join(parts)


def wiring(tree):
    source = tree.read(ICEANIM_CPP)
    update = update_body(source)
    arm = arm_text(update) or ""
    yield ("BehaviourIceAnim::Update blends the heading space with the four-argument rw::math::vpu::SLerp @0x82216858 "
           "(bl 0x82247354: FROM mHeadingSpaceTransform, TO the look-at, the amount, an angle-out slot)",
           re.search(r"mHeadingSpaceTransform\s*=\s*rw::math::vpu::SLerp\(\s*mHeadingSpaceTransform\s*,\s*lLookAt\s*,"
                     r"\s*KF_HEADING_SPACE_2_SLERP_AMOUNT\s*,\s*&\s*\w+\s*\)", arm) is not None)
    constant = re.search(CONSTANT, code_only(source))
    value = None
    if constant:
        match = re.search(r"=\s*([0-9.]+)f?\s*;", constant.group(0))
        value = float(match.group(1)) if match else None
    yield ("KF_HEADING_SPACE_2_SLERP_AMOUNT is 0.2f (flt_82004744 == 0x3E4CCCCD, splatted by the CRT thunk 0x82C49580)",
           value is not None and abs(value - 0.2) < 1e-9)
    yield ("no pointer-amount SLerp overload is declared (BrnBehaviourIceAnim.cpp) or link-stubbed (DirectorLinkStubs.cpp)",
           overload_text(tree) == "")


def numeric(tree):
    source = tree.read(ICEANIM_CPP)
    arm = arm_text(update_body(source))
    constant = re.search(CONSTANT, code_only(source))
    helpers = []
    for signature in HELPERS:
        try:
            helpers.append(definition(source, signature))
        except ValueError:
            pass
    if arm is None or constant is None or len(helpers) != len(HELPERS):
        print("NUMERIC: the revision's BehaviourIceAnim.cpp lacks the heading-space arm, its constant or its helpers")
        return None
    shadow = None
    if tree.rev is not None:
        shadow = {relative: tree.read(relative) for relative in HEADERS}
        shadow["src/rw/math/vpu/matrix44affine_operation.h"] = tree.read(SLERP_HEADER)
        shadow[ATAN] = tree.read(ATAN)
    with tempfile.TemporaryDirectory(prefix="brn_fxlastfix_heading_") as directory:
        extra = []
        for relative in SOURCES:
            target = Path(directory) / Path(relative).name
            target.write_text(tree.read(relative), encoding="utf-8")
            extra.append(target)
        header = "// GENERATED by run_fxlastfix_ice_heading_slerp.py from " + ICEANIM_CPP + "\n"
        return compile_and_run(Path(__file__).with_name("FxLastfixIceHeadingSlerp.cpp"), "fxlastfix_iceanim_arm.inc",
                               header + arm + "\n", "FxLastfixIceHeadingSlerp", shadow=shadow,
                               extra_files={"fxlastfix_iceanim_const.inc": header + constant.group(0) + "\n",
                                            "fxlastfix_iceanim_helpers.inc": header + "\n".join(helpers) + "\n",
                                            "fxlastfix_iceanim_overload.inc": header + overload_text(tree) + "\n"},
                               extra_sources=tuple(extra))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxlastfix_ice_heading_slerp", list(wiring(tree)), numeric(tree), numeric_total())


if __name__ == "__main__":
    sys.exit(main())
