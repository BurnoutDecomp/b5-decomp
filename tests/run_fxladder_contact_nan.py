"""FX-LADDER: the two NaN-only arms of the race-car/race-car contact chain.

  DeformationManager::GetInterpolatedContactPointAndNormal @0x82604948 -- the boundary-neighbour tests
    0x82604AE8 (neighbour 0) and 0x82604B2C (neighbour 1) are `vcmpgtfp. 0 > dot ; beq accept`, so a
    NaN dot ACCEPTS the neighbour (and runs the tangent arm).
  DeformationSensor::ValidateAndAddContact @0x825E1788 -- 0x825E1B5C `vmaxfp128 t, 0, ratio` keeps a NaN
    ratio, so the latch's `1.0 >= t` (0x825E1B78) and the return's (0x825E1CB8) both refuse it.

Extracts the production bodies -- GetInterpolatedContactPointAndNormal, CalculateTangentPoints and the
fix-up TU's helpers; ValidateAndAddContact and its TU's helpers; the DeformableObject accessors they
call -- and runs FxLadderContactNan.cpp's checks on zero-filled storage of the REAL DeformationManager /
DeformableObject / VehiclePhysics / DeformationSensor types.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_contact_nan.py [--pre-fix <b5 rev>]

`--pre-fix <b5 rev>` reads the three sources from that revision (the RED side of the fix).
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import DEFORM, PHYS, REPO, anonymous_namespace, build_and_run, definition, pre_fix_rev, read


def includes(source):
    """The source file's own #include lines, so each extracted body sees the headers it was written against."""
    return "\n".join(line for line in source.splitlines() if line.startswith("#include"))


def main():
    rev = pre_fix_rev(sys.argv)
    fixup = read(DEFORM + "/BrnDeformationManager_VehicleContactFixUp.cpp", rev)
    accessors = read(PHYS + "/BrnDeformableObject_Accessors.cpp", rev)
    vac = read(PHYS + "/BrnDeformationSensor_ValidateAndAddContact.cpp", rev)
    fixup_piece = "\n".join([
        includes(fixup), includes(accessors),
        "namespace BrnPhysics { namespace Deformation {",
        anonymous_namespace(fixup),
        definition(fixup, "    void DeformationManager::CalculateTangentPoints("),
        definition(fixup, "    bool DeformationManager::GetInterpolatedContactPointAndNormal("),
        definition(accessors, "    Vehicle::VehiclePhysics* DeformableObject::GetVehiclePhysics()"),
        definition(accessors, "    void DeformableObject::GetInverseTransform("),
        "} }",
    ])
    vac_piece = "\n".join([
        includes(vac),
        "namespace BrnPhysics { namespace Deformation {",
        anonymous_namespace(vac),
        definition(vac, "\tbool DeformationSensor::ValidateAndAddContact("),
        "} }",
    ])
    rc = build_and_run(Path(__file__).with_name("FxLadderContactNan.cpp"),
                       {"fixup_piece.cpp": fixup_piece, "vac_piece.cpp": vac_piece},
                       "fxladder_contact_nan",
                       extra_sources=["fixup_piece.cpp", "vac_piece.cpp",
                                      REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"],
                       open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
