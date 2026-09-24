"""FX-RCEM4 (crash parity 2026-09-24, CC-3 = G60-D3): ActiveRaceCar::GetPropCollisionBox (ARTIST
0x822D3DB0) and RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene (0x822F5668), called by
PreSceneUpdate at 0x8230E408 between WriteUpdatedAIData and the output-interface fetches: a car in the
scene whose body deformed this frame has its dynamic scene volume replaced (64-bit key) by a box built
from its DEFORMED bbox.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_prop_box.py [--pre-fix <b5 rev>]
A missing body is replayed as a stand-in that does nothing (returns NULL / posts nothing).
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, definition, optional_definition, pre_fix_rev, read

CAR = RCEM + "BrnActiveRaceCar.cpp"
MODULE = RCEM + "BrnRaceCarEntityModule.cpp"


def main():
    rev = pre_fix_rev(sys.argv)
    car = optional_definition(read(CAR, rev), "rw::collision::BoxVolume* ActiveRaceCar::GetPropCollisionBox(")
    print(("found   " if car else "MISSING ") + "ActiveRaceCar::GetPropCollisionBox")
    if not car:
        car = "rw::collision::BoxVolume* ActiveRaceCar::GetPropCollisionBox(void*) { return nullptr; }"

    module_text = read(MODULE, rev)
    update = optional_definition(module_text, "void RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene(")
    print(("found   " if update else "MISSING ") + "RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene")
    if not update:
        update = ("void RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene("
                  "RaceCarEntityModuleIO::OutputBuffer_PreScene*) {}")

    prescene = code_mask(definition(module_text, "void RaceCarEntityModule::PreSceneUpdate("))
    structural = re.search(r"WriteUpdatedAIData\(\s*lpOutput\s*\);\s*UpdatePropBoundingBoxes_PreScene\(\s*lpOutput\s*\);"
                           r"\s*UpdateOutputInterfaces\(", prescene) is not None
    print(("ok      " if structural else "MISSING ") + "the PreSceneUpdate call between WriteUpdatedAIData and UpdateOutputInterfaces")

    pieces = {
        "fxrcem4_pb_car.inc": car,
        "fxrcem4_pb_module.inc": update,
        "fxrcem4_pb_struct.inc": "static const bool gbStructuralOk = " + ("true" if structural else "false") + ";",
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4PropBox.cpp", pieces, "fxrcem4_pb",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
