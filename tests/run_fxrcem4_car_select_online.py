"""FX-RCEM4 (crash parity 2026-09-24, G60-D6): ActiveRaceCar::UpdateCarSelectStateOnline (ARTIST
0x822BFA30), InputBuffer_PreScene::GetCarSelectStatus / IsCarSelectStatusValid (header inlines) and
RaceCarEntityModule::PreSceneUpdate's per-slot car-select leg (0x8230E118..0x8230E170).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_car_select_online.py [--pre-fix <b5 rev>]
Extracts the pieces into tests/FxRcem4CarSelectOnline.cpp. A piece the source lacks (or only declares)
is replayed as a no-op.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, optional_definition, pre_fix_rev, read

CAR = RCEM + "BrnActiveRaceCar.cpp"
IO = RCEM + "BrnRaceCarEntityModuleIO.h"
MODULE = RCEM + "BrnRaceCarEntityModule.cpp"


def bodied(source, signature):
    """The brace-balanced definition starting at `signature` when it is DEFINED there, else ''."""
    start = source.find(signature)
    while start >= 0:
        close = source.find(")", start)
        tail = re.sub(r"//[^\n]*", "", source[close + 1:close + 400]) if close >= 0 else ""
        if re.match(r"\s*(const\s*)?\{", tail):
            return definition(source[start:], signature)
        start = source.find(signature, start + 1)
    return ""


def main():
    rev = pre_fix_rev(sys.argv)
    update = optional_definition(read(CAR, rev), "void ActiveRaceCar::UpdateCarSelectStateOnline(")
    print(("found   " if update else "MISSING ") + "ActiveRaceCar::UpdateCarSelectStateOnline")
    io = read(IO, rev)
    get = bodied(io, "bool GetCarSelectStatus(EActiveRaceCarIndex")
    valid = bodied(io, "bool IsCarSelectStatusValid(EActiveRaceCarIndex")
    print(("found   " if get else "MISSING ") + "GetCarSelectStatus body")
    print(("found   " if valid else "MISSING ") + "IsCarSelectStatusValid body")
    presene = definition(read(MODULE, rev), "void RaceCarEntityModule::PreSceneUpdate(")
    leg = ""
    marker = presene.find("if (lpInput->IsCarSelectStatusValid(leActivateSlot))")
    if marker >= 0:
        leg = definition(presene[marker:], "if (lpInput->IsCarSelectStatusValid(leActivateSlot))")
    print(("found   " if leg else "MISSING ") + "PreSceneUpdate car-select leg")
    pieces = {
        "fxrcem4_cs_update.inc": update or "void ActiveRaceCar::UpdateCarSelectStateOnline(bool) {}",
        "fxrcem4_cs_accessors.inc": (get or "bool GetCarSelectStatus(EActiveRaceCarIndex) const { return false; }") + "\n"
                                    + (valid or "bool IsCarSelectStatusValid(EActiveRaceCarIndex) const { return false; }"),
        "fxrcem4_cs_leg.inc": leg or "(void)lpActivateCar; (void)leActivateSlot; (void)lpInput;",
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4CarSelectOnline.cpp", pieces, "fxrcem4_cs",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
