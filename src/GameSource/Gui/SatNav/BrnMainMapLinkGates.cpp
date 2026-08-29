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
// (HISTORY, retired 2026-08-29 main-menu wave: this banner used to mandate keeping the
// MapManager::MapManager() {} stub in BrnHudStatesLinkStubs.cpp. The real ctor is now
// mounted -- BrnMapManager.cpp was made mountable by replacing its two hand-declared fake
// CRT externals with real construction loops -- so that stub is DELETED and the ctor,
// RecvEvent, MapTransform::CalculateZoomFactor, MainMapComponent::SetZoom and the five
// GuiCache map members all left this file for their homes. Only the gates still present
// below survive.)
//
// DELETE-WHEN: each surviving gate carries its own DELETE-WHEN line. The whole FILE dies
// (and its bat line at the SatNav block goes with it) when the last one lands a real body.
// =================================================================================================


#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector2 / Vector4 / CgsID

#include "GameSource/BurnoutConstants.h"                     // E_ACTIVE_RACE_CAR_INDEX_INVALID
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (by value)
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
    // (FillInertIconInfo DELETED 2026-08-29, main-menu wave: its only callers were the two
    //  GuiCache landmark gates, whose real bodies landed in BrnGuiCache_wJ_01.cpp.)
}

namespace BrnGui
{
    // (gate for MainMapComponent::Update DELETED 2026-08-29, main-menu wave: the real pump
    //  @0x824696E8 + ApplyZoom @0x8245EE78 landed in BrnMainMap.cpp, verifier-PASSed.)
    // (gate for MainMapComponent::SetZoom DELETED 2026-08-29, main-menu wave: real body landed.)
    // (gate for MapManager::RecvEvent DELETED 2026-08-29, main-menu wave: real body landed.)
    // (gate for MapTransform::CalculateZoomFactor DELETED 2026-08-29, main-menu wave: real body landed.)

    // =============================================================================================
    //  GuiCache -- TU of record: GameSource/Gui/BrnGuiCache.cpp and its mounted partfiles
    //  (BrnGuiCache_wB_04/_wB_06/_wB_07/_wB_09/_wH3b, bat 4491-4499).
    // =============================================================================================

    // (gate for GuiCache::GetLandmarkInfoFromIndex DELETED 2026-08-29, main-menu wave: real body landed.)
    // (gate for GuiCache::GetLandmarkInfoFromID DELETED 2026-08-29, main-menu wave: real body landed.)
    // (gate for GuiCache::GetEventDestinationLandmarkIndex DELETED 2026-08-29, main-menu wave: real body landed.)
    // (gate for GuiCache::RefreshMapState DELETED 2026-08-29, main-menu wave: real body landed.)
    // (gate for GuiCache::HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID DELETED 2026-08-29, main-menu wave: real body landed.)

    // X360 BrnGui::GuiCache::GetOnlineFinishPoint @0x82506940 (json name field verified).
    // Declared BrnGuiCache.h:533, declaration-only. The real body asserts
    // mpWorldDataController, popcount-walks the 256-bit maOnlineFinishPointsMask @+0x7770 to
    // the liIndex-th set bit, resolves it through the events-with-unique-finish list
    // (cache+0x8040) and WorldDataController::GetLandmarkInfoFromIndex, and fills the out
    // record. Its ONLY caller (CrashNavIconRenderer::GetIconInformation's
    // ONLINE_FINISH_POINTS arm, BrnCrashNavIconRenderer_wK_01.cpp:898) iterates
    // CountSetBits() of that same mask -- which is ZERO offline -- so this gate is
    // unreachable in the offline main-menu milestone; it exists only to close the link.
    // ⭐ RETURN VALUE = lpOutIconInfo, record ZEROED (deterministic, never a stack-garbage
    // read; the [[placeholder-zero]] rule is satisfied because no reachable caller consumes
    // the zeros). [FLAG link scaffold]
    // DELETE-WHEN the real body lands in a MOUNTED GuiCache partfile (a ledger item of the
    // GuiCache TU -- the popcount walk + the 0x8040-list element lookup).
    GuiEventUpdateSatNav::SatNavIconInfo*
    GuiCache::GetOnlineFinishPoint(s32 /*liIndex*/,
                                   GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const
    {
        static bool sbLogged = false;
        LogGateOnce(sbLogged, "BrnGui::GuiCache::GetOnlineFinishPoint");
        if (lpOutIconInfo != 0)
        {
            std::memset(lpOutIconInfo, 0, sizeof(*lpOutIconInfo));
        }
        return lpOutIconInfo;
    }
}

// (The DerivedCarArray::ConstructPatternLiveryList gate DIED 2026-08-29: the real body
//  landed header-inline in BrnDerivedCars.h (flyby map-arm wave, dev 8fc3718a) --
//  exactly the "body written INTO BrnDerivedCars.h" retirement its banner predicted.)
