"""FX-BRIDGES (crash parity 2026-09-24) header request H-PP1: BrnWorld::EPowerParkOutcome has one home.

src/GameSource/GameState/BrnGameActions.h carried a PROVISIONAL mirror of the enum "until the PowerParking TU lands";
BrnPowerParkingManager.h (DWARF :48) landed in b5 fd8d4ce1 / 259fa839, so every TU seeing both failed C2011 -- the
Power Parking producer (TrafficEntityModule) and consumer (RaceCarEntityModule) could not include the manager.

Structural: BrnGameActions.h defines no EPowerParkOutcome and includes the DWARF home.
Compile+run: tests/FxBridgesPowerParkEnum.cpp includes BOTH headers (the C2011 case) and pins the enum. On a
revision whose BrnGameActions.h still carries the mirror, the TU does not compile and the checks count as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_power_park_enum.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
NUMERIC_CHECKS = 3


def wiring(tree):
    text = code_only(tree.read(ACTIONS_H))
    yield ("BrnGameActions.h defines no EPowerParkOutcome of its own (one home: BrnPowerParkingManager.h:48)",
           re.search(r"\benum\s+EPowerParkOutcome\b", text) is None)
    yield ("BrnGameActions.h includes the DWARF home BrnPowerParkingManager.h",
           '#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"' in text)


def numeric(tree):
    shadow = {ACTIONS_H: tree.read(ACTIONS_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxBridgesPowerParkEnum.cpp"), "fxbridges_unused.inc", "",
                           "FxBridgesPowerParkEnum", shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read BrnGameActions.h from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_power_park_enum", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
