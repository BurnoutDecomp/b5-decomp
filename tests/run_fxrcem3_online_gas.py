"""Regression for crash-parity G67-D9 (FX-RCEM3, 2026-09-23): the ONLINE_RACE catch-up gas of
RaceCarEntityModule::ProcessPlayerVehicleInput (ARTIST 0x82300530..0x82300588), extracted from
BrnRaceCarEntityModule.cpp (the `if( lpScoring->miNumPlayersInGame > 1 )` block) and compared bit for
bit against the console arithmetic over every reachable (position, players) state
(FxRcem3OnlineGas.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem3_online_gas.py [--pre-fix <b5 rev>]
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

MODULE = RCEM + "BrnRaceCarEntityModule.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    process = definition(read(MODULE, rev), "void RaceCarEntityModule::ProcessPlayerVehicleInput(")
    block = definition(process, "if( lpScoring->miNumPlayersInGame > 1 )")
    rc = build_and_run(REPO / "tests" / "FxRcem3OnlineGas.cpp", {"fxrcem3_online_gas.inc": block},
                       "fxrcem3_online_gas")
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
