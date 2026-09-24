"""FX-DIRECTOR (crash parity 2026-09-24): Camera::BehaviourFixedCam is a real Camera::Behaviour.

  The moment tick's first live run AV'd in BehaviourHelper::Prepare @0x82255F48: it dispatches the pooled
  behaviour's vtable slot 0, and the behaviours the moments pool included two HOLLOW SHELLS -- classes
  with no Behaviour base and no vtable. BehaviourFixedCam (MomentStaticCamImpact's shot) was one: it
  modelled only SetParameters' two stores. It is now re-based on the DWARF (BrnBehaviourFixedCam.h:52
  `: public Behaviour`, members :90..:97) with its ARTIST bodies: Construct @0x82229D20, Prepare
  @0x821FAD28, Update @0x82229DE0, GetCollisionPolicy @0x821FB588, SetupTweaker @0x821FAD48, GetName
  @0x821FAE48, Parameters::Construct (inlined by the bank @0x8223DC90), and the bank seeds its block.

Numeric: tests/FxDirectorFixedCam.cpp compiles the revision's BrnBehaviourFixedCam.cpp + Behaviour.cpp
against the revision's own header, placement-news the behaviour into raw storage and drives it through
a Behaviour* (the manager's dispatch). A revision whose class is a hollow shell cannot build it: every
numeric check is then counted as failed.
Wiring: the class derives from Behaviour, it has ONE definition, the bank constructs its block, and the
exe mounts its TU.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_fixed_cam.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from fxgs_common import REPO, WORKFLOW, Tree, compile_and_run, report

HEADER = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
SOURCE = "src/GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp"
BEHAVIOUR_CPP = "src/GameSource/Director/Camera/Behaviours/Behaviour.cpp"
BANK_H = "src/GameSource/Director/Camera/BrnBehaviourParameterBank.h"
POLICY_H = "src/GameSource/Director/Camera/BrnCollisionPolicy.h"
BUILD_BAT = WORKFLOW / "tools/build/build_game_exe.bat"
NUMERIC_CHECKS = 31

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


def wiring(tree):
    header = squash(code_only(tree.read(HEADER)))
    yield ("BehaviourFixedCam derives from Camera::Behaviour (DWARF BrnBehaviourFixedCam.h:52)",
           "classBehaviourFixedCam:publicBehaviour{" in header)
    definitions = []
    for path in (REPO / "src/GameSource/Director").rglob("*.h"):
        relative = path.relative_to(REPO).as_posix()
        text = squash(code_only(tree.read(relative))) if tree.rev else squash(code_only(
            path.read_text(encoding="utf-8-sig", errors="replace")))
        if re.search(r"(?:class|struct)BehaviourFixedCam(?::|\{)", text):
            definitions.append(relative)
    yield (f"exactly one definition of Camera::BehaviourFixedCam under GameSource/Director ({', '.join(definitions) or 'none'})",
           len(definitions) == 1)
    bank = squash(code_only(tree.read(BANK_H)))
    yield ("the bank constructs its fixed-cam block (the inlined Parameters::Construct at 0x8223DC90) instead of "
           "zeroing it", "mFixedDefault.Construct();" in bank and "ZeroBlock(&mFixedDefault" not in bank)
    policy = squash(code_only(tree.read(POLICY_H)))
    yield ("VisibilityCollisionPolicy::SetTestLookingAt / IsVisibilityInterrupted have bodies (the fixed cam's "
           "Construct and Update inline both)",
           "voidSetTestLookingAt(boollbTestLookingAt){" in policy and "boolIsVisibilityInterrupted()const{" in policy)
    bat = BUILD_BAT.read_text(encoding="utf-8", errors="replace")
    yield ("the exe mounts BrnBehaviourFixedCam.cpp (its vtable slots must link) -- tools/build/build_game_exe.bat",
           'echo "%SRC%\\GameSource\\Director\\Camera\\Behaviours\\BrnBehaviourFixedCam.cpp"' in bat)


def numeric(tree):
    source = tree.read(SOURCE)
    behaviour = tree.read(BEHAVIOUR_CPP)
    if not source or not behaviour:
        print("NUMERIC: the revision lacks the behaviour TU")
        return None
    with tempfile.TemporaryDirectory(prefix="brn_fixedcam_") as directory:
        work = Path(directory)
        (work / "BrnBehaviourFixedCam.cpp").write_text(source, encoding="utf-8")
        (work / "Behaviour.cpp").write_text(behaviour, encoding="utf-8")
        shadow = {HEADER: tree.read(HEADER), POLICY_H: tree.read(POLICY_H)} if tree.rev else None
        return compile_and_run(Path(__file__).with_name("FxDirectorFixedCam.cpp"), "fixed_cam_unused.inc", "",
                               "FxDirectorFixedCam", shadow=shadow,
                               extra_sources=(work / "BrnBehaviourFixedCam.cpp", work / "Behaviour.cpp"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_fixed_cam", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
