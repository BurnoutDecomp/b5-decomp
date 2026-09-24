"""FX-RCEM4 (crash parity 2026-09-24, G61-D4): ActiveRaceCar::Update's start-line boost flame
(ARTIST 0x822F7C58..0x822F7D14) and the module's mNonDeterministicRandom (+0x18490): seeded in
RaceCarEntityModule::Construct, stepped once per PreSceneUpdate (0x8230D9D8..0x8230DA10), handed to
Update by UpdateActiveCars.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_start_line_boost.py [--pre-fix <b5 rev>]
Extracts the block from ActiveRaceCar::Update into tests/FxRcem4StartLineBoost.cpp (a missing block is
replayed as nothing) and checks the three module legs structurally.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, pre_fix_rev, read

CAR = RCEM + "BrnActiveRaceCar.cpp"
MODULE = RCEM + "BrnRaceCarEntityModule.cpp"
MARKER = "if( IsOnRaceStartState( E_RACE_START_STATE_ON_START_LINE ) )"


def main():
    rev = pre_fix_rev(sys.argv)
    update = definition(read(CAR, rev), "void ActiveRaceCar::Update(f32 lfTimeStep,")
    at = update.find(MARKER)
    block = definition(update[at:], MARKER) if at >= 0 else ""
    print(("found   " if block else "MISSING ") + "the start-line block in ActiveRaceCar::Update")

    module = read(MODULE, rev)
    construct = code_mask(definition(module, "void RaceCarEntityModule::Construct()"))
    prescene = code_mask(definition(module, "void RaceCarEntityModule::PreSceneUpdate("))
    active = code_mask(definition(module, "void RaceCarEntityModule::UpdateActiveCars("))
    legs = {
        "Construct seeds it": re.search(r"mNonDeterministicRandom\.Construct\(\)", construct),
        "PreSceneUpdate steps it right after the locks":
            re.search(r"lpOutput->LockForWrite\(\);\s*\(void\)mNonDeterministicRandom\.RandomUInt\(\);", prescene),
        "UpdateActiveCars passes it to Update": re.search(r"&mNonDeterministicRandom\s*\)", active),
    }
    for name, hit in legs.items():
        print(("ok      " if hit else "MISSING ") + name)
    structural = all(legs.values())
    pieces = {
        "fxrcem4_slb_block.inc": block,
        "fxrcem4_slb_struct.inc": "static const bool gbStructuralOk = " + ("true" if structural else "false") + ";",
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4StartLineBoost.cpp", pieces, "fxrcem4_slb",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
                                      REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
