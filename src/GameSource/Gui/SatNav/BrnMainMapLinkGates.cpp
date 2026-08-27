// =================================================================================================
// GameSource/Gui/SatNav/BrnMainMapLinkGates.cpp
//
// ⛔ LINK SCAFFOLD -- NOT A RECONSTRUCTION. Every body in this file is a stand-in.
//
// WHY IT EXISTS (measured 2026-08-27, stunt-race UI wave, scout S1). The seven finished
// PreRaceFlyBy partfiles (GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_0*.cpp,
// ~2,400 lines, all seven compile GREEN under the canonical exe flags) are a LINK problem,
// not a compile problem: mounting them leaves exactly 15 real unresolved externals
// (31 raw UNDEF minus the 16 CRT/lib symbols the exe already resolves from libs).
// Re-measurable: scratch/stuntrace_ui/s1_flyby_mount/{wj_syms.txt,build_syms.txt,holes.txt}.
//
// FOUR of those 15 get REAL bodies in this same wave and are NOT in this file:
//   MainMapComponent::Construct @0x8245E228, MainMapComponent::Prepare @0x8244F4A8,
//   MainMapComponent::SetDesiredWorldCentre + SetStickMapToScreenEdges (header-inline on
//   console per DWARF BrnMainMap.h:379/:415), and GuiCache::UnloadResource @0x824FEB60.
// The TEN below are the remainder. ⚠️ DO NOT add a body here for any of those four --
// two definitions is LNK2005, exactly the failure mode this scaffold exists to avoid.
//
// ⚠️ WHY GATES AND NOT GUESSES -- ALL TEN ARE PROVABLY DEAD ON THE STUNT-RUN PATH.
// Read out of the wJ bodies themselves (S1 §4), the fly-by reaches them only through
// guards that are FALSE for STUNT_ATTACK:
//   * behind `IsMapApplicableToGameMode(...)`, which returns FALSE for STUNT_ATTACK --
//     GuiCache::RefreshMapState (wJ_02:317), MainMapComponent::SetZoom (wJ_04:295),
//     MainMapComponent::Update (wJ_04:343), MapTransform::CalculateZoomFactor and
//     GuiCache::GetLandmarkInfoFromID (wJ_05:293/:269, reached only from the gated :295).
//   * behind `mpIconManager == 0`, which is only assigned inside that same map gate
//     (wJ_02:208, under the `if (IsMapApplicableToGameMode(...))` at :206) --
//     GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID (wJ_01:413, inside
//     UpdateIconManager, which returns at wJ_01:391).
//   * behind a game-mode switch arm the stunt run never takes --
//     GuiCache::GetLandmarkInfoFromIndex + GuiCache::GetEventDestinationLandmarkIndex
//     (wJ_04:88, `case E_MODE_OFFLINE_RACE / E_MODE_MARKED_MAN` only) and
//     DerivedCarArray::ConstructPatternLiveryList (wJ_06:177, SetBurningRouteDescription,
//     BURNING_ROUTE only).
//   * the map-event arm of the fly-by's event handler -- MainMapComponent::RecvEvent
//     (wJ_03:164).
// So there is no live stunt-path behaviour to preserve here, only a link to close. Each
// gate logs ONCE so an absence downstream is never scored as a silent success
// ([[silent-drop-stubs]]).
//
// ⭐ CONSOLE SEMANTICS THAT WILL LOOK LIKE A BUG AFTER THE MOUNT (pre-empting the report):
// IsMapApplicableToGameMode is FALSE for STUNT_ATTACK on the console too, so the stunt-run
// fly-by legitimately shows NO MINIMAP -- event titles and description only. That is
// correct behaviour, not a consequence of this scaffold.
//
// ⭐ RETURN-VALUE RATCHET. An inert gate whose result a caller TESTS must return the value
// that keeps the caller on its no-op path, and must never leave a caller's out-record
// uninitialised (the [[placeholder-zero]] / uninit-stack-read family of bugs). Every gate
// below names its call site and justifies its chosen value in its own banner. Note the
// deliberate NON-zero choice at MapTransform::CalculateZoomFactor.
//
// ⚠️ ONE S1 STATEMENT IS REFUTED BY S1'S OWN ARTIFACT, and it matters to the mount:
// S1 §0 says "MapManager::MapManager() is NO LONGER a flyby blocker ... no wJ obj
// references it (measured: 0 referencing objs)". It is still referenced --
// scratch/stuntrace_ui/s1_flyby_mount/wj_syms.txt:197 shows
// `??0MapManager@BrnGui@@QEAA@XZ ... UNDEF` inside BrnPreRaceFlyBy_wJ_01.obj, pulled in by
// the COMDAT MainMapComponent implicit ctor (`??0MainMapComponent@BrnGui@@QEAA@XZ`, SECT13
// of the same obj) which constructs the embedded MapManager. It is absent from holes.txt
// only because BrnHudStatesLinkStubs.cpp:173 DEFINES it. The operational conclusion is
// unchanged but now mandatory rather than optional: ⭐ THE `MapManager::MapManager() {}`
// STUB AT BrnHudStatesLinkStubs.cpp:173 MUST STAY -- deleting it turns 15 holes into 16.
//
// DELETE-WHEN: each gate carries its own DELETE-WHEN line. The whole FILE dies (and its
// bat line at the SatNav block goes with it) when the last of the ten lands a real body.
// The follow-on faithful wave should start with the BrnMapManager ctor split (S1 §2):
// MapManager::MapManager() is the sole source of both fake externals (BrnMapManager.cpp:20
// / :30) AND the sole LNK2005 in that TU, so splitting it out makes the rest of
// BrnMapManager.cpp a ZERO-new-external mount -- which is the cheap route to
// MapManager::RecvEvent, which is the cheap route to MainMapComponent::RecvEvent.
// =================================================================================================


#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector2 / Vector4 / CgsID

#include "GameSource/BurnoutConstants.h"                     // E_ACTIVE_RACE_CAR_INDEX_INVALID
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (by value)
#include "GameSource/GameState/Progression/BrnDerivedCars.h" // BrnProgression::DerivedCarArray
#include "GameSource/Gui/BrnGuiCache.h"                      // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/Gui/SatNav/BrnMainMap.h"                // BrnGui::MainMapComponent
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // BrnGui::MapTransform
#include "SharedClasses/World/BrnWorldRegion.h"              // BrnWorld::ECounty / EDistrict
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace
{
    // Same one-shot logger as the BrnFriendsListLinkGates.cpp precedent, retagged.
    void LogGateOnce(bool& lrbLogged, const char* lpacSymbol)
    {
        if (lrbLogged)
        {
            return;
        }
        lrbLogged = true;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[mainmap-link-gate] " << lpacSymbol
                << ": inert stand-in, no body anywhere in the tree [FLAG link scaffold]\n";
        }
    }

    // Fill a caller's stack SatNavIconInfo with a deterministic empty record. The two
    // GuiCache landmark-info gates below share it so that NO caller ever reads an
    // uninitialised stack struct -- that read, not the missing body, is what would turn a
    // dead path into a nondeterministic one. Values: zero position lane, kCGSID_NULL id,
    // zero rotation/speed, icon type LANDMARK (== 4, the type the real
    // GetLandmarkInfoFromID @0x825067E0 writes, per BrnGuiCache.h:471-481), and the two
    // attested "none" sentinels for the trailing bytes (E_ACTIVE_RACE_CAR_INDEX_INVALID
    // == -1, BurnoutConstants.h:10; E_PLAYER_TEAM_NONE == 0, BrnGameStateSharedIO.h:228).
    // County/district take enumerator 0 (E_COUNTY_PALM_BAY_HEIGHTS / E_DISTRICT_OCEAN_VIEW,
    // BrnWorldRegion.h:15/:29) because their setters assert `>= 0`.
    // [FLAG link scaffold] These are SCAFFOLD values, not console values -- the console
    // fills the record from the WorldDataController.
    // DELETE-WHEN both landmark-info gates below land real bodies.
    void FillInertIconInfo(BrnGui::GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo)
    {
        if (lpOutIconInfo == 0)
        {
            return;
        }

        const Vector4 lv4Zero = { 0.0f, 0.0f, 0.0f, 0.0f };

        lpOutIconInfo->SetPositionLane(lv4Zero);
        lpOutIconInfo->SetCgsId(0);
        lpOutIconInfo->SetRotation(0.0f);
        lpOutIconInfo->SetSpeedMph(0.0f);
        lpOutIconInfo->SetHiddenDriveThru(false);
        lpOutIconInfo->SetIconType(
            BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK);
        lpOutIconInfo->SetCounty(BrnWorld::E_COUNTY_PALM_BAY_HEIGHTS);
        lpOutIconInfo->SetDistrict(BrnWorld::E_DISTRICT_OCEAN_VIEW);
        lpOutIconInfo->SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
        lpOutIconInfo->SetPlayerTeamByte(0);   // E_PLAYER_TEAM_NONE
    }
}

namespace BrnGui
{
    // =============================================================================================
    //  MainMapComponent -- TU of record: GameSource/Gui/SatNav/BrnMainMap.cpp
    //  (MOUNTED 2026-08-27; it defines Construct @0x8245E228, Prepare @0x8244F4A8,
    //  RecvEvent @0x82458370, SetStandardDefZoomParams @0x82447ED8 and the two class-static
    //  zoom tables. The gates below cover only its still-unreconstructed siblings.)
    // =============================================================================================

    // X360 BrnGui::MainMapComponent::Update @0x824696E8 (json name field verified).
    // Declared BrnMainMap.h:105. 233 lines of pseudocode, and it pulls two more
    // unreconstructed helpers -- CalculatePositionedWorldRect @0x8245E5F0 (413 lines) and
    // CalculateViewPaddingOffset @0x82447D38 (110 lines) -- plus the still-unrecovered
    // vector helper sub_8245A080 flagged at BrnMainMap.cpp:6-18.
    //
    // ⭐ RETURN VALUE = THE INPUT, UNCHANGED. The one call site is
    // BrnPreRaceFlyBy_wJ_04.cpp:343, `mv2WorldCenterPoint = mMainMapComponent.Update(
    // mv2WorldCenterPoint);` -- a feedback assignment inside the IsMapApplicableToGameMode
    // arm. Returning the argument is the fixed point of that assignment: the fly-by's world
    // centre neither drifts nor goes non-finite while the animation is gated out. Returning
    // a default-constructed Vector2 would silently teleport the centre to the origin, and
    // falling off the end is a /w14715 ratchet violation in this build.
    //
    // DELETE-WHEN Update / CalculatePositionedWorldRect / CalculateViewPaddingOffset land in
    // BrnMainMap.cpp and that TU mounts.
    Vector2 MainMapComponent::Update(Vector2 lv2WorldCentre)
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::MainMapComponent::Update");
        return lv2WorldCentre;
    }

    // X360 BrnGui::MainMapComponent::SetZoom @0x82469A38 (json name field verified).
    // Declared BrnMainMap.h:111. 126 lines; needs ApplyZoom @0x8245EE78 (no body in tree).
    // OutputGuiEvent<GuiAudioTriggerEvent>, its other collaborator, IS already in the build.
    //
    // void return, so no ratchet applies. Sole call site BrnPreRaceFlyBy_wJ_04.cpp:295
    // (behind IsMapApplicableToGameMode); the visible consequence of the gate is that the
    // pre-race map never zooms to fit the event -- and on the stunt path the map is not
    // drawn at all.
    //
    // DELETE-WHEN SetZoom + ApplyZoom land in BrnMainMap.cpp and that TU mounts.
    void MainMapComponent::SetZoom(ZoomFactor leZoomFactor, float lfCustomZoom, bool lbApplyNow)
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::MainMapComponent::SetZoom");
        (void)leZoomFactor;
        (void)lfCustomZoom;
        (void)lbApplyNow;
    }

    // X360 BrnGui::MapManager::RecvEvent @0x8244F898 (declared BrnMapManager.h:68; bodied
    // only in the UNMOUNTED BrnMapManager.cpp, which cannot mount -- its :20/:30 hand-declare
    // CRT closure artifacts with no definition; see the bat's own note at its SatNav block).
    //
    // ⚠️ MUST BE A GATE, NOT THE REAL BODY (conductor decision 2026-08-27, replacing the
    // MainMapComponent::RecvEvent gate that stood here -- the REAL RecvEvent now links from
    // the mounted BrnMainMap.cpp and forwards here unconditionally). The embedded MapManager
    // is NOT constructed on this build: BrnHudStatesLinkStubs.cpp:173 gives it an EMPTY ctor
    // and MainMapComponent::Construct's MapManager sub-construct is FLAG-gated, so
    // mpStateInterface / mpAllocator / mapDirectories are uninitialised -- a real
    // MapManager::RecvEvent would dereference garbage.
    //
    // void return, no ratchet. Reached every frame the fly-by map arm forwards an event.
    //
    // DELETE-WHEN BrnMapManager.cpp mounts (after the ctor split + MapManager::Construct
    // @0x82458590 lands, per the FLAG boundary in BrnMainMap.cpp).
    void MapManager::RecvEvent(const CgsModule::Event* lpEvent, int32_t liEventType)
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::MapManager::RecvEvent");
        (void)lpEvent;
        (void)liEventType;
    }

    // =============================================================================================
    //  MapTransform -- TU of record: SharedClasses/Gui/SatNav/BrnMapUtils.cpp
    //  (ALREADY MOUNTED, build_game_exe.bat:4486; it already carries every other
    //  MapTransform static -- MakeCoordSpaceFromRect :103, MakeCoordSpaceFromPoints :119,
    //  Transform :130/:149, MakeTransform :156). This gate therefore costs ZERO bat lines
    //  to retire: the real body moves into BrnMapUtils.cpp and this block is deleted.
    // =============================================================================================

    // X360 BrnGui::MapTransform::CalculateZoomFactor @0x8244F318 (json name field verified).
    // Declared BrnMapUtils.h:88, declaration-only. 109 lines of pure static map-space math
    // (no unreconstructed callee -- this is the cheapest of the ten to retire).
    //
    // ⭐ RETURN VALUE = 1.0f, DELIBERATELY NOT 0.0f. Sole call site
    // BrnPreRaceFlyBy_wJ_05.cpp:293, whose result is handed straight to the (also gated)
    // MainMapComponent::SetZoom as its E_ZOOMFACTOR_CUSTOM scale. A zoom SCALE of zero is
    // the classic placeholder-zero trap: the moment SetZoom lands ahead of this body, a 0
    // scale collapses the view rect and turns the first divide by an extent into inf/NaN,
    // and nothing on the path is non-finite until then, so no tripwire fires. 1.0f is the
    // identity scale: the map, if it were drawn, stays at its unzoomed extent.
    // [FLAG scaffold constant] 1.0f is NOT read from the image -- it is the inert identity.
    //
    // DELETE-WHEN the real body lands in SharedClasses/Gui/SatNav/BrnMapUtils.cpp (no bat
    // change needed -- that TU is already mounted).
    f32 MapTransform::CalculateZoomFactor(Vector2 lv2A, Vector2 lv2B, Vector2 lv2C, f32 lfBase)
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::MapTransform::CalculateZoomFactor");
        (void)lv2A;
        (void)lv2B;
        (void)lv2C;
        (void)lfBase;
        return 1.0f;
    }

    // =============================================================================================
    //  GuiCache -- TU of record: GameSource/Gui/BrnGuiCache.cpp and its mounted partfiles
    //  (BrnGuiCache_wB_04/_wB_06/_wB_07/_wB_09/_wH3b, bat 4491-4499).
    // =============================================================================================

    // X360 BrnGui::GuiCache::GetLandmarkInfoFromIndex @0x82506688 (json name field verified).
    // Declared BrnGuiCache.h:465, declaration-only. 60 lines. PREREQUISITE ALREADY PRESENT:
    // the forwardee WorldDataController::GetLandmarkInfoFromIndex exists and is mounted
    // (BrnGuiWorldDataController.cpp:308) -- the missing half is only the cache face and
    // the out-record fill.
    //
    // ⭐ RETURN VALUE = lpOutIconInfo (the console returns the out pointer), with the record
    // filled by FillInertIconInfo above. The caller
    // (BrnPreRaceFlyBy_wJ_04.cpp:88, SetEventIconResource) ignores the return and instead
    // switches on `lLandmarkInfo.GetCgsId()` read out of ITS OWN STACK STRUCT -- so the
    // load-bearing part of this gate is that the record is WRITTEN AT ALL. With the inert
    // id 0, that switch takes its `default:` arm, which is the console's own SKIPPABLE
    // assert ("Invalid destination ID (skippable) - ", wJ_04 cpp:1723) followed by the
    // north icon (muId 105, "DestN"). That is a deterministic, already-handled outcome; the
    // alternative -- not writing the record -- is a stack-garbage switch.
    // ⚠️ It also means the FLAG log below is joined by a console assert on the OFFLINE_RACE
    // / MARKED_MAN fly-by. Both are expected until the body lands; neither is fatal.
    //
    // DELETE-WHEN the real body lands in a MOUNTED GuiCache partfile.
    GuiEventUpdateSatNav::SatNavIconInfo*
        GuiCache::GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex,
                                           GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::GetLandmarkInfoFromIndex");
        (void)lLandmarkIndex;
        FillInertIconInfo(lpOutIconInfo);
        return lpOutIconInfo;
    }

    // X360 BrnGui::GuiCache::GetLandmarkInfoFromID @0x825067E0 (json name field verified).
    // Declared BrnGuiCache.h:480, declaration-only; `void` return per DWARF
    // (dwarfdump BrnGuiCache.h:798). 60 lines. PREREQUISITE ALREADY PRESENT: the forwardee
    // WorldDataController::GetLandmarkInfoFromID is mounted
    // (BrnGuiWorldDataController.cpp:340). The header comment at BrnGuiCache.h:471-481
    // already pins the field-fill list and the "r3 at return is the DistrictToCounty
    // leftover, NOT a result" gotcha -- do not resurrect a return value when bodying this.
    //
    // ⭐ THE OUT-RECORD FILL IS THE WHOLE POINT OF THIS GATE. Sole fly-by call site
    // BrnPreRaceFlyBy_wJ_05.cpp:269, inside PreRaceFlyByState::CalculateZoomFactor's
    // checkpoint loop, which immediately reads `lLandmarkInfo.GetPositionLane()` and folds
    // it into a running vminfp/vmaxfp bounding box. An unwritten record would feed stack
    // garbage into that box -- unbounded extents, then a division by them. The zero lane
    // keeps the box finite (it merely includes the world origin), and the whole result is
    // consumed by the gated MapTransform::CalculateZoomFactor above anyway.
    //
    // DELETE-WHEN the real body lands in a MOUNTED GuiCache partfile.
    void GuiCache::GetLandmarkInfoFromID(CgsID lLandmarkID,
                                         GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::GetLandmarkInfoFromID");
        (void)lLandmarkID;
        FillInertIconInfo(lpOutIconInfo);
    }

    // X360 BrnGui::GuiCache::GetEventDestinationLandmarkIndex @0x8240FA88 (json name field
    // verified). Declared BrnGuiCache.h:823.
    //
    // ⚠️⚠️ A FINISHED BODY FOR THIS SYMBOL ALREADY EXISTS IN THE TREE, at
    // GameSource/Gui/BrnGuiCache_wB_res.cpp:69 -- in an UNMOUNTED TU. S1 measured that
    // mounting wB_res itself is net-negative (it closes this 1 symbol, OPENS 2 --
    // BrnGui::GetPresetEventAtIndex and BrnTraffic::GetScoringTrafficDataElement -- and
    // carries a hard LNK2005: GuiCache::GetPresetEvent(s32) is already defined by the
    // MOUNTED BrnGuiCache_wH3b.cpp). The right retirement is to MOVE the wB_res:69 body
    // into a mounted GuiCache partfile. ⭐ THE MOVE AND THIS DELETION MUST BE ONE CHANGE --
    // this gate and that body cannot coexist in one build (LNK2005).
    //
    // ⭐ RETURN VALUE = LandmarkIndex(-1) == the canonical K_INVALID_LANDMARK sentinel
    // (BrnModeManager.h:633 spells it exactly that way). Sole fly-by call site is
    // BrnPreRaceFlyBy_wJ_04.cpp:88, where the value is passed straight into the gated
    // GetLandmarkInfoFromIndex above and never inspected -- so no caller is steered by this
    // number today. -1 is chosen over 0 precisely because 0 is a VALID landmark index: if a
    // future wave lands the landmark-info body ahead of this one, the invalid sentinel trips
    // that body's own assert loudly instead of silently resolving landmark #0.
    //
    // DELETE-WHEN the BrnGuiCache_wB_res.cpp:69 body moves into a mounted GuiCache partfile.
    BrnGameState::LandmarkIndex GuiCache::GetEventDestinationLandmarkIndex() const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::GetEventDestinationLandmarkIndex");
        return BrnGameState::LandmarkIndex(-1);   // K_INVALID_LANDMARK
    }

    // X360 BrnGui::GuiCache::RefreshMapState @0x82510F40 (json name field verified).
    // Declared BrnGuiCache.h:887, declaration-only.
    //
    // ⚠️ DECEPTIVE COST -- NOT A LEAF. The body is 8 pseudocode lines, but it calls
    // GuiCache::UpdateTrackerInfo @0x82506F28 (68 lines, NO BODY IN TREE -- BrnGuiCache.cpp:1120
    // says so in as many words) and the still-unnamed sub_82507070 (70 lines). Retiring this
    // gate is a THREE-function slice; do not schedule it as a one-liner.
    //
    // void return. Sole call site BrnPreRaceFlyBy_wJ_02.cpp:317 (OnLeave, inside the
    // IsMapApplicableToGameMode arm) -- the fly-by re-publishing the map state after tearing
    // its screen down. On the stunt path the map was never published, so there is nothing to
    // refresh.
    //
    // DELETE-WHEN RefreshMapState + UpdateTrackerInfo + sub_82507070 land in a MOUNTED
    // GuiCache partfile.
    void GuiCache::RefreshMapState()
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::RefreshMapState");
    }

    // X360 BrnGui::GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID
    // @0x825071C8 (json name field verified). Declared BrnGuiCache.h:897, declaration-only.
    // 112 lines. ⭐ The header at :890-896 already carries the PPC float-arg-GPR-skip note --
    // THE recurring campaign bug -- for whoever bodies this: at the call site r4 =
    // GetEventID(), f1 = the clamped animation t (the float SKIPS its GPR slot, so r5 is
    // dead) and r6 = 0. Signature is `s32 (u32, f32, bool)`, DWARF BrnGuiCache.h:1476.
    //
    // ⭐ RETURN VALUE = 0. The call site is BrnPreRaceFlyBy_wJ_01.cpp:413, inside
    // UpdateIconManager, and the result is TESTED: `if (miPreviousIconCount <
    // liNumActiveIcons) { ...OutputGuiEvent(CodeMapScrollEnd chirp)... }` then
    // `miPreviousIconCount = liNumActiveIcons;`. Zero means "no landmark icons are active",
    // which is both true (the icon manager is null on this path) and the value that holds
    // the comparison false forever -- miPreviousIconCount starts at 0 and this gate never
    // raises it, so the reveal chirp cannot fire spuriously. Any positive constant would
    // fire the audio event exactly once, on a path with no icons to accompany it.
    // ⚠️ A second consumer is already waiting: BrnCrashNavMap_wJ_06.cpp:280.
    //
    // DELETE-WHEN the real body lands in a MOUNTED GuiCache partfile.
    s32 GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(u32 luEventID, f32 lfT,
                                                                        bool lbFlag)
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID");
        (void)luEventID;
        (void)lfT;
        (void)lbFlag;
        return 0;
    }
}

namespace BrnProgression
{
    // =============================================================================================
    //  DerivedCarArray -- TU of record: NONE. ⚠️ There is no BrnDerivedCars.cpp anywhere in
    //  the tree, and there was never meant to be: every function of this class was defined
    //  IN THE HEADER in the original source, measured from the CGS_ASSERT __FILE__ the X360
    //  bakes in (".../Progression/BrnDerivedCars.h", see the WHERE THE BODIES LIVE banner at
    //  BrnDerivedCars.h:33-53). So the faithful retirement of this gate is a body written
    //  INTO BrnDerivedCars.h -- which costs ZERO bat lines and deletes this block -- NOT a
    //  new BrnDerivedCars.cpp. The out-of-line definition below exists only because a
    //  declaration-only header member has to be defined somewhere to link.
    // =============================================================================================

    // X360 BrnProgression::DerivedCarArray::ConstructPatternLiveryList @0x823751C0 (json
    // name field verified). Declared BrnDerivedCars.h:96; DWARF h:61, body h:~156-205.
    // 134 lines. BLOCKED ON A HEADER THIS FILE DOES NOT OWN: the real walk reads
    // BrnResource::VehicleListEntry's parent-car id at entry+0x08 and the livery-kind byte
    // at entry+0xE9 behind IsLiveryColour(), neither of which the committed
    // SharedClasses/DataLists/VehicleListEntry.h exposes yet (BrnDerivedCars.h:44-50).
    // Its sibling ConstructColourLiveryList @0x82374F60 is blocked identically.
    //
    // ⭐ THE TWO Clear() CALLS ARE NOT DECORATION -- THEY ARE THE MINIMUM CORRECT INERT
    // STATE. Array<T,N>'s default ctor seeds BOTH count words with the KI_UNCONSTRUCTED
    // (-1) sentinel (CgsArray.h:30/:74), and every accessor asserts against it ("Array used
    // before Construct/Clear was called"). The real builder's first act is to zero both
    // count words (X360 0x82374F60: `*(this+64)=0; *(this+104)=0`), so this gate performs
    // exactly that prefix and stops. Skipping it would leave the caller's array in the
    // pre-Construct state and trip an assert storm on the FIRST accessor.
    //
    // ⚠️ WHAT THE CALLER THEN SEES, stated plainly rather than left to be discovered:
    // BrnPreRaceFlyBy_wJ_06.cpp:177 (SetBurningRouteDescription) walks
    // `while (liVariant < (s32)GetLength() && GetItem(liVariant) == lPlayersCarId)`. With
    // length 0 the walk does not run, liVariant stays 0, and the following GetItem(0) trips
    // Array's own "Array index out of bounds" assert (a no-op CGS_ASSERT in this build) and
    // reads slot 0 of the caller's uninitialised stack array -- so the burning-route
    // description shows a garbage car name. That is BURNING_ROUTE only; it is not on the
    // stunt path, and it is strictly better than the alternatives (appending the parent id
    // just moves the out-of-bounds read to index 1, and fabricating a variant id would put
    // an invented car in a shipped string). ⭐ MOUNTING ANY OTHER CONSUMER MAKES THIS
    // VISIBLE: BrnCarSelectManager.cpp:473 and BrnProgressionManager.cpp:345 both carry
    // HONEST PARTIAL notes naming this class, and BrnProgressionManager_EventFinish.cpp:394
    // and :417 call it directly. Land the real body with any of those mounts.
    //
    // DELETE-WHEN the real body lands in BrnDerivedCars.h (after VehicleListEntry.h grows
    // the parent-id and livery-kind faces).
    void DerivedCarArray::ConstructPatternLiveryList(const BrnResource::VehicleList* lpVehicleList,
                                                     const CgsID& lParentOrSiblingCarId)
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnProgression::DerivedCarArray::ConstructPatternLiveryList");
        (void)lpVehicleList;
        (void)lParentOrSiblingCarId;

        Clear();                 // the CgsID base array   (X360 `*(this+64)=0`)
        maLiveryTypes.Clear();   // the parallel kind array (X360 `*(this+104)=0`)
    }
}
