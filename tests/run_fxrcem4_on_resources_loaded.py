"""FX-RCEM4 (crash parity 2026-09-24, reviewer B on 87d1ad23 / G62): ActiveRaceCar::OnResourcesLoaded
@0x822EB168 -- the legs that read the car's streamed deformation spec through BrnPhysics::Def.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_on_resources_loaded.py [--pre-fix <b5 rev>]
Extracts ActiveRaceCar::OnResourcesLoaded and ResolveDeformationSpec (this TU's Def) from
BrnActiveRaceCar.cpp into tests/FxRcem4OnResourcesLoaded.cpp, which replays them against the real
StreamedDeformationSpec layout.
"""
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

ACTIVE = RCEM + "BrnActiveRaceCar.cpp"
RESOLVE = "ResolveDeformationSpec(const CgsResource::ResourceHandle& lrHandle)\n    {"


def main():
    rev = pre_fix_rev(sys.argv)
    active = read(ACTIVE, rev)
    pieces = {
        "fxrcem4_orl.inc": definition(active, "void ActiveRaceCar::OnResourcesLoaded("),
        "fxrcem4_orl_resolve.inc": ("const BrnPhysics::Deformation::StreamedDeformationSpec*\n"
                                    + definition(active, RESOLVE)),
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4OnResourcesLoaded.cpp", pieces, "fxrcem4_orl",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
