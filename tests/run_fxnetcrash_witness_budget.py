"""crash parity FX-NETCRASH (2026-09-25), item 3: the posted=0 witness budget.

CrashModule::HandleNetworkCrashingTraffic (ARTIST 0x827CB788) carries a capped FLAG PC witness
(BRN_NETCRASH_DIAG) -- "[netcrash] HandleNetworkCrashingTraffic player= updates= posted= ...". It had ONE budget
of KI_NETCRASH_DIAG_MAX_LINES lines. On the pair's guest in run fxnetcrash_pair/20260925_131154 the 40 lines all
went to frames whose updates were contentious (posted=0: each half's own lockstep swerve crash), so a later frame
that applied the other player's wreck could never print. The witness now keeps two budgets: posted >= 1 frames
and posted == 0 frames.

NUMERIC: the production witness block and constant, compiled into FxNetcrashWitnessBudget.cpp.
WIRING: the block sits in HandleNetworkCrashingTraffic after the cleared-up loop, before
OnContactFromNetworkPlayer.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_witness_budget.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

RACE_CPP = "src/GameSource/World/CrashModule/BrnCrashModule_RaceCarCrashes.cpp"
HANDLE = "void CrashModule::HandleNetworkCrashingTraffic("
WITNESS = "if( NetCrashDiagEnabled() && CgsDev::Log::gpDebugPrint && lpCrashingTrafficQueue->GetLength() > 0 )"
NUMERIC_CHECKS = 9


def handler(tree):
    source = tree.read(RACE_CPP).replace("\r\n", "\n")
    try:
        return source, definition(source, HANDLE)
    except ValueError:
        return source, ""


def numeric(tree):
    source, body = handler(tree)
    constant = re.search(r"const s32 KI_NETCRASH_DIAG_MAX_LINES\s*=\s*[^;]+;", source)
    if not body or WITNESS not in body or constant is None:
        print("NUMERIC: cannot build -- the witness block or its constant is absent")
        return None
    block = definition(body, WITNESS)
    return compile_and_run(Path(__file__).with_name("FxNetcrashWitnessBudget.cpp"), "budget_block.inc", block,
                           "FxNetcrashWitnessBudget", extra_files={"budget_constant.inc": constant[0] + "\n"})


def wiring(tree):
    _, body = handler(tree)
    code = code_only(body)
    witness_at = code.find(WITNESS)
    cleared_at = code.find("SetNetworkVehicleClearedUp();")
    contact_at = code.find("OnContactFromNetworkPlayer( leActiveRaceCarIndex );")
    return [
        ("the witness sits in HandleNetworkCrashingTraffic after the cleared-up loop and before "
         "OnContactFromNetworkPlayer (reads only; the console order of the calls is unchanged)",
         0 <= cleared_at < witness_at < contact_at),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_witness_budget", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
