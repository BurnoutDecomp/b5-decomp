"""FX-LASTFIX item 1b (crash parity 2026-09-26): a BehaviourIceAnim take reads its OWN reference spaces, as on the console.

  BehaviourIceAnim::Update @0x82247108 copy-constructs the MainDirector's shared ICE::CameraSpaceHandler into a stack
  handler (0x82247384) and, before the take evaluator reads it (the controller's vtable Update, 0x822474A8), overrides
  four of the copy's spaces: mCarToWorld (+0x00) <- the primary vehicle's transform, mCar2ToWorld (+0x40) <- the
  secondary's, mHeading2ToWorld (+0x1C0) <- the behaviour's eased heading space (this+0x610), and, only when
  mbForceHeadingSpaceToBeLooseHeadingSpace (+0xE2A: 0x82247450 `beq`, clear -> T 0x8224748C / set -> F 0x82247454),
  mHeadingToWorld (+0x140) <- world +0x80, the player's loose heading space. The PC discarded both VehicleRef::Get
  results and wrote nothing, so every take read the shared handler: CAR = the player, CAR2 = the race car nearest the
  player, HEADING2 = that car's raw transform, and TAKEDOWN / REVERSE_TAKEDOWN / BYSTANDER derived from them.

  1. WIRING -- the arm calls the four setters (inline in ICECameraSpaceHandler.hpp, their DWARF home :82 / :86 / :90 /
     :95) with the primary's and the secondary's mRaceCarState.mTransform, mHeadingSpaceTransform, and -- under
     mbForceHeadingSpaceToBeLooseHeadingSpace -- the world's GetPlayerLooseHeadingSpace().
  2. NUMERIC -- tests/FxLastfixIceSpaces.cpp compiles the revision's PRODUCTION statements (from `ICE::CameraSpaceHandler
     lSpaces(` to the ShotContext) against the revision's real CameraSpaceHandler / AllVehicleData / VehicleInfo and
     replays tests/FxLastfixIceSpacesData.h: 8 rows (both flag arms, one car on both refs) of the console's window
     0x8224725C..0x8224748B run on emu64 (the copy constructor 0x821FAA88 interpreted; generator
     scratch/CRASHPARITY_0922/fixes/FX-LASTFIX.emu/gen_iceanim_spaces_data.py). Ten checks a row: the take copy's eight
     matrices, each mapped from its console offset to the member it names, its mpGamePlayCam, and the shared handler
     left untouched.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxlastfix_ice_spaces.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

ICEANIM_CPP = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp"
HANDLER_HPP = "src/SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"
HEADERS = (
    HANDLER_HPP,
    "src/SDKs/Packages/ICE/ICEDataEnums.hpp",   # the handler header includes these two siblings by bare name,
    "src/SDKs/Packages/ICE/ICEMath.hpp",        # so a shadowed copy needs them beside it
    "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h",
    "src/GameSource/Director/Utils/BrnDirectorAllVehicleData.h",
)
SOURCES = ("src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",)
UPDATE = "bool BehaviourIceAnim::Update("
ARM_START = "ICE::CameraSpaceHandler lSpaces("
ARM_END = "ShotContext lShotContext;"
DATA = Path(__file__).with_name("FxLastfixIceSpacesData.h")


def numeric_total():
    return 10 * DATA.read_text(encoding="utf-8").count('    { "')


def arm_text(source):
    try:
        update = code_only(definition(source, UPDATE))
    except ValueError:
        return None
    start = update.find(ARM_START)
    end = update.find(ARM_END, start if start >= 0 else 0)
    return update[start:end] if start >= 0 and end > start else None


def wiring(tree):
    arm = arm_text(tree.read(ICEANIM_CPP)) or ""
    flat = re.sub(r"\s+", " ", arm)
    header = code_only(tree.read(HANDLER_HPP))
    yield ("the take's CAR space is the primary vehicle's transform (0x82247388..0x822473CC, handler +0x00)",
           re.search(r"lSpaces\.SetCarToWorld\(\s*lpPrimaryVehicle->mRaceCarState\.mTransform\s*\)", flat) is not None
           and re.search(r"lpPrimaryVehicle\s*=\s*mPrimaryVehicleRef\.Get\(\s*lpWorld\s*\)", flat) is not None)
    yield ("the take's CAR2 space is the secondary vehicle's transform (0x822473D0..0x82247418, handler +0x40)",
           re.search(r"lSpaces\.SetCar2ToWorld\(\s*lpSecondaryVehicle->mRaceCarState\.mTransform\s*\)", flat) is not None
           and re.search(r"lpSecondaryVehicle\s*=\s*mSecondaryVehicleRef\.Get\(\s*lpWorld\s*\)", flat) is not None)
    yield ("the take's HEADING2 space is the eased heading space (0x822473DC..0x8224744C, handler +0x1C0)",
           "lSpaces.SetHeading2ToWorld(mHeadingSpaceTransform)" in flat)
    yield ("under mbForceHeadingSpaceToBeLooseHeadingSpace (+0xE2A, 0x82247450) HEADING is the player's loose heading "
           "space (world +0x80, handler +0x140)",
           re.search(r"if \( ?mbForceHeadingSpaceToBeLooseHeadingSpace ?\) lSpaces\.SetHeadingToWorld\( ?lpWorld->"
                     r"GetPlayerLooseHeadingSpace\(\) ?\)", flat) is not None)
    yield ("the four setters are bodied inline in ICECameraSpaceHandler.hpp (their DWARF home, .hpp:82 / :86 / :90 / :95)",
           all(re.search(r"void\s+%s\s*\(\s*Matrix44Affine\s+\w+\s*\)\s*\{\s*%s\s*=\s*\w+\s*;\s*\}" % (name, member),
                         header) for name, member in (("SetCarToWorld", "mCarToWorld"),
                                                      ("SetCar2ToWorld", "mCar2ToWorld"),
                                                      ("SetHeadingToWorld", "mHeadingToWorld"),
                                                      ("SetHeading2ToWorld", "mHeading2ToWorld"))))


def numeric(tree):
    arm = arm_text(tree.read(ICEANIM_CPP))
    if arm is None:
        print("NUMERIC: the revision's BehaviourIceAnim::Update has no `ICE::CameraSpaceHandler lSpaces(` ... ShotContext")
        return None
    shadow = {relative: tree.read(relative) for relative in HEADERS} if tree.rev is not None else None
    with tempfile.TemporaryDirectory(prefix="brn_fxlastfix_spaces_") as directory:
        extra = []
        for relative in SOURCES:
            target = Path(directory) / Path(relative).name
            target.write_text(tree.read(relative), encoding="utf-8")
            extra.append(target)
        return compile_and_run(Path(__file__).with_name("FxLastfixIceSpaces.cpp"), "fxlastfix_iceanim_spaces_arm.inc",
                               "// GENERATED by run_fxlastfix_ice_spaces.py from " + ICEANIM_CPP + "\n" + arm + "\n",
                               "FxLastfixIceSpaces", shadow=shadow, extra_sources=tuple(extra))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxlastfix_ice_spaces", list(wiring(tree)), numeric(tree), numeric_total())


if __name__ == "__main__":
    sys.exit(main())
