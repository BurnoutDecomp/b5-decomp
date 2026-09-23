"""Regression for crash-parity G68-D1 / G68-D2 (FX-RCEM3, 2026-09-23): the production
RaceCarEntityModule::HandleResetPlayerCarAction (ARTIST 0x82304FE8), extracted from
BrnRaceCarEntityModule.cpp and run against a fixture module (FxRcem3ResetPlayerCar.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem3_reset_player_car.py [--pre-fix <b5 rev>]

  G68-D1: the re-spawn re-attaches the new car to the player's old slot (r27 @0x823051CC).
  G68-D2: a teleport-only record calls RequestPlaceOnTrack(wAxis, zAxis, 0.0f) @0x82305544.
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

MODULE = RCEM + "BrnRaceCarEntityModule.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    body = definition(read(MODULE, rev), "void RaceCarEntityModule::HandleResetPlayerCarAction(")
    rc = build_and_run(REPO / "tests" / "FxRcem3ResetPlayerCar.cpp",
                       {"fxrcem3_reset_player_car.inc": body}, "fxrcem3_reset_player_car",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
