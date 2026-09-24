"""FX-BRIDGES (crash parity 2026-09-24): SoundTriggerAction::IsEmpty @0x82355178's tolerance is the image's.

IsEmpty compares |mQueryPos| lane-wise against the float at 0x82029BA4 -- image-read 0x34000000 == 1.1920929e-07
(FLT_EPSILON). BrnGameActions.cpp carried 1.0e-4f as an "UNCONFIRMED stand-in", so a query position with a lane up
to 1e-4 was reported empty (TriggerQueryManager::PostWorldUpdate @0x82386BD8 is the caller).

Numeric: tests/FxBridgesSoundTriggerEmpty.cpp compiles the PRODUCTION IsEmpty and the PRODUCTION constant (both
extracted from BrnGameActions.cpp) against the real SoundTriggerAction.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_sound_trigger_empty.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

ACTIONS_CPP = "src/GameSource/GameState/BrnGameActions.cpp"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
NUMERIC_CHECKS = 6


def epsilon_line(source):
    match = re.search(r"^\s*const float KF_QUERY_POS_EPSILON = [^;]+;", code_only(source), re.M)
    return match.group(0).strip() if match else None


def wiring(tree):
    line = epsilon_line(tree.read(ACTIONS_CPP)) or ""
    yield ("the IsEmpty tolerance is the image's 0x82029BA4 (0x34000000 == 1.1920928955078125e-07f)",
           "1.1920928955078125e-07f" in line)


def numeric(tree):
    source = tree.read(ACTIONS_CPP)
    line = epsilon_line(source)
    try:
        body = definition(source, "bool SoundTriggerAction::IsEmpty()")
    except ValueError:
        body = None
    if line is None or body is None:
        print("NUMERIC: missing the constant or the body")
        return None
    shadow = {ACTIONS_H: tree.read(ACTIONS_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxBridgesSoundTriggerEmpty.cpp"),
                           "fxbridges_sound_trigger_empty.inc", body + "\n", "FxBridgesSoundTriggerEmpty",
                           shadow=shadow,
                           extra_files={"fxbridges_sound_trigger_epsilon.inc": "namespace {\n" + line + "\n}\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_sound_trigger_empty", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
