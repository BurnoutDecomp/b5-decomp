"""FX-LADDER item 1: the car-vs-car impact ladder's decisions on RECORDED contacts.

Extracts the production VehicleManager::CheckForAllTypesOfImpacts @0x82642E58 and its eight rungs
from BrnVehicleManager.cpp, plus the production callees the rungs read through
(HasRaceCarHadRecentImpact @0x825B4EB8, IsPointBetweenTwoParallelPlanes @0x825C5660,
CheckForVerticalTakedownSituation @0x825C56D8, ShouldRaceCarCrashOnCarImpact @0x825C6FF8,
VehiclePhysics::IsBeingSlamedOrShunted @0x825E6D50 / ...ByRaceCar @0x82615290,
ShuntEffect::IsActive @0x8236AFC0), and replays the contacts FxLadderReplay.cpp carries. Each
record is one player-involved ladder evaluation captured bit for bit by the [td-replay] witness
(BRN_TD_DIAG=1 BRN_TD_REPLAY=1) in live runs; the fixture asserts the console's decision for it,
derived gate by gate from the ARTIST asm (the deciding gate and its address are on every record).
The only doubles are the two commits the ladder calls, InstantTakedown and ApplyShunt, which just
record their arguments.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_replay.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, body_or_stub, compile_and_run, constant, definition, parse_args  # noqa: E402

VEHICLE_MANAGER = "src/GameSource/Physics/VehicleManager/BrnVehicleManager.cpp"
IMPACT_HELPERS = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_ImpactHelpers.cpp"
PLAYER_STATS = "src/GameSource/Physics/VehicleManager/BrnVehicleManagerPlayerStats.cpp"
TRAFFIC_CONTACT = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_RaceCarTrafficContact.cpp"
VEHICLE_PHYSICS = "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
SHUNT_EFFECT = "src/GameSource/Physics/VehicleManager/VehiclePhysics/ShuntEffect.cpp"

RUNGS = ("CheckForPlayerSlammingAIIntoAI", "CheckForHittingAlreadyCrashingCar", "CheckForVerticalTakedown",
         "CheckForTBoneTakedown", "CheckForHeadToHead", "CheckForShuntAndNudge",
         "CheckForSlamAndTradingPaint", "CheckForStationaryTargetTakedown")


def main():
    args = parse_args()
    tree = Tree(args.rev)
    manager = tree.read(VEHICLE_MANAGER)

    # The TU head: its includes, the file-local vector helpers and constants, and the open
    # `namespace BrnPhysics { namespace Vehicle {` every later chunk lives in.
    chunks = [manager[:manager.index("    // [td-contact] / [td-crash]")]]
    # The witness helpers CheckForAllTypesOfImpacts names (its witness stays off: no gpDebugPrint).
    for signature in ("    static bool TakedownDiagEnabled()", "    static const char* WitnessImpactTypeName("):
        chunks.append(definition(manager, signature))
    # (the replay witness's own switch; older revisions have none)
    chunks.append(body_or_stub(manager, "    static bool TakedownReplayEnabled()",
                               "    static bool TakedownReplayEnabled() { return false; }"))
    start = manager.index("    static const f32 KF_SPEED_UNIT_SCALE")
    chunks.append(manager[start:manager.index("    // EntityId packing helper", start)])
    chunks.append(definition(manager, "    static inline EntityId MakeRaceCarEntityId("))
    for rung in RUNGS:
        chunks.append(definition(manager, "    bool VehicleManager::" + rung + "("))
    chunks.append(definition(manager, "    void VehicleManager::CheckForAllTypesOfImpacts("))

    helpers = tree.read(IMPACT_HELPERS)
    chunks.append("namespace {\n" + definition(helpers, "        inline f32 SignOrZero(") + "\n}")
    chunks.append(definition(helpers, "    bool VehicleManager::IsPointBetweenTwoParallelPlanes("))
    chunks.append(definition(helpers, "    bool VehicleManager::CheckForVerticalTakedownSituation("))
    chunks.append(definition(tree.read(PLAYER_STATS), "    bool VehicleManager::HasRaceCarHadRecentImpact("))

    traffic = tree.read(TRAFFIC_CONTACT)
    chunks.append("namespace {\n" + definition(traffic, "    bool TrafficDiagEnabled()") + "\n"
                  + constant(traffic, "KF_MAX_IMPACT_MASS") + "\n}")
    chunks.append(definition(traffic, "bool VehicleManager::ShouldRaceCarCrashOnCarImpact("))

    physics = tree.read(VEHICLE_PHYSICS)
    chunks.append(definition(physics, "    bool VehiclePhysics::IsBeingSlamedOrShunted() const"))
    chunks.append(definition(physics, "    bool VehiclePhysics::IsBeingSlamedOrShuntedByRaceCar("))
    chunks.append(definition(tree.read(SHUNT_EFFECT), "    bool VehiclePhysics::ShuntEffect::IsActive() const"))
    chunks.append("} }")
    sys.exit(compile_and_run("FxLadderReplay.cpp", chunks, prefix="brn_fxladder_replay_"))


if __name__ == "__main__":
    main()
