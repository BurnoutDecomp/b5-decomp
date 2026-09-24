"""FX-FLOW (crash parity 2026-09-24, NEW-ACTION219): three game-action ids in the X360 +8 band.

  E_ACTION_SOUND_TRIGGER (DWARF 210), E_ACTION_ONLINE_PLAYER_ADDED (211) and
  E_ACTION_ONLINE_PLAYER_REMOVED (212) carried the raw PS3 DWARF values; on the X360 they are 218
  (TriggerQueryManager::PreWorldUpdate @0x8239F894), 219 (ProcessGameEvents case 127 @0x823A2390) and
  220 (case 129 @0x823A261C). The raw values are live X360 ids of other records --
  ALL_RIVALS_SHUTDOWN 210, the payback pair 211 / 212 -- so each record's type tag named another
  action.

Numeric: tests/FxFlowAction219.cpp reads the ids off the record types' GameAction<T> tags in the
revision's own BrnGameActions.h (shadowed in, so --rev sees that revision's header).
Wiring: no two EGameActionType enumerators share a value.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_action219.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
NUMERIC_CHECKS = 5


def enum_values(header):
    code = code_only(header)
    start = code.index("enum EGameActionType")
    body = code[start:code.index("};", start)]
    values = {}
    for match in re.finditer(r"^\s*(E_ACTION_\w+)\s*=\s*(\d+)", body, re.M):
        values.setdefault(int(match.group(2)), []).append(match.group(1))
    return values


def wiring(header):
    duplicates = {value: names for value, names in enum_values(header).items() if len(names) > 1}
    if duplicates:
        print("  duplicate EGameActionType values: " + ", ".join(
            "%d = %s" % (value, " / ".join(names)) for value, names in sorted(duplicates.items())))
    yield ("EGameActionType has no two enumerators on one value (the record tags are unambiguous)",
           not duplicates)


def numeric(header):
    return compile_and_run(Path(__file__).with_name("FxFlowAction219.cpp"), "unused.inc", "\n", "FxFlowAction219",
                           shadow={ACTIONS_H: header})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    header = Tree(args.rev).read(ACTIONS_H)
    return report("run_fxflow_action219", list(wiring(header)), numeric(header), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
