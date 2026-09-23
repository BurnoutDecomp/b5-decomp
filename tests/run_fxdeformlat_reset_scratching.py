"""Regression for DeformationManager::ProcessEvents @0x82644E38 / DeformableObject::ResetScratching
(crash parity G24-D1, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_reset_scratching.py [--pre-fix <b5 rev>]

On a player-scratch reset (game action 98, the paint-shop respray) the console zeroes every spec
sensor's mfScratchAmount (0x82644ED0..0x82644F04, the inlined ResetScratching; PS3 0x6B9D7C) BEFORE
UpdateIK re-blends it. The tree skipped the zeroing, so the scratches survived. The shipped
ProcessEvents and (when present) ResetScratching are extracted and run on the real types.
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import DEFORM, PHYS, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    manager = read(DEFORM + "/BrnDeformationManager.cpp", rev)
    update = read(PHYS + "/BrnDeformableObject_Update.cpp", rev)
    try:
        reset = definition(update, "    void DeformableObject::ResetScratching()")
    except ValueError:
        reset = ""   # pre-fix: no body anywhere (and ProcessEvents does not call it)
    inc = "\n".join(["namespace BrnPhysics { namespace Deformation {",
                     reset,
                     definition(manager, "    void DeformationManager::ProcessEvents("),
                     "} }"])
    rc = build_and_run(Path(__file__).with_name("FxDeformLatResetScratching.cpp"), {"methods.inc": inc},
                       "fxdeformlat_reset_scratching", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
