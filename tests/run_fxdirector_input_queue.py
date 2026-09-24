"""FX-DIRECTOR (crash parity 2026-09-24): MainDirector::ProcessInputQueue's game-action arms.

  Showtime ran at 0.005x on PC. ArbStateCrashMode::Update @0x82235488 requests the camera's sim
  time scale from GameState::mfImpactTimeSloMoFactor (+0x108) whenever no close-up runs, and the
  ONLY writer of that field is ProcessInputQueue @0x822372F8 case 42 (E_ACTION_IMPACT_TIME_START,
  @0x82237E54). The PC switch had no case 42 -- nor 43 / 140 / 144 / 145 / 146, the rest of the
  Showtime arms (impact-time end, the close-up requests +0x1EA / +0x1EB, the blur requests +0x1E9 /
  +0x1EC, the intro latch +0x1ED) -- so the factor stayed 0 and MainDirector::Update floored the sim
  at 0.005. The GameState tail those arms write (RankUpInfo +0x1D4, ShowTimeInfo +0x1DC,
  DirectorProfileData +0x1F0) is re-modelled to its DWARF layout in the same change.

Numeric: tests/FxDirectorInputQueue.cpp compiles the revision's WHOLE ProcessInputQueue (+
ClearEventPresentationBlock, KF_CRASH_TIME_WINDOW, the TU-local action-42 record) into a stand-in
MainDirector, against the revision's own BrnDirectorGameState.h / .cpp (shadowed), drives it through
the real VariableEventQueue<13312,16> with the console's byte-built wire records, and reads the
GameState through per-layout accessors (so the pre-fix revision compiles and FAILS on behaviour).
Wiring: the arms exist, and ArbStateCrashMode reads the named ShowTimeInfo requests.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_input_queue.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, REPO, STRSTREAM_CPP

DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
ARBITRATOR_CPP = "src/GameSource/Director/Arbitrator/BrnDirectorArbitrator.cpp"
DIRECTOR_H = "src/GameSource/Director/BrnMainDirector.h"
GAMESTATE_H = "src/GameSource/Director/DirectorModule/BrnDirectorGameState.h"
GAMESTATE_CPP = "src/GameSource/Director/DirectorModule/BrnDirectorGameState.cpp"
CRASHMODE_CPP = "src/GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.cpp"
REGION_CPP = REPO / "src/SharedClasses/Trigger/BrnRegion.cpp"
NUMERIC_CHECKS = 77

PROCESS_INPUT_QUEUE = "void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)"
CLEAR_PRESENTATION = "static void ClearEventPresentationBlock(GameState& lrGameState)"
IMPACT_RECORD = "struct ImpactTimeStartActionRecord"
ARB_UPDATE = "void Arbitrator::Update(bool lbPaused, Camera::Camera& lrCameraInOut,"
ARB_RESET_READ = "if (lrSharedInfo.mpGameState->mbShouldResetPlayerCameraThisFrame)"

# GameState field accessors per header layout. NEW: the DWARF sub-objects (FX-DIRECTOR re-model).
# OLD (<= 2eadcaf1): ShowTimeInfo = u8[0x14] @+0x1D4, DirectorProfileData = {u8[8], s32} @+0x1E8.
ACCESSORS_NEW = """
namespace FxDirectorTest
{
    using BrnDirector::GameState;
    inline f32  DeformationLevel(const GameState& g)    { return g.mShowTimeInfo.mfDeformationLevel; }
    inline s32  ComboLevel(const GameState& g)          { return g.mShowTimeInfo.miComboLevel; }
    inline s32  TotalVehiclesHit(const GameState& g)    { return g.mShowTimeInfo.miTotalVehiclesHit; }
    inline bool ComboLevelIncreased(const GameState& g) { return g.mShowTimeInfo.mbComboLevelIncreasedThisFrame; }
    inline bool VehicleImpact(const GameState& g)       { return g.mShowTimeInfo.mbVehicleImpactThisFrame; }
    inline bool CrushCombo(const GameState& g)          { return g.mShowTimeInfo.mbCrushComboThisFrame; }
    inline bool EarntMultiplier(const GameState& g)     { return g.mShowTimeInfo.mbEarntMultiplierThisFrame; }
    inline bool ExtraSpin(const GameState& g)           { return g.mShowTimeInfo.mbExtraSpinThisFrame; }
    inline bool InIntro(const GameState& g)             { return g.mShowTimeInfo.mbInIntro; }
    inline s32  CameraMode(const GameState& g)          { return static_cast<s32>(g.mDirectorProfileData.meCameraMode); }
}
"""
ACCESSORS_OLD = """
namespace FxDirectorTest
{
    using BrnDirector::GameState;
    inline f32 BlobF32(const u8* p) { f32 v; std::memcpy(&v, p, sizeof(v)); return v; }
    inline s32 BlobS32(const u8* p) { s32 v; std::memcpy(&v, p, sizeof(v)); return v; }
    inline f32  DeformationLevel(const GameState& g)    { return BlobF32(&g.mShowTimeInfo.maOpaque[0x08]); }  // +0x1DC
    inline s32  ComboLevel(const GameState& g)          { return BlobS32(&g.mShowTimeInfo.maOpaque[0x0C]); }  // +0x1E0
    inline s32  TotalVehiclesHit(const GameState& g)    { return BlobS32(&g.mShowTimeInfo.maOpaque[0x10]); }  // +0x1E4
    inline bool ComboLevelIncreased(const GameState& g) { return g.mDirectorProfileData.maOpaque[0] != 0; }   // +0x1E8
    inline bool VehicleImpact(const GameState& g)       { return g.mDirectorProfileData.maOpaque[1] != 0; }   // +0x1E9
    inline bool CrushCombo(const GameState& g)          { return g.mDirectorProfileData.maOpaque[2] != 0; }   // +0x1EA
    inline bool EarntMultiplier(const GameState& g)     { return g.mDirectorProfileData.maOpaque[3] != 0; }   // +0x1EB
    inline bool ExtraSpin(const GameState& g)           { return g.mDirectorProfileData.maOpaque[4] != 0; }   // +0x1EC
    inline bool InIntro(const GameState& g)             { return g.mDirectorProfileData.maOpaque[5] != 0; }   // +0x1ED
    inline s32  CameraMode(const GameState& g)          { return g.mDirectorProfileData.miCameraModeWord; }  // +0x1F0
}
"""


def has_case(function_text, case_id):
    return re.search(r"\bcase\s+%d\s*:" % case_id, code_only(function_text)) is not None


def wiring(tree):
    director = tree.read(DIRECTOR_CPP)
    crash_mode = code_only(tree.read(CRASHMODE_CPP))
    try:
        pinq = definition(director, PROCESS_INPUT_QUEUE)
    except ValueError:
        pinq = ""
    flat = re.sub(r"\s+", "", code_only(pinq))
    yield ("ProcessInputQueue has case 42 storing mbImpactTimeActive and mfImpactTimeSloMoFactor "
           "(0x82237E54..0x82237E6C)",
           has_case(pinq, 42) and ".mbImpactTimeActive=true;" in flat
           and ".mfImpactTimeSloMoFactor=" in flat)
    yield ("ProcessInputQueue has the rest of the Showtime arms: 43 / 140 / 144 / 145 / 146 "
           "(0x82237E74 / 0x822386CC / 0x8223864C / 0x822386BC / 0x82238608)",
           all(has_case(pinq, case_id) for case_id in (43, 140, 144, 145, 146)))
    flat_crash_mode = re.sub(r"\s+", "", crash_mode)
    yield ("ArbStateCrashMode reads the close-up / blur requests as the named ShowTimeInfo fields "
           "(+0x1EA/+0x1EB/+0x1E9/+0x1EC/+0x1E0) and requests mfImpactTimeSloMoFactor (0x822356FC)",
           all(name in flat_crash_mode for name in (
               "mShowTimeInfo.mbCrushComboThisFrame", "mShowTimeInfo.mbEarntMultiplierThisFrame",
               "mShowTimeInfo.mbVehicleImpactThisFrame", "mShowTimeInfo.mbExtraSpinThisFrame",
               "mShowTimeInfo.miComboLevel", "SetRequestedTimeDilation(lrGameState.mfImpactTimeSloMoFactor)")))
    # [item 2] the rest of the console's arms, the tail, and the arbitrator's +0x151 read.
    yield ("ProcessInputQueue has the audit's arms 0 / 53 / 54 / 107 / 120 / 132 / 150 / 151 / 215 / 216 / 218 / 224 "
           "(0x82238478 / 0x822384E8 / 0x82238504 / 0x82238520 / 0x82237E44 / 0x82238530 / 0x82237E84 / "
           "0x82237E94 / 0x82238488 / 0x822384C8 / 0x82238550 / 0x822382C0)",
           all(has_case(pinq, case_id) for case_id in (0, 53, 54, 107, 120, 132, 150, 151, 215, 216, 218, 224)))
    crash_window = flat.find("maGameState.mfCrashTimeRemaining=KF_CRASH_TIME_WINDOW;")
    pad_clock = flat.find("maGameState.mfPadInactiveTime=0.0f;")
    player_clock = flat.find("maGameState.mfPlayerInactiveTime=0.0f;")
    been_active = flat.find("maGameState.mbPlayerBeenActive=true;")
    yield ("the tail drops mbCanUseSlomo on the mbForceSloMoNotAllowed latch (0x8223886C) and runs the two "
           "inactivity clocks after the crash window (0x8223893C..0x82238A3C)",
           "if(maStateFlagTail[E_FLAG_TAIL_FORCE_SLOMO_NOT_ALLOWED])" in flat
           and 0 <= crash_window < pad_clock < player_clock < been_active)
    try:
        arb = re.sub(r"\s+", "", code_only(definition(tree.read(ARBITRATOR_CPP), ARB_UPDATE)))
    except ValueError:
        arb = ""
    read = arb.find(re.sub(r"\s+", "", ARB_RESET_READ))
    yield ("Arbitrator::Update reads +0x151 into mbUseGameplayExternal after publishing its containers and before "
           "clearing mbLookbackOverride (0x8226ADE0..0x8226ADFC)",
           0 <= arb.find("lrSharedInfo.mpSharedCameraContainer=&mSharedCameraContainer;") < read
           < arb.find("mSharedCameraContainer.mbLookbackOverride=false;"))


def numeric(tree):
    director = tree.read(DIRECTOR_CPP)
    director_h = tree.read(DIRECTOR_H)
    header = tree.read(GAMESTATE_H)
    gamestate_cpp = tree.read(GAMESTATE_CPP)
    try:
        pinq = definition(director, PROCESS_INPUT_QUEUE)
        clear_block = definition(director, CLEAR_PRESENTATION)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: %s" % error)
        return None
    window = re.search(r"const f32 KF_CRASH_TIME_WINDOW\s*=\s*[^;]+;", director)
    if window is None:
        print("NUMERIC: cannot build -- KF_CRASH_TIME_WINDOW absent")
        return None
    try:
        record = definition(director, IMPACT_RECORD) + ";"
    except ValueError:
        record = "// [this revision has no ImpactTimeStartActionRecord]"
    if "mbCrushComboThisFrame" in header:
        accessors = ACCESSORS_NEW
    elif "miCameraModeWord" in header:
        accessors = ACCESSORS_OLD
    else:
        print("NUMERIC: cannot build -- unknown GameState tail layout in this revision")
        return None
    flag_tail = sorted(set(re.findall(r"\b(E_FLAG_TAIL_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", director_h)))
    enum = ",\n        ".join("%s = %s" % pair for pair in flag_tail) or "E_FLAG_TAIL_NONE = 0"
    inc = ("namespace BrnDirector\n{\n"
           "struct MainDirector\n{\n"
           "    GameState maGameState;\n"
           "    u8        maStateFlagTail[0x20];\n"
           "    AllVehicleDataStandIn mAllVehicleData;\n"
           "    enum EFlagTail\n    {\n        " + enum + "\n    };\n"
           "    void ProcessNewVehicleEvents(const DirectorIO::InputBuffer*) {}\n"
           "    void HandlePrepareForModeAction(const BrnGameState::GameStateModuleIO::PrepareForModeAction&,\n"
           "                                    const DirectorInputOutput*) { ++gPrepareCalls; }\n"
           "    void CalcTrafficLightSpace(const DirectorInputOutput*) {}\n"
           "    void ProcessInputQueue(const DirectorInputOutput* lpIO);\n"
           "};\n"
           "namespace\n{\n    " + window.group(0) + "\n" + record + "\n"
           "    bool BrnDiag_DirectorActionDiagOn() { return false; }\n}\n"
           + clear_block + "\n" + pinq + "\n}\n" + accessors)
    try:
        arb_update = code_only(definition(tree.read(ARBITRATOR_CPP), ARB_UPDATE))
    except ValueError:
        arb_update = ""
    start = arb_update.find(ARB_RESET_READ)
    if start >= 0:
        brace = arb_update.find("{", start)
        arb_stmt = arb_update[start:arb_update.find("}", brace) + 1]
    else:
        print("NUMERIC: Arbitrator::Update has no +0x151 read in this revision (empty stand-in)")
        arb_stmt = "/* [stand-in: no mbShouldResetPlayerCameraThisFrame read in this revision] */"
    inc += ("namespace BrnDirector\n{\n"
            "void ArbitratorPrologueStandIn(SharedCameraContainerStandIn& mSharedCameraContainer,\n"
            "                               ArbSharedInfoStandIn& lrSharedInfo)\n{\n" + arb_stmt + "\n}\n}\n")
    shadow = {GAMESTATE_H: header}
    return compile_and_run(Path(__file__).with_name("FxDirectorInputQueue.cpp"), "director_input_queue.inc", inc,
                           "FxDirectorInputQueue", shadow=shadow,
                           extra_sources=[STRSTREAM_CPP, REGION_CPP, "director_gamestate_rev.cpp"],
                           extra_files={"director_gamestate_rev.cpp": gamestate_cpp})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_input_queue", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
