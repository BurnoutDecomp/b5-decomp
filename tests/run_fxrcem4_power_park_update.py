"""FX-RCEM4 (crash parity 2026-09-24): the Power Parking scorer's per-frame bodies --
PowerParkingManager::Update (ARTIST 0x822F8400), UpdateScoring (0x822A7140) and the inlined
ClearData / AddNearTraffic / AddContactTraffic / SetNearbyParkedTrafficData -- with the file-scope
KF_* tuning globals (.data 0x82CDB4C4..0x82CDB504, .bss 0x82FAD2E4..0x82FAD2F0 / 0x82FAD3FC).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_power_park_update.py [--pre-fix <b5 rev>]
Extracts the bodies into tests/FxRcem4PowerParkUpdate.cpp. A body the source lacks (or only
declares) is replayed as an empty stub; a missing tuning global as NaN.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

HEADER = RCEM + "PowerParking/BrnPowerParkingManager.h"
SOURCE = RCEM + "PowerParking/BrnPowerParkingManager.cpp"

GLOBALS = (
    "KF_MIN_LINEAR_VELOCITY_TO_START_POWER_PARK", "KF_MIN_HANDBRAKE_TO_START_POWER_PARK",
    "KF_MAX_LINEAR_VELOCITY_TO_END_POWER_PARK", "KF_MAX_ANGULAR_VELOCITY_TO_END_POWER_PARK",
    "KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT", "KF_MAX_PERPENDICULAR_DISTANCE_FOR_PERFECT",
    "KF_PROMIXITY_IDEAL_1ST_CAR_DISTANCE", "KF_PROMIXITY_IDEAL_2ND_CAR_DISTANCE",
    "KF_DISTANCE_SCORE_SCALE", "KF_PROXIMITY_SCORE_SCALE", "KF_SPEED_SCORE_SCALE",
    "KF_ROTATION_SCORE_SCALE", "KF_POSITION_ALIGNMENT_SCORE_SCALE", "KF_ANGLE_ALIGNMENT_SCORE_SCALE",
    "KF_DISTANCE_SCORE_WEIGHT", "KF_PROXIMITY_SCORE_WEIGHT", "KF_SPEED_SCORE_WEIGHT",
    "KF_ROTATION_SCORE_WEIGHT", "KF_ANGLE_ALIGNMENT_SCORE_WEIGHT", "KF_POSITION_ALIGNMENT_SCORE_WEIGHT",
    "KF_TOTAL_SCORE_WEIGHTS", "KF_WAIT_FOR_OUTCOME_TIME",
)

BODIES = (
    ("void PowerParkingManager::ClearData(", "void PowerParkingManager::ClearData() {}"),
    ("void PowerParkingManager::DetermineOutcome(", "void PowerParkingManager::DetermineOutcome() {}"),
    ("void PowerParkingManager::AddNearTraffic(", "void PowerParkingManager::AddNearTraffic(u32) {}"),
    ("void PowerParkingManager::AddContactTraffic(", "void PowerParkingManager::AddContactTraffic(u32) {}"),
    ("void PowerParkingManager::SetNearbyParkedTrafficData(",
     "void PowerParkingManager::SetNearbyParkedTrafficData(u32, u32, f32, f32, f32, f32) {}"),
    ("void PowerParkingManager::UpdateScoring(", "void PowerParkingManager::UpdateScoring(f32, f32, f32) {}"),
    ("void PowerParkingManager::Update(",
     "void PowerParkingManager::Update(BrnGameState::GameStateModuleIO::EGameModeType, f32, ActiveRaceCar*, "
     "PlayerVehicleControls*, RaceCarEntityModuleIO::GameEventQueue*) {}"),
)


def bodied(source, signature):
    """The brace-balanced definition starting at `signature` when it is DEFINED there (the next
    code token after the parameter list is `{`), else ''."""
    start = source.find(signature)
    while start >= 0:
        close = source.find(")", start)
        tail = re.sub(r"//[^\n]*", "", source[close + 1:close + 400]) if close >= 0 else ""
        if tail.lstrip().startswith("{"):
            return definition(source[start:], signature)
        start = source.find(signature, start + 1)
    return ""


def main():
    rev = pre_fix_rev(sys.argv)
    header, source = read(HEADER, rev), read(SOURCE, rev)

    detail = definition(header, "namespace PowerParkingDetail") if "namespace PowerParkingDetail" in header else ""
    print(("found   " if detail else "MISSING ") + "PowerParkingDetail")
    if not detail:
        detail = "namespace PowerParkingDetail { const f32 KF_HALF_PI = 1.5707964f; }"

    globals_text = []
    for name in GLOBALS:
        found = re.search(r"^\s*f32 " + name + r"\s*=[^;]+;", source, re.M)
        print(("found   " if found else "MISSING ") + name)
        globals_text.append(found.group(0).strip() if found
                            else f"f32 {name} = std::numeric_limits<f32>::quiet_NaN();")

    bodies = []
    for signature, stub in BODIES:
        text = bodied(source, signature)
        print(("found   " if text else "MISSING ") + signature.split("::")[1].rstrip("("))
        bodies.append(text or stub)

    pieces = {
        "fxrcem4_pu_detail.inc": detail,
        "fxrcem4_pu_kf.inc": "\n".join(globals_text),
        "fxrcem4_pu_bodies.inc": "\n\n".join(bodies),
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4PowerParkUpdate.cpp", pieces, "fxrcem4_pu",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
