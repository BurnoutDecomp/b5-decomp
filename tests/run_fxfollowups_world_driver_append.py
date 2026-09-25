"""FX-FOLLOWUPS (crash parity 2026-09-25, item 3): UpdateInputBuffer::AppendVehicleDriverInputInterface @0x823DB6C0.

The console body asserts the write lock and then calls VehicleDriverInputInterface::Append @0x823DB640 on the
+0x22BC0 member (0x823DB75C..0x823DB768): the update-driver queue merge, the :164 one-list assert and the adoption
of the source's target-assist list. The pre-fix PC body (BrnWorldModuleIO.cpp) merged the queue only.

tests/FxFollowupsWorldDriverAppend.cpp receives the revision's production body (pasted into a fixture
UpdateInputBuffer) and links the REAL BrnVehicleDriverInputInterface.cpp. Checks: adoption into an empty member
(count, ids, positions), the :164 assert when both sides carry a list, no adoption over an existing list, the queue
merge, and the lock assert.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxfollowups_world_driver_append.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, code_only, definition, compile_and_run, report

IO_CPP = "src/GameSource/World/BrnWorldModuleIO.cpp"
DRIVER_CPP = REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.cpp"
SIGNATURE = "void UpdateInputBuffer::AppendVehicleDriverInputInterface("
NUMERIC_CHECKS = 10


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read BrnWorldModuleIO.cpp from this git revision (RED: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read(IO_CPP)
    try:
        if not source:
            raise ValueError(IO_CPP + " is absent at this revision")
        body = definition(source, SIGNATURE)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return report("run_fxfollowups_world_driver_append", [], None, NUMERIC_CHECKS)
    wiring = [("the body hands the source to the member's own Append (bl 0x823DB640 at 0x823DB768)",
               re.search(r"mVehicleDriverInputInterface\s*\.\s*Append\s*\(\s*lpInterface\s*\)",
                         code_only(body)) is not None)]
    numeric = compile_and_run(Path(__file__).with_name("FxFollowupsWorldDriverAppend.cpp"), "fxfu_wdappend.inc",
                              body, "FxFollowupsWorldDriverAppend", extra_sources=(DRIVER_CPP, STRSTREAM_CPP))
    return report("run_fxfollowups_world_driver_append", wiring, numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
