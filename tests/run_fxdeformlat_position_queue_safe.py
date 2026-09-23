"""Regression for PhysicalBodyPartPool::OutputEvents @0x8260DBE8 (crash parity G30-D1, FX-DEFORM-LAT).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdeformlat_position_queue_safe.py [--pre-fix <b5 rev>]

The console appends the detached-part CURRENT-POSITION event with the bounds-gated AddEventSafe
(0x8260DDAC bl 0x825E5B00: drops silently when the 50-slot queue is full); the tree used the
asserting, unconditional AddEvent, which on a full queue asserts and writes past maEvents[50]. The
shipped OutputEvents (+ GetGlobalEntityId) is extracted and run on the real types.
"""
import sys

sys.dont_write_bytecode = True
from pathlib import Path
from fxdeformlat_common import PHYS, REPO, build_and_run, definition, pre_fix_rev, read


def main():
    rev = pre_fix_rev(sys.argv)
    pool = read(PHYS + "/BrnPhysicalBodyPartPool.cpp", rev)
    part = read(PHYS + "/BrnPhysicalBodyPart.cpp", rev)
    inc = "\n".join(["namespace BrnPhysics { namespace Deformation {",
                     definition(part, "    EntityId PhysicalBodyPart::GetGlobalEntityId()"),
                     definition(pool, "    void PhysicalBodyPartPool::OutputEvents("),
                     "} }"])
    extra = [REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
    rc = build_and_run(Path(__file__).with_name("FxDeformLatPositionQueueSafe.cpp"), {"methods.inc": inc},
                       "fxdeformlat_position_queue_safe", extra_sources=extra, open_access=True)
    sys.exit(rc)


if __name__ == "__main__":
    main()
