"""Regression for crash-parity G60-D1 / G60-D2 / G61-D5 (FX-RCEM3, 2026-09-23): the race-car turn
indicators.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem3_indicators.py [--pre-fix <b5 rev>]

  * ActiveRaceCar::SetIndicatorState (0x822A52B0) and ActiveRaceCar::UpdateIndicators (0x822A5340)
    are extracted from BrnActiveRaceCar.cpp and run on a fixture car (FxRcem3Indicators.cpp). A
    source without UpdateIndicators (pre-fix) is replayed as an empty body -- nothing wrote the
    render bits.
  * Structural: ActiveRaceCar::Update calls UpdateIndicators(lfTimeStep) after UpdateInAirRotations
    (0x822F7E74..0x822F7E7C, f1 = f31 = Update's lfTimeStep).
The action-276 producer arm is covered by run_fxrcem3_game_actions.py.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import (RCEM, REPO, build_and_run, code_mask, definition, optional_definition,
                            pre_fix_rev, read)

SOURCE = RCEM + "BrnActiveRaceCar.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    text = read(SOURCE, rev)
    set_state = definition(text, "void ActiveRaceCar::SetIndicatorState(")
    update = optional_definition(text, "void ActiveRaceCar::UpdateIndicators(")
    if not update:
        print("MISSING ActiveRaceCar::UpdateIndicators -- replayed as an empty body")
        update = "void ActiveRaceCar::UpdateIndicators(f32) {}"

    failures = []
    body = code_mask(definition(text, "void ActiveRaceCar::Update("))
    in_air = body.find("UpdateInAirRotations(")
    call = re.search(r"\bUpdateIndicators\(\s*lfTimeStep\s*\)", body)
    if not call or in_air < 0 or call.start() < in_air:
        failures.append("G61-D5 ActiveRaceCar::Update must call UpdateIndicators(lfTimeStep) after "
                        "UpdateInAirRotations (bl @0x822F7E7C, f1 = f31)")

    rc = build_and_run(REPO / "tests" / "FxRcem3Indicators.cpp",
                       {"fxrcem3_indicators.inc": set_state + "\n" + update}, "fxrcem3_indicators")
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
