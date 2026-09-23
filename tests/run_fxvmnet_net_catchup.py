"""Regression for crash-parity G44-D2 part 2 (FX-VMNET, 2026-09-23): the network catch-up stage --
VehicleManager::UpdateNetworkCatchup @0x82618E30 and PhysicsModule::UpdateNetworkCatchup @0x825A1508.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvmnet_net_catchup.py [--pre-fix <b5 rev>]

1. Structural: PhysicsModule::UpdateNetworkCatchup (BrnPhysicsModule.cpp) makes the console's calls in
   the console's order -- CheckState, the lpInputBuffer assert, LockForRead,
   GetVehicleDriverInterface, mVehicleManager.UpdateNetworkCatchup, UnlockForRead, CheckState -- and
   the WorldLinkStubs.cpp boot gate that stood for it is gone. Pre-fix the gate was the only
   definition.
2. FxVmnetNetCatchup.cpp compiles the shipped VehicleManager::UpdateNetworkCatchup
   (BrnVehicleManagerPlayerStats.cpp) against a real driver queue of real BrnNetworkDriverControls
   records. Pre-fix there was no body, so the harness runs one that does nothing.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxvmnet_common import REPO, build_and_run, definition, pre_fix_rev, read

STATS = "src/GameSource/Physics/VehicleManager/BrnVehicleManagerPlayerStats.cpp"
MODULE = "src/GameSource/Physics/BrnPhysicsModule.cpp"
STUBS = "src/GameSource/World/WorldLinkStubs.cpp"
SIGNATURE = "    void VehicleManager::UpdateNetworkCatchup("
NO_BODY = "void VmFixture::UpdateNetworkCatchup(const VehicleDriverInputInterface*) {}\n"

# The console order of 0x825A1508 (each token must appear, in this order, in the body text).
WRAPPER_ORDER = (
    "mVehicleManager.CheckState()",                      # 0x825A1528
    'CGS_ASSERT(lpInputBuffer != NULL, "lpInputBuffer != NULL")',   # 0x825A152C (:1049)
    "lpInputBuffer->LockForRead()",                      # 0x825A1558
    "lpInputBuffer->GetVehicleDriverInterface()",        # 0x825A1560
    "mVehicleManager.UpdateNetworkCatchup(",             # 0x825A156C
    "lpInputBuffer->UnlockForRead()",                    # 0x825A1574
    "mVehicleManager.CheckState()",                      # 0x825A157C
)


def in_order(text, tokens):
    position = 0
    for token in tokens:
        found = text.find(token, position)
        if found < 0:
            return False
        position = found + len(token)
    return True


def main():
    rev = pre_fix_rev(sys.argv)
    failures = []
    checks = 3

    try:
        wrapper = definition(read(MODULE, rev), "    void PhysicsModule::UpdateNetworkCatchup(")
        if not in_order(wrapper, WRAPPER_ORDER):
            failures.append("PhysicsModule::UpdateNetworkCatchup does not make the console's calls in order")
    except ValueError:
        failures.append("PhysicsModule::UpdateNetworkCatchup has no body in BrnPhysicsModule.cpp")

    if re.search(r"^void BrnPhysics::PhysicsModule::UpdateNetworkCatchup\(", read(STUBS, rev), re.M):
        failures.append("WorldLinkStubs.cpp still defines the PhysicsModule::UpdateNetworkCatchup boot gate")

    try:
        body = definition(read(STATS, rev), SIGNATURE).replace("VehicleManager::", "VmFixture::")
    except ValueError:
        failures.append("VehicleManager::UpdateNetworkCatchup has no body (harness runs a no-op)")
        body = NO_BODY

    rc = build_and_run(REPO / "tests" / "FxVmnetNetCatchup.cpp", {"net_catchup.inc": body}, "fxvmnet_netcatchup")
    for failure in failures:
        print("FAIL:", failure)
    print(f"structural/extraction: {len(failures)} of {checks} failed; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
