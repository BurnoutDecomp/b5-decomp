"""FX-DIRECTOR (crash parity 2026-09-24): Camera::BehaviourBystanderCam is a real Camera::Behaviour.

  The moment tick's first live run AV'd in BehaviourHelper::Prepare @0x82255F48: it dispatches the pooled
  behaviour's vtable slot 0, and BehaviourBystanderCam -- MomentBystanderSeesAction's shot -- was a HOLLOW
  SHELL. The tree carried three definitions of it (BehaviourBystanderCam.h, a byte-span layout with every
  sub-object opaque and a detail:: shim layer that was never defined; BrnBehaviourBystanderCam.h, a
  SetParameters/SetTarget slice; BrnBehaviourBystanderCamSerialise.h, a Parameters view) and none derived
  from Camera::Behaviour. It is now ONE class, re-based on the DWARF (BehaviourBystanderCam.h:107
  `: public Behaviour`, members :156..:175, Parameters :181..:198), with its ARTIST bodies: Construct
  @0x822438E8, Prepare @0x821F9AD0, Update @0x82243C80, GetCollisionPolicy @0x821F9B28, SetupTweaker
  @0x821F9B30, GetName @0x821F9C78, Parameters::Construct @0x821F9A00, SetParameters @0x821F3F10,
  SetTarget @0x821F3F80. With it: the bank constructs its four bystander blocks (0x8223DC90),
  PositionFinder::Construct, VehicleRef::SetToPlayer and SetToRaceCar's h:222 assert (@0x821F29D8),
  Behaviour::VehicleRef::GetVehicle/GetTransform, the two CameraUtils range tests, and
  Behaviour::SetCantSwitchToMeNow loses an assert the console does not have.

Numeric: tests/FxDirectorBystanderCam.cpp compiles the revision's BehaviourBystanderCam.cpp,
Behaviour.cpp, BrnPositionFinder.cpp and BrnVehicleRef.cpp (plus the two range tests extracted from its
CameraUtils.cpp) against the revision's own headers, placement-news the behaviour into raw storage and
drives it through a Behaviour* (the manager's dispatch). A revision whose class is a hollow shell cannot
build it: every numeric check is then counted as failed.
Wiring: one definition deriving from Behaviour, the bank constructs its blocks, the exe mounts the TUs,
no detail:: shims, the base's SetCantSwitchToMeNow asserts nothing, the moment forwards to the behaviour.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_bystander_cam.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, WORKFLOW, Tree, compile_and_run, definition, report

DIRECTOR = "src/GameSource/Director"
HEADER = f"{DIRECTOR}/Camera/Behaviours/BehaviourBystanderCam.h"
SOURCE = f"{DIRECTOR}/Camera/Behaviours/BehaviourBystanderCam.cpp"
BEHAVIOUR_CPP = f"{DIRECTOR}/Camera/Behaviours/Behaviour.cpp"
FINDER_CPP = f"{DIRECTOR}/Camera/Utils/BrnPositionFinder.cpp"
VEHICLE_REF_CPP = f"{DIRECTOR}/Utils/BrnVehicleRef.cpp"
PASSENGER_CPP = f"{DIRECTOR}/Camera/Behaviours/BehaviourPassengerCam.cpp"
CAMERA_UTILS_CPP = f"{DIRECTOR}/Camera/Utils/CameraUtils.cpp"
BANK_H = f"{DIRECTOR}/Camera/BrnBehaviourParameterBank.h"
MOMENT_CPP = f"{DIRECTOR}/MomentController/Moments/BrnMomentBystanderSeesAction.cpp"
BUILD_BAT = WORKFLOW / "tools/build/build_game_exe.bat"
# Headers a --rev run must see at that revision (the retired forks included, so an old tree's
# includers still resolve).
SHADOW_HEADERS = (
    HEADER,
    f"{DIRECTOR}/Camera/Behaviours/Behaviour.h",
    f"{DIRECTOR}/Camera/Behaviours/BrnBehaviourBystanderCam.h",
    f"{DIRECTOR}/Camera/Behaviours/BrnBehaviourBystanderCamSerialise.h",
    BANK_H,
    f"{DIRECTOR}/Camera/Utils/BrnPositionFinder.h",
    f"{DIRECTOR}/Camera/Utils/CameraUtils.h",
    f"{DIRECTOR}/Camera/Utils/BrnLooker.h",
    f"{DIRECTOR}/Utils/BrnVehicleRef.h",
    f"{DIRECTOR}/Camera/BrnCollisionPolicy.h",
    f"{DIRECTOR}/Camera/Behaviours/BehaviourPassengerCam.h",
    f"{DIRECTOR}/Camera/Utils/BrnCameraImpactEffect.h",
)
RANGE_TESTS = ("bool TargetOutsideRange(Vector3 lPosition", "bool TargetWillExceedRangeInXSecs(Vector3 lPosition")
NUMERIC_CHECKS = 65

_TOKENS = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|' + r"'(?:\\.|[^'\\\n])*'")


def code_only(text):
    def keep(match):
        token = match.group(0)
        if token.startswith("//"):
            return ""
        if token.startswith("/*"):
            return "\n" * token.count("\n") or " "
        return token
    return _TOKENS.sub(keep, text)


def squash(text):
    return re.sub(r"\s+", "", text)


def body(source, signature):
    try:
        return squash(code_only(definition(source, signature)))
    except ValueError:
        return ""


def wiring(tree):
    header = squash(code_only(tree.read(HEADER)))
    yield ("BehaviourBystanderCam derives from Camera::Behaviour (DWARF BehaviourBystanderCam.h:107)",
           "classBehaviourBystanderCam:publicBehaviour{" in header)
    definitions = []
    for path in (REPO / DIRECTOR).rglob("*.h"):
        relative = path.relative_to(REPO).as_posix()
        text = squash(code_only(tree.read(relative))) if tree.rev else squash(code_only(
            path.read_text(encoding="utf-8-sig", errors="replace")))
        if re.search(r"(?:class|struct)BehaviourBystanderCam(?::|\{)", text):
            definitions.append(relative)
    if tree.rev:
        # the retired forks no longer exist on disk; ask the revision for them directly
        for relative in (f"{DIRECTOR}/Camera/Behaviours/BrnBehaviourBystanderCam.h",
                         f"{DIRECTOR}/Camera/Behaviours/BrnBehaviourBystanderCamSerialise.h"):
            if relative not in definitions and re.search(r"(?:class|struct)BehaviourBystanderCam(?::|\{)",
                                                         squash(code_only(tree.read(relative)))):
                definitions.append(relative)
    yield (f"exactly one definition of Camera::BehaviourBystanderCam under GameSource/Director "
           f"({', '.join(sorted(definitions)) or 'none'})", len(definitions) == 1)
    source = code_only(tree.read(SOURCE))
    yield ("BehaviourBystanderCam.cpp reaches the real callees (no `namespace detail` shim layer)",
           bool(source) and "namespacedetail" not in squash(source))
    bank = squash(code_only(tree.read(BANK_H)))
    yield ("the bank constructs its four bystander blocks (0x8223DE20..) and copies Close from Far (0x8223E260) "
           "instead of zeroing them",
           all(f"{name}.Construct();" in bank for name in ("mBystanderJumpLeftParameters",
                                                            "mBystanderJumpFromBehindParameters",
                                                            "mBystanderCloseParameters", "mBystanderFarParameters"))
           and "mBystanderCloseParameters=mBystanderFarParameters;" in bank and "ZeroBlock(&mBystander" not in bank)
    behaviour = tree.read(BEHAVIOUR_CPP)
    set_cant = body(behaviour, "void Behaviour::SetCantSwitchToMeNow(")
    yield ("Behaviour::SetCantSwitchToMeNow asserts nothing (its text is not in the image; the inlined copies "
           "have no assert)", bool(set_cant) and "CGS_ASSERT" not in set_cant)
    moment = body(tree.read(MOMENT_CPP), "void MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor(")
    yield ("MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor forwards to the behaviour's own "
           "(the inlined h:268 does the compare)",
           "->SetPerceivedDistanceModificationFactor(lfFactor);" in moment
           and "GetPerceivedDistanceModificationFactor" not in moment)
    bat = BUILD_BAT.read_text(encoding="utf-8", errors="replace")
    yield ("the exe mounts BehaviourBystanderCam.cpp (its vtable slots must link) -- tools/build/build_game_exe.bat",
           'echo "%SRC%\\GameSource\\Director\\Camera\\Behaviours\\BehaviourBystanderCam.cpp"' in bat)
    yield ("the exe mounts BrnPositionFinder.cpp (Construct / FindPosition / Update) -- tools/build/build_game_exe.bat",
           'echo "%SRC%\\GameSource\\Director\\Camera\\Utils\\BrnPositionFinder.cpp"' in bat)


def numeric(tree, overrides=None):
    # --old <src path>=<rev> swaps ONE production TU for its text at that revision (e.g. the pre-fix
    # Behaviour.cpp against everything else current), to show which checks that one body fails.
    overrides = overrides or {}
    sources = {name: (Tree(overrides[path]) if path in overrides else tree).read(path)
               for name, path in (("BehaviourBystanderCam.cpp", SOURCE),
                                  ("Behaviour.cpp", BEHAVIOUR_CPP),
                                  ("BrnPositionFinder.cpp", FINDER_CPP),
                                  ("BrnVehicleRef.cpp", VEHICLE_REF_CPP),
                                  ("BehaviourPassengerCam.cpp", PASSENGER_CPP))}
    missing = [name for name, text in sources.items() if not text]
    utils = tree.read(CAMERA_UTILS_CPP)
    range_tests = []
    for signature in RANGE_TESTS:
        try:
            range_tests.append(definition(utils, signature))
        except ValueError:
            missing.append(signature)
    if missing:
        print("NUMERIC: the revision lacks " + ", ".join(missing))
        return None
    with tempfile.TemporaryDirectory(prefix="brn_bystander_") as directory:
        work = Path(directory)
        for name, text in sources.items():
            (work / name).write_text(text, encoding="utf-8")
        shadow = {path: tree.read(path) for path in SHADOW_HEADERS if tree.read(path)} if tree.rev else None
        return compile_and_run(Path(__file__).with_name("FxDirectorBystanderCam.cpp"), "bystander_range_tests.inc",
                               "\n\n".join(range_tests), "FxDirectorBystanderCam", shadow=shadow,
                               extra_sources=tuple(work / name for name in sources))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--old", action="append", default=[], metavar="PATH=REV",
                        help="read one production TU (src-relative path) from REV instead")
    args = parser.parse_args()
    tree = Tree(args.rev)
    overrides = dict(item.split("=", 1) for item in args.old)
    return report("run_fxdirector_bystander_cam", list(wiring(tree)), numeric(tree, overrides), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
