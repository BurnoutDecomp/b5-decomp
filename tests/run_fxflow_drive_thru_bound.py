"""FX-FLOW (crash parity 2026-09-24, reviewer C item 1): DriveThruManager's just-armed re-arm bound.

  flt_82FADEC8 is .bss; the TU's dynamic initialiser @0x82C4D730..0x82C4D748 stores
  flt_82CDBD90 (2.1f) - flt_82001C98 (1.0f) == 0x3F8CCCCC into it (fsubs, single precision), and
  HandleDriveThru @0x8239B698 compares the entry countdown against it before re-arming the shop to
  1.0 on re-entry. The PC constant KF_DRIVE_THRU_JUST_ARMED_BOUND was a 0.0f placeholder, so a shop
  re-entered with its countdown in [0, 1.0) was not pushed back up to 1.0.

Numeric: tests/FxFlowDriveThruBound.cpp compiles every production `static const f32 KF_*` line of
BrnDriveThruManager.cpp and the production re-arm statement of HandleDriveThru.
Wiring: the bound is derived from the two image constants, not written as a literal.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_drive_thru_bound.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

DRIVE_THRU_CPP = "src/GameSource/GameState/Offences/BrnDriveThruManager.cpp"
NUMERIC_CHECKS = 8

CONSTANT = re.compile(r"^static const f32 KF_\w+\s*=[^;]*;", re.M)
REARM = re.compile(r"const f32 lfTimer = lrEntry\.mfTimeToActiveation;[\s\S]*?"
                   r"lrEntry\.mfTimeToActiveation = KF_DRIVE_THRU_REARM_TIME;")


def wiring(source):
    code = re.sub(r"\s+", "", code_only(source))
    yield ("KF_DRIVE_THRU_JUST_ARMED_BOUND is flt_82CDBD90 - flt_82001C98 (the initialiser's fsubs), "
           "not a literal",
           "staticconstf32KF_DRIVE_THRU_JUST_ARMED_BOUND=KF_DRIVE_THRU_ACTIVATION_TIME-KF_DRIVE_THRU_REARM_TIME;"
           in code)


def numeric(source):
    code = code_only(source)
    constants = CONSTANT.findall(code)
    rearm = REARM.search(code)
    if not constants or rearm is None:
        print("NUMERIC: cannot build -- the constants block or the re-arm statement is absent")
        return None
    return compile_and_run(Path(__file__).with_name("FxFlowDriveThruBound.cpp"), "drive_thru_constants.inc",
                           "\n".join(constants) + "\n", "FxFlowDriveThruBound",
                           extra_files={"drive_thru_rearm.inc": rearm.group(0) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    source = Tree(args.rev).read(DRIVE_THRU_CPP)
    return report("run_fxflow_drive_thru_bound", list(wiring(source)), numeric(source), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
