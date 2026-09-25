"""FX-GATE, the AI fused group (crash parity 2026-09-25): every AI lerp the console computes with ONE rounding --
fmadds / fnmsubs / vmaddfp, ROUNDING_RULE 3 (scratch/CRASHPARITY_0922/ROUNDING_RULE.md) -- is std::fmaf with the
console's operands, and CheckVehicleForPowerPark's ground-plane square is fmaf(x, x, z*z).

  SteeringFan::CalculateFanAngle                 0x82768D20 fmadds (high - low) * ratio + low
                                                 0x82768D2C fmadds ratio * 15.0 + 10.0
  RaceBalancingGraph::ComputeSpeedRatio          0x8277B894 fnmsubs -(prev * (1/7) - fraction), then fmuls 7.0
                                                 0x8277B8B8 fmadds (next - prev) * segment + prev
  RaceBalancingManager::ComputeTargetSpeed       0x827917AC fmadds (raceTime - parTime) * 0.1 + 1.0
  RaceBalancingRoute::ComputeRaceCompletionRatio 0x8277AE08 fmadds (end - start) * ratio + start
  ResetOnTrackManager::GetRoadSideForStartingLine 0x82784488 fmadds draw * 2.0 + 8.0 / width (exact product:
                                                 the same value both ways, pinned by wiring only)
  AIDriver::ProximitySpeed                       0x827708C8 vmaddfp (minSpeed - v) * t + v
  AIDriver::CorneringTopSpeed                    0x8277D2A0 vmaddfp (scaled - input) * ramp + input
  CheckVehicleForPowerPark                       0x822B1FFC vmaddfp x * x + (z * z)

  1. WIRING -- each statement spells the console's instruction with its operands in the console's order.
  2. NUMERIC -- each statement is EXTRACTED into a kernel and run on tests/FxGateAiFusedData.h (exact arithmetic,
     checked against the real words on emu64; scratch/CRASHPARITY_0922/fixes/FX-GATE.aifma), rows chosen so that
     one rounding and two differ; plus the source constants against the image, and RoadSide over every ring draw.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_ai_fused.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

FAN = "src/GameSource/World/AI/RacingLine/BrnAISteeringFan_Weightings.cpp"
GRAPH = "src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.cpp"
MANAGER = "src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.cpp"
ROUTE = "src/GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.cpp"
RESET = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_Strategies.cpp"
DRIVER = "src/GameSource/World/AI/BrnAIDriver.cpp"
PARK = "src/GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"
NUMERIC_CHECKS = 598

# name: (file, function signature, statement start (regex, or "return" = the body's last return), kernel head,
#        kernel tail, the console spelling expected in the statement (whitespace removed), the instruction)
SITES = {
    "FanAngle": (FAN, "void SteeringFan::CalculateFanAngle(AICar* lpCar)", r"\bmfFanAngle\s*=",
                 "static float K_FanAngle(float lfRatio)\n{\n    float mfFanAngle = 0.0f;\n", "    return mfFanAngle;\n}\n",
                 "mfFanAngle=std::fmaf(KF_STEER_AT_HIGH_SPEED-KF_STEER_AT_LOW_SPEED,lfRatio,KF_STEER_AT_LOW_SPEED);",
                 "fmadds 0x82768D20"),
    "LookAhead": (FAN, "void SteeringFan::CalculateFanAngle(AICar* lpCar)", r"\bmfLookAheadRadius\s*=",
                  "static float K_LookAhead(float lfRatio)\n{\n    float mfLookAheadRadius = 0.0f;\n",
                  "    return mfLookAheadRadius;\n}\n",
                  "mfLookAheadRadius=std::fmaf(lfRatio,KF_FAN_LOOK_AHEAD_GROWTH,KF_FAN_LOOK_AHEAD_BASE);",
                  "fmadds 0x82768D2C"),
    "Segment": (GRAPH, "f32 RaceBalancingGraph::ComputeSpeedRatio(GraphType leGraphType, f32 lfFraction) const",
                r"\bf32\s+lfSegmentFraction\s*=",
                "static float K_Segment(float lfFraction, s32 liPrevPoint)\n{\n", "    return lfSegmentFraction;\n}\n",
                "f32lfSegmentFraction=-std::fmaf(static_cast<f32>(liPrevPoint),1.0f/7.0f,-lfFraction)*7.0f;",
                "fnmsubs 0x8277B894"),
    "SpeedLerp": (GRAPH, "f32 RaceBalancingGraph::ComputeSpeedRatio(GraphType leGraphType, f32 lfFraction) const",
                  "return", "static float K_SpeedLerp(float lfPrev, float lfNext, float lfSegmentFraction)\n{\n", "}\n",
                  "returnstd::fmaf(lfNext-lfPrev,lfSegmentFraction,lfPrev);", "fmadds 0x8277B8B8"),
    "Multiplier": (MANAGER, "f32 RaceBalancingManager::ComputeTargetSpeed(GraphType leGraphType, const AICar* lpAICar,",
                   r"\bf32\s+lfMultiplier\s*=",
                   "static float K_Multiplier(float mfRaceTime, float lfTargetTime)\n{\n", "    return lfMultiplier;\n}\n",
                   "f32lfMultiplier=std::fmaf(mfRaceTime-lfTargetTime,KF_SPEED_DIFFERENCE_MULTIPLIER,1.0f);",
                   "fmadds 0x827917AC"),
    "Completion": (ROUTE, "f32 RaceBalancingRoute::ComputeRaceCompletionRatio(f32 lfDistanceToNextCheckpoint,",
                   r"\bf32\s+lfResult\s*=",
                   "static float K_Completion(float lfCheckpointStartRatio, float lfCheckpointEndRatio,"
                   " float lfCheckpointRatio)\n{\n", "    return lfResult;\n}\n",
                   "f32lfResult=std::fmaf(lfCheckpointEndRatio-lfCheckpointStartRatio,lfCheckpointRatio,"
                   "lfCheckpointStartRatio);", "fmadds 0x8277AE08"),
    "RoadSide": (RESET, "f32 ResetOnTrackManager::GetRoadSideForStartingLine(const RouteNode* lpNextNode, s32 liRaceCarIndex)",
                 r"\bf32\s+lfInterp\s*=",
                 "static float K_RoadSide(float lfDraw, float lfRoadWidth)\n{\n"
                 "    struct FakeRandom { float mfDraw; float RandomFloat() { return mfDraw; } } mRandom = { lfDraw };\n",
                 "    return lfInterp;\n}\n",
                 "f32lfInterp=std::fmaf(mRandom.RandomFloat(),KF_STARTING_LINE_SPREAD,KF_STARTING_LINE_CAR_WIDTH/"
                 "lfRoadWidth)-1.0f;", "fmadds 0x82784488"),
    "Proximity": (DRIVER, "f32 AIDriver::ProximitySpeed(f32 lfMinSpeed)", "return",
                  "static float K_Proximity(float lfV, float lfMinSpeed, float lfT)\n{\n", "}\n",
                  "returnstd::fmaf(lfMinSpeed-lfV,lfT,lfV);", "vmaddfp 0x827708C8"),
    "Cornering": (DRIVER, "f32 AIDriver::CorneringTopSpeed(f32 lfInputSpeed)", "return",
                  "static float K_Cornering(float lfInputSpeed, float lfScaled, float lfRamp)\n{\n", "}\n",
                  "returnstd::fmaf(lfScaled-lfInputSpeed,lfRamp,lfInputSpeed);", "vmaddfp 0x8277D2A0"),
    "DistanceSq": (PARK, "inline bool CheckVehicleForPowerPark(", r"\bconst\s+f32\s+lfDistanceSq\s*=",
                   "static float K_DistanceSq(float lfX, float lfY, float lfZ)\n{\n"
                   "    struct FakeVector { float x, y, z; } lPlayerToVehicle = { lfX, lfY, lfZ };\n",
                   "    return lfDistanceSq;\n}\n",
                   "constf32lfDistanceSq=std::fmaf(lPlayerToVehicle.x,lPlayerToVehicle.x,"
                   "lPlayerToVehicle.z*lPlayerToVehicle.z);", "vmulfp128 0x822B1FF8 + vmaddfp 0x822B1FFC"),
}

# The constants the kernels read, from the file that defines them.
CONSTANTS = [
    (FAN, "KF_STEER_AT_LOW_SPEED"), (FAN, "KF_STEER_AT_HIGH_SPEED"), (FAN, "KF_FAN_LOOK_AHEAD_BASE"),
    (FAN, "KF_FAN_LOOK_AHEAD_GROWTH"), (MANAGER, "KF_SPEED_DIFFERENCE_MULTIPLIER"),
    (RESET, "KF_STARTING_LINE_SPREAD"), (RESET, "KF_STARTING_LINE_CAR_WIDTH"),
]


def statement(tree, site):
    path, signature, start = site[0], site[1], site[2]
    try:
        body = code_only(definition(tree.read(path).replace("\r\n", "\n"), signature))
    except ValueError:
        return None
    if start == "return":
        begin = body.rfind("return ")
        if begin < 0:
            return None
    else:
        match = re.search(start, body)
        if match is None:
            return None
        begin = match.start()
    end = body.find(";", begin)
    return None if end < 0 else body[begin:end + 1]


def constant(tree, path, name):
    match = re.search(r"\bconst\s+f32\s+" + name + r"\s*=\s*([^;]+);", code_only(tree.read(path)))
    return None if match is None else match.group(1).strip()


def wiring(tree):
    for name, site in SITES.items():
        text = statement(tree, site)
        flat = None if text is None else re.sub(r"\s+", "", text)
        yield (f"{name}: {site[6]} spelled {site[5]}", flat is not None and site[5] in flat)


def numeric(tree):
    parts = ["namespace\n{\n"]
    for path, name in CONSTANTS:
        value = constant(tree, path, name)
        if value is None:
            print(f"NUMERIC: cannot build -- {name} was not found in {path}")
            return None
        parts.append(f"    const f32 {name} = {value};\n")
    parts.append("}\n\n")
    for name, site in SITES.items():
        text = statement(tree, site)
        if text is None:
            print(f"NUMERIC: cannot build -- the {name} statement was not found")
            return None
        parts.append(site[3] + "    " + text + "\n" + site[4] + "\n")
    return compile_and_run(Path(__file__).with_name("FxGateAiFused.cpp"), "fxgate_ai_fused.inc", "".join(parts),
                           "FxGateAiFused", extra_flags=f'/I"{Path(__file__).parent}"')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_ai_fused", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
