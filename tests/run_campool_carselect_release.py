"""L1 CAMPOOL (owner's list 2026-09-27): ArbStateCarSelect::Release @0x82236050, the junkyard's camera behaviours.

  The console's car-select state overrides Release: meState = INACTIVE first (0x82236068), then nine inlined
  BehaviourHandle::Release bodies (+0x180, +0x20C, +0x25C, +0x234, +0x1A8, +0x1BC, +0x1F8, +0x1E4, +0x1D0), then
  CheckNoBehavioursAreAllocatedByState(info +0x18 manager, this). The PC had no override -- the export set files the
  function under BrnBehaviourManager.h, and the TU read its absence as "the base declaration stands" -- so the
  CHANGING_TO_ROAMING hand-off's virtual Release ran ArbitratorState::Release (`return true`) and every junkyard exit
  left the state's behaviours allocated: all three of the owner's crash dumps (exe d65db9997047) show ArbStateCarSelect
  still owning two IceAnim takes (two of the eight LARGE behaviour slots), a RotateAboutVehicle and an interpolator in
  free roam, which is part of why the large pool ran dry.

Numeric: tests/CampoolCarSelectRelease.cpp compiles the revision's Release body against the revision's header with
recording stand-ins for the two manager calls and checks the console's sequence. A revision without the override
cannot build it: every numeric check then fails.
Wiring: the header declares the override; the hand-off arm no longer stores INACTIVE itself (it moved into Release).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_campool_carselect_release.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

STATE_CPP = "src/GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.cpp"
STATE_H = "src/GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"
MANAGER_H = "src/GameSource/Director/Camera/BrnBehaviourManager.h"
NUMERIC_CHECKS = 26


def wiring(tree):
    header = code_only(tree.read(STATE_H))
    yield ("BrnArbStateCarSelect.h declares the Release override (@0x82236050)",
           re.search(r"bool\s+Release\s*\(\s*ArbStateSharedInfo\s*&\s*\w*\s*\)\s*override\s*;", header) is not None)
    source = tree.read(STATE_CPP)
    try:
        update = code_only(definition(source, "void ArbStateCarSelect::Update(ArbStateSharedInfo& lrSharedInfo)"))
    except ValueError:
        update = ""
    arm = update[update.find("E_STATE_CHANGING_TO_ROAMING:"):] if "E_STATE_CHANGING_TO_ROAMING:" in update else ""
    arm = arm[:arm.find("break;")] if "break;" in arm else arm
    yield ("the CHANGING_TO_ROAMING arm hands off through Release and stores no INACTIVE of its own",
           bool(arm) and "Release(lrSharedInfo)" in arm and "meState = E_STATE_INACTIVE" not in arm)


def numeric(tree):
    source = tree.read(STATE_CPP)
    try:
        body = definition(source, "bool ArbStateCarSelect::Release(ArbStateSharedInfo& lrSharedInfo)")
    except ValueError:
        print("NUMERIC: the revision has no ArbStateCarSelect::Release body")
        return None
    shadow = {relative: tree.read(relative) for relative in (STATE_H, MANAGER_H)} if tree.rev is not None else None
    return compile_and_run(Path(__file__).with_name("CampoolCarSelectRelease.cpp"), "campool_carselect.inc",
                           body + "\n", "CampoolCarSelectRelease", shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_campool_carselect_release", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
