"""Regression for DeformableObject::UpdateSkinningOffsets @0x825DFA90 (crash parity G17-D3, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_skinning_gather.py [--pre-fix <b5 rev>]

The console tests the skinned flag of the tag at the RUNNING ROW (r9 advances only in the skinned
arm, 0x825DFB44) while reading the data of the tag at the loop index (r10, 0x825DFB80): it gathers
only the leading run of skinned tags. The tree tested maTagPoints[li]. The shipped
UpdateSkinningOffsets and its three file-scope constants are extracted and run on the real types.
"""
import re
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    update = read(PHYS + "/BrnDeformableObject_Update.cpp", rev)
    constants = [re.search(r"const s32 KI_BODY_PART_BOX_CLAMPED_A\s*=[^;]+;", update).group(0),
                 re.search(r"const s32 KI_BODY_PART_BOX_CLAMPED_B\s*=[^;]+;", update).group(0),
                 re.search(r"const Vector3 KVF_SKINNING_CLAMP_MARGIN\s*=[^;]+;", update).group(0)]
    inc = "\n".join([
        "namespace BrnPhysics { namespace Deformation {",
        "namespace {", *constants, "}",
        definition(update, "    void DeformableObject::UpdateSkinningOffsets()"),
        "} }",
    ])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatSkinningGather.cpp"), {"methods.inc": inc},
                       "fxdeformlat_skinning_gather", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
