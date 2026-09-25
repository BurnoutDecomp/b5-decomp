"""FX-AIPAD (crash parity 2026-09-25): the "AI PAD" harness seat, BRN_AI_PAD_PLAYER.

The game's OWN AI computes the player car's controls and they reach physics through the PAD path, with the
control word left at 1 so rivals still target the player (AIAggression::FindTarget @0x82793C60 is untouched).
The seam, every address and the live evidence: scratch/CRASHPARITY_0922/fixes/FX-AIPAD.md.

  1. WIRING -- the production text: the seat's scope wraps the player's own AICar::Update (UpdateCars) and
     AIDriver::Update (UpdateDrivers) and nothing else; the pursuit's victim store and Slam-bias hook are
     gated on the ram window; the world's three call sites sit where the seam needs them (the pad written
     right after BridgeInputToEntityModules, the arm after HarnessArmAIDrivesPlayer, the stash after
     AIModule::Update); the stash reads through the const (READ) accessor; the slam window is the
     console's (flt_820C8074 -4.5, flt_820C4890 20, flt_820047C8 0.05); the reset result loop notes resets.
  2. NUMERIC -- tests/FxAiPadSeat.cpp runs the production bodies (the AI-side scope + reset note and the
     world-side arm / stash / pad write) against stand-ins: OFF means OFF, the scope, the READ seat, the
     record choice, the pad composition, the ram, the manual override.

RED sides:
    --rev 59f18c4f      the tree before the seat: no bodies, every check fails.
    --red-stash         the working tree with the stash's first live spelling put back
                        (`lpAIOutput->GetVehicleDriverInterface()`, the non-const WRITE seat 0x8276D9C8 under a
                        read lock: fxaipad_cruise/20260925_100036 logged 6655 x "Not locked for writing"); the
                        READ-seat checks must fail.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaipad_seat.py [--rev <b5 rev>] [--red-stash]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, compile_and_run, definition, report

DRIVE_CPP = "src/GameSource/World/AI/BrnAIModule_Drive.cpp"
DRIVER_CPP = "src/GameSource/World/AI/BrnAIDriver.cpp"
RESETPUMP_CPP = "src/GameSource/World/AI/BrnAIModule_ResetPump.cpp"
WORLD_CPP = "src/GameSource/World/BrnWorldModule.cpp"
PAD_H = "src/GameSource/World/AI/BrnAIHarnessPad.h"
NUMERIC_CHECKS = 31

AI_SIGNATURES = (
    "HarnessAIPad gHarnessAIPad =",
    "bool HarnessAIPadBeginPlayerSeat(AICar* lpCar)",
    "void HarnessAIPadEndPlayerSeat(AICar* lpCar, bool lbTaken)",
    "void HarnessAIPadNoteReset(s32 liGlobalRaceCarIndex)",
)
WORLD_BLOCK = "namespace\n{\n    // One frame's player-seat decision"
WORLD_SIGNATURES = (
    "void WorldModule::HarnessArmAIPadPlayer()",
    "void WorldModule::HarnessStashAIPadControls( BrnAI::AIModuleIO::OutputBuffer* lpAIOutput )",
    "void WorldModule::HarnessApplyAIPad( RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarInput_PreScene,",
)
READ_SEAT = "lpAIOutputRead->GetVehicleDriverInterface()"
WRITE_SEAT_SPELLING = "lpAIOutput->GetVehicleDriverInterface()"


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def world_update(source):
    """WorldModule::Update's body (the member function, not a namespace-scope helper)."""
    return body_or_empty(source, "void\nWorldModule::Update( BrnUpdateSet lUpdateSet,")


def ordered(text, *needles):
    """True when every needle occurs in text, each after the previous one."""
    position = 0
    for needle in needles:
        found = text.find(needle, position)
        if found < 0:
            return False
        position = found + len(needle)
    return True


def wiring(tree):
    drive = normalised(tree, DRIVE_CPP)
    update_cars = body_or_empty(drive, "void AIModule::UpdateCars(")
    yield ("UpdateCars: the player's own AICar::Update runs inside Begin/End (and only that call)",
           ordered(update_cars, "HarnessAIPadBeginPlayerSeat(lpAICar)", "lpAICar->Update(",
                   "HarnessAIPadEndPlayerSeat(lpAICar, lbHarnessSeat)")
           and update_cars.count("HarnessAIPadBeginPlayerSeat") == 1)
    update_drivers = body_or_empty(drive, "void AIModule::UpdateDrivers(")
    yield ("UpdateDrivers: the player's own AIDriver::Update runs inside Begin/End",
           ordered(update_drivers, "HarnessAIPadBeginPlayerSeat(lpHarnessSeatCar)", "lpDriver->Update(",
                   "HarnessAIPadEndPlayerSeat(lpHarnessSeatCar, lbHarnessSeat)")
           and update_drivers.count("HarnessAIPadBeginPlayerSeat") == 1)
    yield ("UpdateDrivers: the pursuit runs before the round robins and the drivers",
           ordered(update_drivers, "HarnessAIPadPursuit(lpPlayerCar)", "DoRoundRobins()", "lpDriver->Update("))
    yield ("UpdateDrivers: the target replaces the player driver's victim only while ramming, after the console's stores",
           re.search(r"lpDriver->SetAggressionVictim\(lpPlayerCar->GetRaceCarIndex\(\)\);\s*\}\s*"
                     r"if \(gHarnessAIPad\.mbRamming && leSlot == mePlayerActiveRaceCarIndex\)", update_drivers)
           is not None)
    pursuit = body_or_empty(drive, "void AIModule::HarnessAIPadPursuit(AICar* lpPlayerCar)")
    yield ("pursuit: returns (and lets go of the route) unless armed with the pursuit objective",
           re.search(r"lrPad\.mbArmed\s*&&\s*lrPad\.meMode == E_HARNESS_AI_PAD_PURSUIT", pursuit) is not None
           and "HarnessAIPadReleaseRoute(lpPlayerCar" in body_or_empty(pursuit, "if (!lbPursuing)")
           and "return;" in body_or_empty(pursuit, "if (!lbPursuing)"))
    yield ("pursuit: the attached roster is UpdateCars' own (8 in a game mode, 35 otherwise; 0x8279A580)",
           "mbIsInGameMode ? KI_MAX_ACTIVE_RACE_CARS : KI_MAX_OUT_OF_RANGE_RACE_CARS" in pursuit
           and "lpCar->IsActive()" in pursuit)
    yield ("pursuit: the slam window is the console's (lfSep * 0.05 < 1, aheadness in [-4.5, 20])",
           re.search(r"lfSep \* KF_HARNESS_SLAM_SEPARATION_SCALE < 1\.0f\)\s*&& !\(lrCandidate\.mfAheadness < "
                     r"KF_HARNESS_SLAM_MIN_AHEADNESS\)\s*&& !\(lrCandidate\.mfAheadness > KF_HARNESS_SLAM_MAX_AHEADNESS\)",
                     pursuit) is not None)
    constants = code_only(drive)
    yield ("the slam constants are flt_820C8074 = -4.5, flt_820C4890 = 20.0, flt_820047C8 = 0.05",
           re.search(r"KF_HARNESS_SLAM_MIN_AHEADNESS\s*=\s*-4\.5f;", constants) is not None
           and re.search(r"KF_HARNESS_SLAM_MAX_AHEADNESS\s*=\s*20\.0f;", constants) is not None
           and re.search(r"KF_HARNESS_SLAM_SEPARATION_SCALE\s*=\s*0\.05f;", constants) is not None)
    yield ("pursuit: a moved target is re-aimed only on the console's own player route-age test (0x8276FD50)",
           "lpPlayerCar->IsExtrapolatedRouteGettingOld()" in pursuit)
    driver = normalised(tree, DRIVER_CPP)
    fan = body_or_empty(driver, "void AIDriver::SetDrivingFanBiases(")
    yield ("SetDrivingFanBiases: the Slam bias hook fires only for the player while armed AND ramming",
           re.search(r"if \(lbPlayer && gHarnessAIPad\.mbArmed && gHarnessAIPad\.mbRamming\)\s*\{\s*"
                     r"mSteeringFan\.SetBiasMode\(eBiasMode_Slam\);\s*return;\s*\}", fan) is not None)
    resetpump = normalised(tree, RESETPUMP_CPP)
    manager = body_or_empty(resetpump, "void AIModule::UpdateResetOnTrackManager(")
    yield ("UpdateResetOnTrackManager: a SUCCESS reset and a placement note the car for the pursuit",
           manager.count("HarnessAIPadNoteReset(") == 2
           and ordered(manager, "E_STATE_SUCCESS", "HarnessAIPadNoteReset(", "GetPlaceOnTrackRequestQueue",
                       "HarnessAIPadNoteReset("))
    world = normalised(tree, WORLD_CPP)
    update = world_update(world)
    yield ("WorldModule::Update: the AI pad is written right after BridgeInputToEntityModules, before the unlocks",
           ordered(update, "BridgeInputToEntityModules(", "HarnessApplyAIPad( lpRaceCarInput_PreScene, lpUpdateInputBuffer )",
                   "lpPropInput_PreScene->UnlockForWrite()"))
    yield ("WorldModule::Update: armed after the console seat's arm, before AIModule::Update",
           ordered(update, "HarnessArmAIDrivesPlayer()", "HarnessArmAIPadPlayer()", "mAIModule.Update("))
    yield ("WorldModule::Update: the stash reads the AI output after AIModule::Update, before it is destroyed",
           ordered(update, "mAIModule.Update(", "HarnessStashAIPadControls( lpAIOutput )",
                   "DestroyIOBuffer( &lpAIInput )"))
    stash = body_or_empty(world, WORLD_SIGNATURES[1])
    yield ("stash: the record queue is read through the const (READ) accessor 0x8279CA00",
           READ_SEAT in stash and WRITE_SEAT_SPELLING not in stash)
    arm = body_or_empty(world, WORLD_SIGNATURES[0])
    yield ("arm: OFF (BRN_AI_PAD_PLAYER unset) returns before anything is written",
           re.search(r"if \( seMode == BrnAI::E_HARNESS_AI_PAD_OFF \)\s*\{\s*return;\s*\}", arm) is not None
           and arm.find("return;") < arm.find("lrPad.meMode = seMode;"))


def numeric(tree, red_stash):
    drive = normalised(tree, DRIVE_CPP)
    world = normalised(tree, WORLD_CPP)
    missing = []
    ai_parts = []
    for signature in AI_SIGNATURES:
        try:
            text = definition(drive, signature)
            ai_parts.append(text + (";" if signature.endswith("=") else ""))
        except ValueError:
            missing.append(signature)
    world_parts = []
    try:
        world_parts.append(definition(world, WORLD_BLOCK))
    except ValueError:
        missing.append("the seat's anonymous-namespace block")
    for signature in WORLD_SIGNATURES:
        try:
            world_parts.append(definition(world, signature))
        except ValueError:
            missing.append(signature)
    if not tree.read(PAD_H):
        missing.append(PAD_H)
    if missing:
        print("NUMERIC: cannot build -- missing: " + "; ".join(missing))
        return None
    world_text = "\n\n".join(world_parts)
    if red_stash:
        if READ_SEAT not in world_text:
            print("NUMERIC: --red-stash needs the READ-seat spelling in the stash")
            return None
        world_text = world_text.replace(READ_SEAT, WRITE_SEAT_SPELLING)
        print("RED: the stash's first live spelling put back: " + WRITE_SEAT_SPELLING)
    shadow = {PAD_H: tree.read(PAD_H)}
    return compile_and_run(Path(__file__).with_name("FxAiPadSeat.cpp"), "fxaipad_ai.inc", "\n\n".join(ai_parts),
                           "FxAiPadSeat", shadow=shadow, extra_files={"fxaipad_world.inc": world_text})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--red-stash", action="store_true",
                        help="put the stash's first live spelling (the WRITE seat) back; the READ-seat checks must fail")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxaipad_seat", list(wiring(tree)), numeric(tree, args.red_stash), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
