"""Regression for crash-parity G40-D2 / G43-D2 (FX-VMNET, 2026-09-23): the three post-scene
VehicleManager game-action leaves.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvmnet_mode_junkyard.py [--pre-fix <b5 rev>]

1. Structural: PhysicsModule::HandleGameActionsPostScene @0x825A70C0 hands the payload to
   VehicleManager::OnPrepareGameMode (jt case 0 = action 23, 0x825A7294), OnStartGameMode (case 11 =
   action 34, 0x825A72A4) and OnJunkYardDriveThru (case 76 = action 99, 0x825A72B4). Pre-fix all three
   arms were ReportDeferral one-shots.
2. FxVmnetModeJunkyard.cpp compiles the three shipped bodies (BrnVehicleManagerPlayerStats.cpp) onto a
   fixture and checks the console's stores / call / log lines. Pre-fix none of the bodies existed.
"""
import sys

sys.dont_write_bytecode = True
from fxvmnet_common import REPO, build_and_run, definition, pre_fix_rev, read

STATS = "src/GameSource/Physics/VehicleManager/BrnVehicleManagerPlayerStats.cpp"
MODULE = "src/GameSource/Physics/BrnPhysicsModule.cpp"
ACTIONS = "src/GameSource/GameState/BrnGameActions.cpp"

ARMS = (
    ("E_ACTION_PREPARE_FOR_MODE", "OnPrepareGameMode", "PrepareForModeAction", "G40-D2 action 23"),
    ("E_ACTION_START_PLAYING_MODE", "OnStartGameMode", "StartPlayingModeAction", "G40-D2 action 34"),
    ("E_ACTION_DRIVE_THRU_JUNK_YARD", "OnJunkYardDriveThru", "DriveThruJunkYardAction", "G43-D2 action 99"),
)


def arm_text(dispatch, case_label):
    """The text of one `case ...:` arm of the post-scene switch, up to the next case/default."""
    start = dispatch.index("case BrnGameState::GameStateModuleIO::" + case_label + ":")
    ends = [dispatch.find(marker, start + 1) for marker in ("\n                case ", "\n                default:")]
    return dispatch[start:min(e for e in ends if e > 0)]


def main():
    rev = pre_fix_rev(sys.argv)
    failures = []
    checks = 0

    module = read(MODULE, rev)
    dispatch = definition(module, "    void PhysicsModule::HandleGameActionsPostScene(")
    for case_label, method, payload, tag in ARMS:
        checks += 1
        arm = arm_text(dispatch, case_label)
        if ("mVehicleManager." + method + "(" not in arm or "lpEventData" not in arm or payload not in arm
                or "ReportDeferral" in arm):
            failures.append(f"{tag}: HandleGameActionsPostScene must call mVehicleManager.{method}(payload)")

    stats = read(STATS, rev)
    methods = []
    for _, method, _, tag in ARMS:
        checks += 1
        try:
            methods.append(definition(stats, "    void VehicleManager::" + method + "("))
        except ValueError:
            failures.append(f"{tag}: VehicleManager::{method} has no body")

    rc = 1
    if len(methods) == len(ARMS):
        getter = definition(read(ACTIONS), "const GameModeParams* PrepareForModeAction::GetGameModeParams() const")
        pieces = {
            "mode_junkyard_methods.inc": "\n".join(m.replace("VehicleManager::", "VmFixture::") for m in methods),
            "get_game_mode_params.inc": getter,
        }
        rc = build_and_run(REPO / "tests" / "FxVmnetModeJunkyard.cpp", pieces, "fxvmnet_mode")

    for failure in failures:
        print("FAIL:", failure)
    print(f"structural/extraction: {len(failures)} of {checks} failed; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
