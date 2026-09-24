"""FX-AINAN2: NaN polarity of the AI steering fan (RacingLine/BrnAISteeringFan_*.cpp).

Extracts the production helpers (the TU-local namespaces of the Aggression, Weightings and HNG
partfiles) and SteeringFan::IncludeSmashIntoTarget, FindNeabyAIInTraffic,
IncludeDriveCloseToPlayer, IncludeDrift{Direction,Location}Tracking, CalculateFanAngle,
IncludeCentreLineTracking, IncludeRouteParallelTracking, FindPlayerInTraffic, IncludeHardNoGo and
IncludeRouteEdgeIntersection, and feeds them NaNs. See FxAinan2SteeringFan.cpp for the ARTIST
addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_steering_fan.py [--rev <rev>]

`--rev <b5 rev>` reads the production files from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

FAN = "src/GameSource/World/AI/RacingLine/"
AGGRESSION = FAN + "BrnAISteeringFan_Aggression.cpp"
WEIGHTINGS = FAN + "BrnAISteeringFan_Weightings.cpp"
HNG = FAN + "BrnAISteeringFan_HNG.cpp"


def main():
    args = parse_args()
    tree = Tree(args.rev)
    agg, wgt, hng = tree.read(AGGRESSION), tree.read(WEIGHTINGS), tree.read(HNG)
    chunks = ["namespace BrnAI {",
              definition(agg, "namespace\n{"),
              definition(wgt, "namespace\n{"),
              definition(wgt, "namespace\n{\n    const f32 KF_STEER_AT_LOW_SPEED"),
              definition(hng, "namespace\n{"),
              definition(agg, "void SteeringFan::IncludeSmashIntoTarget("),
              definition(agg, "const NearbyVehicle* SteeringFan::FindNeabyAIInTraffic("),
              definition(agg, "void SteeringFan::IncludeDriveCloseToPlayer("),
              definition(agg, "void SteeringFan::IncludeDriftDirectionTracking("),
              definition(agg, "void SteeringFan::IncludeDriftLocationTracking("),
              definition(wgt, "void SteeringFan::CalculateFanAngle(AICar* lpCar)"),
              definition(wgt, "void SteeringFan::IncludeCentreLineTracking("),
              definition(wgt, "void SteeringFan::IncludeRouteParallelTracking("),
              definition(wgt, "const NearbyVehicle* SteeringFan::FindPlayerInTraffic("),
              definition(hng, "void SteeringFan::IncludeHardNoGo("),
              definition(hng, "void SteeringFan::IncludeRouteEdgeIntersection("),
              "}"]
    sys.exit(compile_and_run("FxAinan2SteeringFan.cpp", chunks, prefix="brn_fxainan2_fan_"))


if __name__ == "__main__":
    main()
