"""FX-DIRECTOR2 (crash parity 2026-09-25): Camera::BehaviourRig re-derived from the X360.

  The rig camera films the takedown look-back (moment type 3) and the jump cutaway (type 7). Every body the tree held
  was a paraphrase: Parameters::Construct seeded invented tunings, Construct skipped half the members, SetParameters
  dropped the debug name, and Update never composed the camera transform at all -- it looked the cars up and threw
  the answers away. The bodies now follow the console: Parameters::Construct @0x821F9680, Construct @0x82242488,
  SetParameters @0x821F3B10, Prepare @0x821F9798, Update @0x822427C0, SetupTweaker @0x821F9870, GetName @0x821F99F0,
  GetCollisionPolicy, AttachToRaceCar / SetDetached.

Numeric: tests/FxDirector2BehaviourRig.cpp compiles the revision's BehaviourRig.cpp, Behaviour.cpp, the rig's
CameraRig::Construct partfile and presets, Spring1D, the two lags, DepthOfField, VehicleRef and the VehicleInfo /
RaceCarState copies, placement-news the behaviour into pool garbage and drives it through a Behaviour*. The camera the
default block produces on a car at the origin is compared with the CONSOLE's own CameraRig::Construct output
(FxDirector2CameraRigGolden.h). A revision without the rig bodies cannot build it: every numeric check then fails.
Wiring: the exe mounts BehaviourRig.cpp and the rig's two new TUs.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_behaviour_rig.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import WORKFLOW, Tree, compile_and_run, report

RIG_CPP = "src/GameSource/Director/Camera/Behaviours/BehaviourRig.cpp"
SOURCES = (
    RIG_CPP,
    "src/GameSource/Director/Camera/Behaviours/Behaviour.cpp",
    "src/GameSource/Director/Camera/Utils/BrnCameraRigConstruct.cpp",
    "src/GameSource/Director/Camera/Utils/BrnCameraRigParams.cpp",
    "src/GameSource/Director/Camera/Utils/BrnOrientationLag.cpp",
    "src/GameSource/Director/Camera/Utils/BrnPositionLag.cpp",
    "src/GameSource/Director/Camera/BrnDepthOfField.cpp",
    "src/GameSource/Director/Utils/BrnVehicleRef.cpp",
    "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.cpp",
    "src/GameSource/Physics/PhysicsUtilities/Spring1D.cpp",
    "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp",
)
# The headers the sources above compile against, read from the revision under test (--rev) so its bodies meet
# their own declarations.
HEADERS = (
    "src/GameSource/Director/Camera/Behaviours/BehaviourRig.h",
    "src/GameSource/Director/Camera/Behaviours/Behaviour.h",
    "src/GameSource/Director/Camera/Utils/BrnOrientationLag.h",
    "src/GameSource/Director/Camera/Utils/BrnPositionLag.h",
    "src/GameSource/Director/Camera/Utils/BrnLooker.h",
    "src/GameSource/Director/Camera/Utils/BrnCameraShake.h",
    "src/GameSource/Director/Camera/Utils/CameraUtils.h",
    "src/GameSource/Director/Camera/Utils/BrnConsoleVpu.h",
    "src/SDKs/XboxMath/XMVectorSinCos.h",
    "src/GameSource/Director/Camera/BrnDepthOfField.h",
    "src/GameSource/Director/Camera/BrnCollisionPolicy.h",
    "src/GameSource/Director/Camera/BrnCameraState.h",
    "src/GameSource/Director/Camera/Camera.h",
    "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h",
    "src/GameSource/Director/Utils/BrnVehicleRef.h",
    "src/GameSource/Director/Utils/BrnDirectorAllVehicleData.h",
    "src/GameSource/Director/Utils/BrnDirectorTimestep.h",
    "src/GameSource/Physics/PhysicsUtilities/Spring1D.h",
    "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h",
)
BUILD_BAT = WORKFLOW / "tools/build/build_game_exe.bat"
NUMERIC_CHECKS = 48


def wiring(tree):
    bat = BUILD_BAT.read_text(encoding="utf-8", errors="replace") if BUILD_BAT.exists() else ""
    for relative in ("Behaviours\\BehaviourRig", "Utils\\BrnCameraRigConstruct", "Utils\\BrnCameraRigParams"):
        yield (f"the exe mounts Director\\Camera\\{relative}.cpp (tools/build/build_game_exe.bat)",
               f'echo "%SRC%\\GameSource\\Director\\Camera\\{relative}.cpp"' in bat)


def numeric(tree):
    texts = {relative: tree.read(relative) for relative in SOURCES}
    missing = [relative for relative, text in texts.items() if not text]
    if missing:
        print("NUMERIC: the revision lacks " + ", ".join(missing))
        return None
    if "void CameraRig::Construct(" not in texts["src/GameSource/Director/Camera/Utils/BrnCameraRigConstruct.cpp"]:
        print("NUMERIC: the revision has no CameraRig::Construct body")
        return None
    with tempfile.TemporaryDirectory(prefix="brn_fxd2_rig_") as directory:
        work = Path(directory)
        extra = []
        for relative, text in texts.items():
            target = work / Path(relative).name
            target.write_text(text, encoding="utf-8")
            extra.append(target)
        shadow = {relative: tree.read(relative) for relative in HEADERS} if tree.rev is not None else None
        return compile_and_run(Path(__file__).with_name("FxDirector2BehaviourRig.cpp"), "fxd2_rig_unused.inc", "",
                               "FxDirector2BehaviourRig", shadow=shadow, extra_sources=tuple(extra))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_behaviour_rig", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
