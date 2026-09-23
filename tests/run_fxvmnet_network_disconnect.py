"""Regression for crash-parity G41-D1 (FX-VMNET, 2026-09-23): VehicleManager::ProcessNetworkCarDisconnect.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvmnet_network_disconnect.py [--pre-fix <b5 rev>]

1. Structural: PhysicsModule::HandleGameActions @0x825A72F0 jump-table case 4 (action 11) @0x825A7834
   hands the payload to VehicleManager::ProcessNetworkCarDisconnect (`mr r4,r29 ; addi r3,r31,0x4AA0 ;
   bl 0x825C53F8`). Pre-fix the arm was a one-shot "[s3-action] id 11 DEFERRED" log.
2. FxVmnetNetworkDisconnect.cpp compiles the shipped body (BrnVehicleManagerPlayerStats.cpp) and the
   shipped VehicleDriver::ClearControls (BrnVehicleDriver.cpp) onto the real VehicleDriver[8] /
   BitArray<8> types and compares the result with the console's inlined store run. Pre-fix no body
   existed, so the harness is compiled against what the old arm did at runtime: nothing.
"""
import sys

sys.dont_write_bytecode = True
from fxvmnet_common import REPO, build_and_run, definition, pre_fix_rev, read

STATS = "src/GameSource/Physics/VehicleManager/BrnVehicleManagerPlayerStats.cpp"
ACTIONS = "src/GameSource/Physics/BrnPhysicsModuleGameActions.cpp"
DRIVER = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.cpp"

SIGNATURE = "    void VehicleManager::ProcessNetworkCarDisconnect("
# What the pre-fix arm did at runtime: log once, touch nothing. Only used when the body is absent.
OLD_ARM_BEHAVIOUR = (
    "void VmFixture::ProcessNetworkCarDisconnect(\n"
    "    const BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedAction*) {}\n")


def arm_text(dispatch, case_label):
    """The text of one `case ...:` arm of the pre-scene switch, up to the next case/default."""
    start = dispatch.index("case " + case_label + ":")
    ends = [dispatch.find(marker, start + 1) for marker in ("\n                case ", "\n                default:")]
    return dispatch[start:min(e for e in ends if e > 0)]


def main():
    rev = pre_fix_rev(sys.argv)
    failures = []
    checks = 0

    checks += 1
    dispatch = definition(read(ACTIONS, rev), "    void PhysicsModule::HandleGameActions(")
    arm = arm_text(dispatch, "KI_ACTION_NETWORK_CAR_DISCONNECT")
    if ("mVehicleManager.ProcessNetworkCarDisconnect(" not in arm or "lpEventData" not in arm
            or "RemotePlayerDisconnectedAction" not in arm or "DEFERRED" in arm):
        failures.append("G41-D1 action 11: HandleGameActions must call mVehicleManager.ProcessNetworkCarDisconnect(payload)")

    checks += 1
    try:
        body = definition(read(STATS, rev), SIGNATURE).replace("VehicleManager::", "VmFixture::")
    except ValueError:
        failures.append("G41-D1: VehicleManager::ProcessNetworkCarDisconnect has no body "
                        "(harness runs the old arm's behaviour: no work)")
        body = OLD_ARM_BEHAVIOUR

    pieces = {
        "network_disconnect.inc": body,
        "clear_controls.inc": definition(read(DRIVER, rev), "    void VehicleDriver::ClearControls()"),
    }
    rc = build_and_run(REPO / "tests" / "FxVmnetNetworkDisconnect.cpp", pieces, "fxvmnet_disconnect")

    for failure in failures:
        print("FAIL:", failure)
    print(f"structural/extraction: {len(failures)} of {checks} failed; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
