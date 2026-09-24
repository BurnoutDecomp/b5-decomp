"""FX-RCEM4 (crash-parity 2026-09-24): the car-colour chain -- CHAIN-RECOLOUR + the G61-D6 colour leg.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_recolour.py [--pre-fix <b5 rev>]

Extracts, verbatim, from the shipped sources (or from git revision <rev>) and replays against the
fixtures in tests/FxRcem4Recolour.cpp:
  * burnoutcargraphicsasset::PlayerColourIndex / PlayerColourPaletteIndex (AttribSys generated header)
  * ActiveRaceCar::OnResourcesLoaded (BrnActiveRaceCar.cpp) -- its default-colour leg 0x822EB474
  * RaceCarEntityModule::SetupCarColour @0x822F5170 and IsCarColourInUse @0x822D2E68 (+ their
    TU-local DWARF constants) from BrnRaceCarEntityModule.cpp
  * RaceCarEntityModule::GetRandomCarColour @0x822EA088 (+ KAI_DECENT_AI_CAR_COLOURS) from the Range TU
  * the taken-down block of ProcessRaceCarCrashCompleteEvents (BrnRaceCarEntityModule_CrashExit.cpp)
    and RaceCar::IncreasePersistentDamage (BrnRaceCar.cpp)
A piece the source lacks is replayed as an empty stub. Structural: OnRaceCarResourcesLoaded calls
SetupCarColour between ActiveRaceCar::OnResourcesLoaded and PlaceRaceCarOnLoad (0x822FEDC4 / 0x822FEDD0
/ 0x822FEDDC), and both TU-local KI_BLACK_CAR_COLOUR_INDEX copies are 6.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

GFX = "src/GameSource/AttribSys/Generated/classes/burnoutcargraphicsasset.h"
BLOCK_START = "// 0x822F40C8..0x822F4390 -- the taken-down AI persistent-damage block."
BLOCK_END = "// 0x822F4394 -- the merge point of both arms; always cleared."


def constant(source, pattern):
    found = re.search(r"^\s*(const (?:s32|f32) " + pattern + r"\s*=[^;]+;)", source, re.M)
    return found.group(1) if found else None


def main():
    rev = pre_fix_rev(sys.argv)
    gfx = read(GFX, rev)
    active = read(RCEM + "BrnActiveRaceCar.cpp", rev)
    module = read(RCEM + "BrnRaceCarEntityModule.cpp", rev)
    rangetu = read(RCEM + "BrnRaceCarEntityModule_Range.cpp", rev)
    crashexit = read(RCEM + "BrnRaceCarEntityModule_CrashExit.cpp", rev)
    racecar = read(RCEM + "BrnRaceCar.cpp", rev)
    pieces, failures = {}, []

    # ---- the two AttribSys accessors -------------------------------------------------------------
    accessors = []
    for name in ("PlayerColourIndex", "PlayerColourPaletteIndex"):
        body = optional_definition(gfx, "inline const s32& burnoutcargraphicsasset::" + name + "() const")
        print(("found   " if body else "MISSING ") + "burnoutcargraphicsasset::" + name)
        accessors.append(body or ("inline const s32& burnoutcargraphicsasset::" + name
                                  + "() const { static const s32 kiMissing = -99; return kiMissing; }"))
    pieces["fxrcem4_gfx_accessors.inc"] = "\n".join(accessors)

    # ---- ActiveRaceCar::OnResourcesLoaded -----------------------------------------------------------
    pieces["fxrcem4_on_resources_loaded.inc"] = definition(active, "void ActiveRaceCar::OnResourcesLoaded(")

    # ---- RaceCar::IncreasePersistentDamage (landed G67-D1; the block needs it) ----------------------
    consts = [l for l in racecar.splitlines() if l.startswith("static const f32 KF_PERSISTENT_DAMAGE_")]
    pieces["fxrcem4_increase.inc"] = "\n".join(consts) + "\n" + definition(racecar, "bool RaceCar::IncreasePersistentDamage()")

    # ---- the module functions and their DWARF constants --------------------------------------------
    parts = []
    black = constant(module, "KI_BLACK_CAR_COLOUR_INDEX") or constant(crashexit, "KI_BLACK_CAR_COLOUR_INDEX")
    parts.append(black or "const s32 KI_BLACK_CAR_COLOUR_INDEX = -99;   // MISSING")
    parts.append(constant(module, "KF_MIN_COLOUR_SPACE_DISTANCE") or "const f32 KF_MIN_COLOUR_SPACE_DISTANCE = -99.0f;")
    table = re.search(r"^\s*(const s32 KAI_DECENT_AI_CAR_COLOURS\[\]\s*=\s*\{[^}]*\};)", rangetu, re.M)
    parts.append(table.group(1) if table else "const s32 KAI_DECENT_AI_CAR_COLOURS[] = { -99 };")
    parts.append(constant(rangetu, "KI_NUM_AI_CAR_COLOURS") or "const s32 KI_NUM_AI_CAR_COLOURS = 1;")
    stubs = {
        "void RaceCarEntityModule::SetupCarColour(": (module, "void RaceCarEntityModule::SetupCarColour(EActiveRaceCarIndex) {}"),
        "bool RaceCarEntityModule::IsCarColourInUse(": (module, "bool RaceCarEntityModule::IsCarColourInUse(s32, s32) { return false; }"),
        "s32 RaceCarEntityModule::GetRandomCarColour(": (rangetu, "s32 RaceCarEntityModule::GetRandomCarColour(s32, s32) { return -1; }"),
    }
    for signature, (source, stub) in stubs.items():
        body = optional_definition(source, signature)
        print(("found   " if body else "MISSING ") + signature.split("::")[1].rstrip("("))
        parts.append(body or stub)
    pieces["fxrcem4_module.inc"] = "\n\n".join(parts)

    # ---- the taken-down block -------------------------------------------------------------------------
    if BLOCK_START in crashexit and BLOCK_END in crashexit:
        start = crashexit.index(BLOCK_START)
        pieces["fxrcem4_block.inc"] = crashexit[start:crashexit.index(BLOCK_END, start)]
    else:
        failures.append("ProcessRaceCarCrashCompleteEvents has no taken-down persistent-damage block")
        pieces["fxrcem4_block.inc"] = ""

    # ---- structural ------------------------------------------------------------------------------------
    loaded = code_mask(definition(module, "void RaceCarEntityModule::OnRaceCarResourcesLoaded("))
    order = [loaded.find("->OnResourcesLoaded("), loaded.find("SetupCarColour("), loaded.find("PlaceRaceCarOnLoad(")]
    if -1 in order or not (order[0] < order[1] < order[2]):
        failures.append("OnRaceCarResourcesLoaded must call SetupCarColour(index) between ActiveRaceCar::"
                        "OnResourcesLoaded and PlaceRaceCarOnLoad (bl @0x822FEDD0)")
    for label, source in (("BrnRaceCarEntityModule.cpp", module), ("BrnRaceCarEntityModule_CrashExit.cpp", crashexit)):
        value = constant(source, "KI_BLACK_CAR_COLOUR_INDEX")
        if value is None or not re.search(r"=\s*6\s*;", value):
            failures.append(label + " must carry DWARF :306 KI_BLACK_CAR_COLOUR_INDEX = 6 (li r11/r17, 6)")

    rc = build_and_run(REPO / "tests" / "FxRcem4Recolour.cpp", pieces, "fxrcem4_recolour",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
