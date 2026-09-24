"""FX-AINAN2: NaN branch polarity in the AICar bodies (BrnAICar.cpp / BrnAICar_Update.cpp).

Extracts the production AICar::UpdateRelativePositionToPlayer, UpdateRaceDistance,
UpdateRouteFindingRace, CheckForSectionChange (BrnAICar.cpp) and AICar::Update, HasValidRoute,
CheckForFreeRoamSwapToPursuit, UpdatePositionOutOfRange, IsFreeRoamingCarsRouteOld,
ComputeDistanceToCheckpoint, UpdateOutOfRangeData, OnModeStart plus the TU helper IsZeroVmx
(BrnAICar_Update.cpp), and feeds them NaNs. See FxAinan2AICar.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_aicar.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix); a
revision without IsZeroVmx gets nothing in its place (its bodies call the shared vpu::IsZero).
"""
import re
import sys
sys.dont_write_bytecode = True
from aidrv_common import (REPO, Tree, body_or_stub, compile_and_run, constant,  # noqa: E402
                          definition, parse_args)

AICAR = "src/GameSource/World/AI/BrnAICar.cpp"
AICAR_UPDATE = "src/GameSource/World/AI/BrnAICar_Update.cpp"

CAR_CONSTANTS = ["KF_RACE_DISTANCE_AHEAD_GAP", "KF_RACE_DISTANCE_BEHIND_GAP",
                 "KF_ALT_ROUTE_HOLD_BASE", "KF_ALT_ROUTE_HOLD_PER_OPP"]
UPDATE_CONSTANTS = ["KF_AICAR_FLOAT_EPSILON", "KF_WRONG_WAY_TIME_LIMIT",
                    "KF_FREE_ROAM_TO_PURSUIT_DISTANCE", "KF_PURSUIT_TO_FREE_ROAM_DISTANCE",
                    "KF_FREE_ROAM_ROUTE_OLD_DISTANCE", "KF_SECTION_SPEED_TO_BUZZ_RATIO",
                    "KF_AICAR_FLT_MAX", "KF_MARKED_MAN_AGGRESSION_LO", "KF_MARKED_MAN_AGGRESSION_HI",
                    "KAF_RANK_AGGRESSION_LO", "KAF_RANK_AGGRESSION_HI"]

CAR_SIGNATURES = [
    "    void AICar::UpdateRelativePositionToPlayer(const AICar* lpPlayerCar)",
    "    void AICar::UpdateRaceDistance(const AISectionsData* lpAISectionsData,",
    "    void AICar::UpdateRouteFindingRace(f32 lfTimeStep, const AICar* lpPlayerCar)",
    "    void AICar::CheckForSectionChange(f32 lfTimeStep, AISectionsData* lpAISectionsData,",
]
UPDATE_SIGNATURES = [
    "    void AICar::Update(const RaceBalancingManager* lpRaceBalancingManager, f32 lfTimeStep,",
    "    bool AICar::HasValidRoute() const",
    "    void AICar::CheckForFreeRoamSwapToPursuit()",
    "    void AICar::UpdatePositionOutOfRange(f32 lfTimeStep, AISectionsData* lpAISectionsData)",
    "    bool AICar::IsFreeRoamingCarsRouteOld()",
    "    bool AICar::ComputeDistanceToCheckpoint(const AISectionsData* lpAISectionsData,",
    "    void AICar::UpdateOutOfRangeData(Vector3 lPosition, Vector3 lAtVector, u16 luSectionIndex,",
    "    void AICar::OnModeStart(EAISpeedSelectionMethod leSpeedSelectionMethod, s32 liOpponentIndex,",
]


def main():
    args = parse_args()
    tree = Tree(args.rev)
    car = tree.read(AICAR)
    update = tree.read(AICAR_UPDATE)
    strings = re.findall(r"^[ \t]*static const char\* const KPC_AICAR_(?:CPP|H)\b[^;]+;", update, re.M)
    if len(strings) != 2:
        raise ValueError("expected the two KPC_AICAR_* file strings")
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;"]
    chunks += [constant(car, name) for name in CAR_CONSTANTS]
    chunks += [constant(update, name) for name in UPDATE_CONSTANTS]
    chunks += strings
    chunks.append(body_or_stub(update, "    static inline bool IsZeroVmx(", ""))
    chunks += [definition(car, signature) for signature in CAR_SIGNATURES]
    chunks += [definition(update, signature) for signature in UPDATE_SIGNATURES]
    chunks.append("}")
    extra = [REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]
    sys.exit(compile_and_run("FxAinan2AICar.cpp", chunks, extra, prefix="brn_fxainan2_aicar_"))


if __name__ == "__main__":
    main()
