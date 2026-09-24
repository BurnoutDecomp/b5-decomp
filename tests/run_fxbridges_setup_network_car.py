"""FX-BRIDGES (crash parity 2026-09-24) header request H-GS1: SetupNetworkCarAction's console-only +0x38 f32.

X360 SetupNetworkCarAction::Construct @0x82355088 takes a seventh argument in f1 and stores it at +0x38
(`stfs f31, 0x38(r31)` @0x82355140): the ChangeNetworkCarEvent's +0x18 float (ProcessGameEvents case 7,
`lfs f31, 0x18(r25)` @0x823A17D0), which HandleSetupNetworkCarAction @0x82305880 turns into the network car's
mfBaseDeformAmount. The PC record had no such member and Construct no such parameter.

Structural: the header declares the member and the seven-parameter Construct; the production Construct stores it.
Compile+run: tests/FxBridgesSetupNetworkCar.cpp compiles the PRODUCTION Construct (BrnGameActions.cpp, by signature)
against the production header and checks all seven stores. On a revision without the parameter the fixture's
seven-argument call does not compile and the numeric checks count as failed.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxbridges_setup_network_car.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, extract, report

ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
ACTIONS_CPP = "src/GameSource/GameState/BrnGameActions.cpp"
SIGNATURE = "void SetupNetworkCarAction::Construct("
NUMERIC_CHECKS = 6


def wiring(tree):
    header = code_only(tree.read(ACTIONS_H))
    match = re.search(r"struct\s+alignas\(16\)\s+SetupNetworkCarAction\b[^{]*\{(.*?)\n\};", header, re.S)
    record = match.group(1) if match else ""
    yield ("SetupNetworkCarAction declares f32 mfBaseDeformationAmount (console +0x38)",
           re.search(r"\bf32\s+mfBaseDeformationAmount\s*;", record) is not None)
    yield ("Construct takes the f32 as its last parameter (f1 + the integer slot after r7)",
           re.search(r"CgsID\s+lWheelModelId\s*,\s*f32\s+lfBaseDeformationAmount\s*\)", record) is not None)
    texts, _ = extract(tree, ACTIONS_CPP, [SIGNATURE])
    body = code_only(texts[0]) if texts else ""
    yield ("the production Construct stores it (stfs f31, 0x38(r31) @0x82355140)",
           re.search(r"\bmfBaseDeformationAmount\s*=\s*lfBaseDeformationAmount\s*;", body) is not None)


def numeric(tree):
    texts, missing = extract(tree, ACTIONS_CPP, [SIGNATURE])
    if missing:
        print("NUMERIC: missing bodies: " + ", ".join(missing))
        return None
    shadow = {ACTIONS_H: tree.read(ACTIONS_H)} if tree.rev else None
    return compile_and_run(Path(__file__).with_name("FxBridgesSetupNetworkCar.cpp"),
                           "fxbridges_setup_network_car.inc", "\n".join(texts),
                           "FxBridgesSetupNetworkCar", shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxbridges_setup_network_car", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
