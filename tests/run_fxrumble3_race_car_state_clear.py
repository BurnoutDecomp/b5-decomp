"""FX-RUMBLE3 (crash parity 2026-09-24): RaceCarState::Clear @0x8229FFC8 stores the invalid entity id at +968.

The console body ends its re-init with `lwz r8, dword_82CDB5A0` / `stw r8, 0x3C8(r31)` (0x822A0098..0x822A00A0).
0x3C8 == 968 is mEntityId (the u64 mCarAssetAttribKey @960 pushed mfSpeedMPH to @972), and 0x82CDB5A0 image-reads
0xFFFFFFFF with no CRT writer (findinit: two readers) -- the DWARF's const EntityId K_INVALID_ENTITY_ID. The PC wrote a
"compile-safe placeholder" 0.0f into mfSpeedMPH instead and left mEntityId at the memset's 0.

Numeric: tests/FxRumble3RaceCarStateClear.cpp compiles the PRODUCTION Clear (and the file-local constants block it
reads) against the real BrnVehicleEvents.h, on storage pre-filled with 0xCD.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrumble3_race_car_state_clear.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, definition, compile_and_run, report

EVENTS_CPP = "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"
NUMERIC_CHECKS = 11
CLEAR_SIGNATURE = "void RaceCarState::Clear()"


def clear_body(source):
    try:
        return definition(source, CLEAR_SIGNATURE)
    except ValueError:
        return None


def wiring(tree):
    body = code_only(clear_body(tree.read(EVENTS_CPP)) or "")
    yield ("Clear stores K_INVALID_ENTITY_ID into mEntityId (+0x3C8, 0x822A00A0)",
           "mEntityId.muValue = CgsSceneManager::K_INVALID_ENTITY_ID" in body)
    yield ("Clear no longer writes a placeholder into mfSpeedMPH (+0x3CC is never stored)",
           "mfSpeedMPH" not in body)


def numeric(tree):
    source = tree.read(EVENTS_CPP)
    body = clear_body(source)
    try:
        constants = definition(source, "namespace\n{")
    except ValueError:
        constants = ""
    if body is None:
        print("NUMERIC: RaceCarState::Clear is missing")
        return None
    text = constants + "\nnamespace BrnPhysics\n{\nnamespace Vehicle\n{\n" + body + "\n}\n}\n"
    return compile_and_run(Path(__file__).with_name("FxRumble3RaceCarStateClear.cpp"),
                           "fxrumble3_race_car_state_clear.inc", text, "FxRumble3RaceCarStateClear")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxrumble3_race_car_state_clear", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
