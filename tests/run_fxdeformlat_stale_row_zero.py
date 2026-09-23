"""Regression for DeformationManager::ProcessAddDeformationModelEvents @0x82644828
(crash parity G24-D2, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_stale_row_zero.py [--pre-fix <b5 rev>]

After DeformableObject::Prepare both consoles zero ONE loop-invariant 16-byte row at
manager + 0x60 + 0x700*model (X360 0x82644DB8..0x82644DDC, PS3 0x76AF48..0x76AF74; a 20-sensor clear
whose store indexes the MODEL). The tree stored nothing. The shipped statements after the Prepare
call are extracted and run on a sentinel-filled real DeformationManager for models 0..27.
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import DEFORM, build_and_run, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    manager = read(DEFORM + "/BrnDeformationManager.cpp", rev)
    fn = manager.index("    void DeformationManager::ProcessAddDeformationModelEvents(")
    prep = manager.index("&mDetachedWheelManager, mRandom);", fn) + len("&mDetachedWheelManager, mRandom);")
    end = manager.index("\n        }\n    }", prep)   # the end of the event loop body
    block = manager[prep:end]
    rc = build_and_run(Path(__file__).with_name("FxDeformLatStaleRowZero.cpp"), {"block.inc": block},
                       "fxdeformlat_stale_row_zero", open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
