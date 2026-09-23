"""Regression for crash-parity G44-D1 (FX-VMNET, 2026-09-23): the player-reset network hide in
VehicleManager::ProcessResetEvents.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvmnet_reset_hide.py [--pre-fix <b5 rev>]

The console (0x82617E2C..0x82617EEC) follows the RaceCarResetEvent post with
`if (idx == mePlayerActiveRaceCarIndex) { [log under gxMessageFilterFlags & 1] SetAllNetworkRaceCarsHidden(1); }`
and then a per-event HIDE_ONLINE stream (0x82617EF0..0x8261800C). The runner extracts, from the
production ProcessResetEvents body, everything between the AddRaceCarResetEvent block and the PC-only
[teleport] diagnostic -- the console's tail -- and FxVmnetResetHide.cpp runs it on a fixture.
Pre-fix that region is empty (park (P4)), so the harness compiles an empty tail.
"""
import sys

sys.dont_write_bytecode = True
from fxvmnet_common import REPO, build_and_run, definition, pre_fix_rev, read

SOURCE = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_WriteOutVehicleStats.cpp"
POST = "lpManagerOutputInterface->AddRaceCarResetEvent(lResult);"
BLOCK_END = "\n        }\n"
TELEPORT = "        if (CgsDev::Log::gpDebugPrint != 0)\n        {\n            const Matrix44Affine& lrSeated"


def main():
    rev = pre_fix_rev(sys.argv)
    failures = []
    body = definition(read(SOURCE, rev), "void VehicleManager::ProcessResetEvents(")

    # The tail sits after the closing brace of the `lpManagerOutputInterface != 0` post block (so the
    # hide does not depend on the PC-only null guard) and before the PC [teleport] diagnostic.
    post = body.index(POST)
    start = body.index(BLOCK_END, post) + len(BLOCK_END)
    end = body.index(TELEPORT, start)
    tail = body[start:end]

    checks = 1
    if "SetAllNetworkRaceCarsHidden(1)" not in tail:
        failures.append("G44-D1: no SetAllNetworkRaceCarsHidden(1) after the RaceCarResetEvent post (park (P4))")

    rc = build_and_run(REPO / "tests" / "FxVmnetResetHide.cpp", {"reset_tail.inc": tail}, "fxvmnet_reset")

    for failure in failures:
        print("FAIL:", failure)
    print(f"structural/extraction: {len(failures)} of {checks} failed; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
