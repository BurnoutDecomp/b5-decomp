// BrnMapIconManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The BrnGui::MapIconManager members the
// X360 ARTIST build emits as out-of-line functions in this slice:
//   MapIconManager (ctor)            @ 0x827DF138  -- bring the three icon pools up live
//   ResetOwnerParameter              @ 0x824EBF98  -- owner id back to E_OWNERID_INVALID
//   SetRoadRuleBatchData             @ 0x824B2F80  -- forward to the road-sign icon manager
//   GetNumRivalIcons                 @ 0x824F7B60  -- count rival/network/marked-man icons
//   GetDriveThroughOrJunkyardAtIndex @ 0x824FAC10  -- selection-index lookup over the cache
//   AddTeamToNetworkRivals           @ 0x824F4FF8  -- stamp network-rival icons with a team
//
// ⭐ H3b (2026-08-25) adds the owner/update surface the sat-nav component links against:
//   Construct         @ 0x824FA0F0    SetOwnerParameters @ 0x82520CE8
//   ReleaseResources  @ 0x82520C40    Update             @ 0x82525EF8
//   SetIconsVisible   @ 0x82525F18    UpdateSatNavInfo   @ 0x825023D0
//   UpdateSatNavParams@ 0x824F4458    IsActiveRival      @ 0x824FAE60
//   SetZoomFactor     (inline-folded; the one-line forward per the header note)
// ⭐ H3c (2026-08-25): the SAT-NAV icon pass is LANDED -- the 16-element apt icon pool
// (mSatNavMapIcons, the FlaptIconComponent+SatNavMapIcon pairs), the SetOwnerParameters
// bind pass' sat-nav half, UpdateSatNavIcons @0x82522588 with its support surface
// (UpdateWorldIcons @0x82511C88 drive-through pass, CalculateAlpha @0x82502940,
// GetSatNavIconStateForRival @0x824FA320, IconDisplaySort @0x824F4508) and the
// UpdateSatNavInfo two-flag FIX (the player record now reaches the used set -- the
// yellow arrow). NAMED GATES remaining (each one-shot logged at its site): the
// crash-nav pool half (component ctor unreconstructed) + UpdateCrashNavIcons
// @0x825212C0, the landmark/checkpoint passes + the case-4 landmark state machine,
// the online-route start/finish-point lookups, the online colour lookups and the
// LARGE-map rival naming arm.
//
// All branch conditions and the compared constants come from the X360 asm/pseudocode; the
// game-mode and icon-type literals are resolved to their canonical enumerators (DecFIGS
// DWARF): EGameModeType (BrnGameStateSharedIO.h) and SatNavIconType (BrnGuiEventTypeDefs.h).

#include "GameSource/Gui/SatNav/BrnMapIconManager.h"
#include "GameSource/Gui/BrnGuiCache.h"               // BrnGui::GuiCache accessors (drive-throughs / team / mode)
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameSource/BurnoutConstants.h"               // EActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // [H3c] CgsCore::SPrintf (the icon-name pass)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Message::gxMessageFilterFlags / CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // [H3c] StateInterface::GetAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"         // [H3c] GuiAccessPointers::GetFlaptManager
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"            // [H3c] BrnFlapt::FlaptManager::GetFile
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"            // [H3c] BrnFlapt::FileRef
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"        // [H3c] the GuiPlayerInfo view (marked-man / rival heading)
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // MapTransform::GetZoomedWorldRect (the icon bounds test)

#include <cmath>     // [H3c] sinf/cosf/acosf/sqrtf (the rival FOV cone / CalculateAlpha)
#include <cstdlib>   // [H3c] qsort (the IconDisplaySort pass)

namespace BrnGui
{
namespace
{
    // Game-mode constants the drive-through/junkyard filter branches on (DWARF:
    // BrnGameState::GameStateModuleIO::EGameModeType).
    typedef BrnGameState::GameStateModuleIO::EGameModeType EGameModeType;
    using BrnGameState::GameStateModuleIO::E_MODE_NONE;                 // -1
    using BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE;           //  3
    using BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN;          //  8
    using BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY; // 15

    // Icon-type constants (DWARF: GuiEventUpdateSatNav::SatNavIconType).
    typedef GuiEventUpdateSatNav::SatNavIconInfo SatNavIconInfo;
}

// @ 0x827DF138
//   The X360 ctor walks each of the three icon pools and writes the per-element vtable
//   pointers (50 crash-nav icons @ stride 0x1F0 from +0x9A0, 16 sat-nav icons @ stride
//   0x60 from +0x6A80, and 64 road-sign-icon slots @ stride 0xC0 from +0x7090). In C++
//   those vtable writes are simply the pool members' own construction, so the manager's
//   constructor lets its members default-construct.
MapIconManager::MapIconManager()
{
}

// @ 0x824EBF98
//   Reset the owner id to invalid, emitting a debug trace when message logging is enabled.
void MapIconManager::ResetOwnerParameter()
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "MAPICONMANAGER: Resetting OwnerID to Invalid.\n";
    }

    mOwnerId = E_OWNERID_INVALID;   // X360 stores 0 at +0xAA00
}

// @ 0x824B2F80
//   Forward the road-rule batch response straight into the embedded road-sign icon manager
//   (the X360 reaches it at this+0x7090). The pointer is asserted non-null first.
void MapIconManager::SetRoadRuleBatchData(const GuiEventRoadRuleBatchDataResponse* lpRoadRules)
{
    CGS_ASSERT(lpRoadRules != nullptr, "lpRoadRules");

    mRoadSignIconManager.SetRoadRuleBatchData(lpRoadRules);
}

// @ 0x824F7B60
//   Count the rival icons in the active sat-nav info set: an icon counts if its icon type
//   is a network rival, a marked man or an ordinary rival.
s32 MapIconManager::GetNumRivalIcons() const
{
    s32 liNumRivalIcons = 0;

    for (s32 liIndex = 0; liIndex < miNumUsedIcons; ++liIndex)
    {
        const s8 li8IconType = mSatNavIconInfo[liIndex].GetIconTypeByte();
        CGS_ASSERT(li8IconType >= 0, "leIconType >= 0");
        CGS_ASSERT(li8IconType < SatNavIconInfo::E_SATNAVICON_MAX, "leIconType < E_SATNAVICON_MAX");

        if (li8IconType == SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL ||
            li8IconType == SatNavIconInfo::E_SATNAVICON_MARKED_MAN ||
            li8IconType == SatNavIconInfo::E_SATNAVICON_RIVAL)
        {
            ++liNumRivalIcons;
        }
    }

    return liNumRivalIcons;
}

// @ 0x824F4FF8  (the network-rivals pass of UpdateSatNavIcons)
//   For every network-rival icon, look up the player's current online team for that icon's
//   active-race-car index and stamp the icon with it.
void MapIconManager::AddTeamToNetworkRivals()
{
    for (s32 liIndex = 0; liIndex < miNumUsedIcons; ++liIndex)
    {
        SatNavIconInfo& lrIcon = mSatNavIconInfo[liIndex];

        const s8 li8IconType = lrIcon.GetIconTypeByte();
        CGS_ASSERT(li8IconType >= 0, "leIconType >= 0");
        CGS_ASSERT(li8IconType < SatNavIconInfo::E_SATNAVICON_MAX, "leIconType < E_SATNAVICON_MAX");

        if (li8IconType == SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
        {
            const EActiveRaceCarIndex leActiveRaceCarIndex = lrIcon.GetActiveRaceCarIndex();
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            const s32 li8PlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(leActiveRaceCarIndex);
            // X360 GsmIO::E_PLAYER_TEAM_START == 0, GsmIO::E_PLAYER_TEAM_COUNT == 9.
            CGS_ASSERT(li8PlayerTeam >= E_PLAYER_TEAM_START, "lePlayerTeam >= GsmIO::E_PLAYER_TEAM_START");
            CGS_ASSERT(li8PlayerTeam < E_PLAYER_TEAM_COUNT, "lePlayerTeam < GsmIO::E_PLAYER_TEAM_COUNT");

            lrIcon.SetPlayerTeamByte(static_cast<s8>(li8PlayerTeam));
        }
    }
}

// @ 0x824FAC10
//   Return the drive-through / junkyard sat-nav icon that the map UI's selection sits on.
//   When rival selection is enabled the rival icons occupy the front of the selection list,
//   so the rival count is subtracted off the incoming index first. Hidden drive-throughs are
//   always skipped; in road-rage / marked-man the paint-shop entry is skipped, and outside a
//   live (or free-burn-lobby) game the junkyard entry is skipped.
const GuiEventUpdateSatNav::SatNavIconInfo* MapIconManager::GetDriveThroughOrJunkyardAtIndex(s32 liIndex) const
{
    s32 liRemaining = liIndex;
    if (mbAllowRivalSelection)
    {
        liRemaining = liIndex - GetNumRivalIcons();
    }

    CGS_ASSERT(mpGuiCache != nullptr, "mpGuiCache");

    const bool lbSkipPaintShop =
        (mpGuiCache->GetCurrentGameModeType() == E_MODE_ROAD_RAGE ||
         mpGuiCache->GetCurrentGameModeType() == E_MODE_MARKED_MAN);

    const s32 liNumDriveThroughs = mpGuiCache->GetNumberOfDriveThroughs();
    for (s32 liDriveThrough = 0; liDriveThrough < liNumDriveThroughs; ++liDriveThrough)
    {
        CGS_ASSERT(liDriveThrough < mpGuiCache->GetNumberOfDriveThroughs(),
                   "liIndex < miNumDriveThroughs");

        const SatNavIconInfo* lpIcon = mpGuiCache->GetDriveThrough(liDriveThrough);

        if (!lpIcon->IsHiddenDriveThru() &&
            (!lbSkipPaintShop || lpIcon->GetIconTypeByte() != SatNavIconInfo::E_SATNAVICON_PAINT_SHOP))
        {
            const s32 liGameMode = mpGuiCache->GetCurrentGameModeType();
            const bool lbAllowJunkyard =
                (liGameMode == E_MODE_NONE || liGameMode == E_MODE_ONLINE_FREE_BURN_LOBBY);

            if (lbAllowJunkyard || lpIcon->GetIconTypeByte() != SatNavIconInfo::E_SATNAVICON_JUNKYARD)
            {
                if (liRemaining == 0)
                {
                    return lpIcon;
                }
                --liRemaining;
            }
        }
    }

    // Not found: the X360 fires an assert ("Unable to find drivethrough or junkyard at
    // index <liIndex>") and then leaves r3 = `this` (the manager pointer), i.e. it returns
    // the manager type-punned as a SatNavIconInfo*. The assert is fatal in dev builds; the
    // returned value is only reached in retail (asserts compiled out) and is effectively a
    // poisoned non-null. Reproduced faithfully (no offset arithmetic - a pun of `this`).
    CGS_ASSERT(false, "Unable to find drivethrough or junkyard at index");
    return reinterpret_cast<const SatNavIconInfo*>(this);
}


// ============================ H3b: the owner/update surface ============================

// @ 0x824FA0F0 -- bind the cache and reset the whole selection/flag surface. The X360
// also drives all 50 crash-nav icon components invisible (vtable slot +0x10 with 0) and
// wipes the embedded event-icon manager (memset 2100 @+0xA1B4) + the road-sign manager's
// controller/cache/zoom tail -- those pools ride the parked icon slice (see the banner);
// their storage is zero-initialised static memory on this host, and the one recovered
// road-sign scalar (mfZoomFactor := 1.0) is set by name.
void MapIconManager::Construct(GuiCache* lpGuiCache)
{
    CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // BrnMapIconManager.cpp:144 (non-gating)
    mpGuiCache = lpGuiCache;

    mi8CurrentEventIndex = 0;          // +0x998 (stb 0)
    mOwnerId             = E_OWNERID_INVALID;
    miNumUsedIcons       = 0;
    miMaxNumberIcons     = 0;
    mpStateInterface     = 0;
    meIconSizeMode       = E_ICONSIZE_SMALL;
    mbIsDisplayingEventInfo = false;   // +0xAA18
    meIconFilterMode     = E_ICONFILTER_ALL;
    mbRivalFovFreeburn   = false;      // +0xAA19
    mbRivalFovRace       = false;      // +0xAA1A
    mbUseTrajectory      = false;      // +0xAA1B
    mbRotateSatNav       = false;      // +0xAA1C
    mbShowOffLineRivalsOnSatNav = false; // +0xAA1D (the halfword-1 store's zero byte)
    mbIconsVisible       = true;       // +0xAA1E (the halfword-1 store's one byte)
    mbIsActive           = true;       // +0xAA22
    mbShowingOnlineRoute = false;      // +0xAA1F
    mbShowingCrashNavRoute = false;    // +0xAA21
    mbShowingPreRaceRoute  = false;    // +0xAA20
    miSelectedCheckpoint = 0;          // +0xAA14
    mSelectedLightTriggerID = 0xFFFFFFFFu; // +0xAA0C (stw -1)
    mbUseRoadSigns       = false;      // +0xA1B0
    mbAllowDriveThruSelection = false; // +0x7080
    mbAllowRivalSelection     = false; // +0x7081
    muSelectedJunctionID = 0;          // +0xAA10
    meEventIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT; // +0xA9F0 = 5
    mbShowingDriveThrus  = true;       // +0xA9F4 (stb 1)
    mbAllowPlayerSelection = true;     // +0xA1B1 (stb 1)
    mRoadSignIconManager.SetZoomFactor(1.0f);   // +0xA198 (stfs 1.0)
}

// @ 0x82520CE8 -- take ownership of the shared icon set for one screen. The parameter
// -> member stores and the owner handshake are transcribed whole; the three sub-inits
// the X360 runs on an owner change (the 50+16 apt icon component Construct/Prepare
// loop, RoadSignIconManager::Prepare and EventIconManager::Prepare) ride the parked
// icon slice -- one-shot-logged, see the banner.
MapIconManager::OwnerId MapIconManager::SetOwnerParameters(
    CgsGui::StateInterface* lpStateInterface,
    const char* lpcComponentName,
    s32 liMaxNumberIcons,
    OwnerId leOwnerId,
    bool lbUseRoadSigns,
    bool lbShowingDriveThrus,
    bool lbAllowDriveThruSelection,
    GuiEventDrawEventIcons::EIconDisplayType leEventIconDisplayType,
    const char* lpcIconParentName)
{
    (void)lpcIconParentName;
    CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");   // :263 (non-gating)
    CGS_ASSERT(lpcComponentName != 0, "lpcComponentName");   // :264 (non-gating)

    // Owner handshake: a repeat claim by the CURRENT owner is a no-op (the X360's
    // "SetOwnerParameters called with OwnerID X when current OwnerID is Y" debug trace
    // rides the un-reconstructed verbose stream).
    if (!(leOwnerId == mOwnerId && mOwnerId != E_OWNERID_INVALID))
    {
        mpStateInterface = lpStateInterface;

        if (liMaxNumberIcons > 0)
        {
            // [H3c] the per-icon component bind pass (the X360 loop @0x82520E70..):
            // per icon a SPrintf'd "%s%d" name, the crash-nav component Construct +
            // SetState(INVISIBLE) + dirty-flag raise, and -- for icons 0..15 -- the
            // sat-nav pair: SatNavMapIcon::Construct (slot 0 of the icon vtable),
            // FlaptManager::GetFile(0), the component Prepare (clip bind + icon reset)
            // and a final SetState(INVISIBLE). The SAT-NAV half is LANDED below; the
            // crash-nav half still rides the parked crash-nav pool (component ctor
            // unreconstructed) -- one-shot-logged, not silent.
            static bool sbLoggedCrashPark = false;
            if (!sbLoggedCrashPark && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedCrashPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] PARK: MapIconManager::SetOwnerParameters crash-nav "
                       "component half skipped (crash-nav pool unreconstructed; owner "
                    << static_cast<s32>(leOwnerId) << ", " << liMaxNumberIcons << " icons)\n";
            }

            char lacIconName[257];
            for (s32 liIcon = 0; liIcon < liMaxNumberIcons; ++liIcon)
            {
                // [crash-nav component Construct + SetState(0) + the two dirty bytes:
                //  parked with the crash-nav pool -- covered by the print above]

                if (liIcon < KI_MAX_SATNAV_MAP_ICONS)
                {
                    CgsCore::SPrintf(lacIconName, 256, "%s%d", lpcComponentName, liIcon);

                    SatNavIconComponent& lrComponent = mSatNavMapIcons[liIcon];
                    lrComponent.mIcon.Construct(lacIconName, lpStateInterface,
                                                lpcIconParentName);

                    CgsGui::GuiAccessPointers* lpAccessPointers =
                        lpStateInterface->GetAccessPointers();
                    CGS_ASSERT(lpAccessPointers != 0,
                               "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344 (non-gating)
                    BrnFlapt::FlaptManager* lpFlaptManager =
                        lpAccessPointers->GetFlaptManager();
                    CGS_ASSERT(lpFlaptManager != 0,
                               "NULL != mpFlaptManager");     // CgsGuiShared.h:194 (non-gating)

                    BrnFlapt::FileRef lFile;
                    lpFlaptManager->GetFile(&lFile, 0);
                    lrComponent.Prepare(lacIconName, lFile);
                    // [DIAG] NOT IN THE X360 BINARY -- H3c: did the named clip resolve?
                    if (CgsDev::Log::gpDebugPrint != 0)
                        *CgsDev::Log::gpDebugPrint
                            << "[satnav-icon] bind '" << lacIconName << "' clip="
                            << (lrComponent.GetMovieClipRef().IsValid() ? 1 : 0)
                            << " xform="
                            << ((lrComponent.GetMovieClipRef().mpTransform != 0) ? 1 : 0)
                            << "\n";
                    lrComponent.mIcon.SetState(MapIconBrnBase::E_ICONSTATE_INVISIBLE);
                }
            }
        }

        CGS_ASSERT(liMaxNumberIcons <= KI_SATNAV_MAX_ICONS,
                   "Max icons is larger than possible max icons");   // :302 (non-gating)
        CGS_ASSERT(liMaxNumberIcons >= 0, "Max icons is negative");  // :303 (non-gating)
        miMaxNumberIcons = liMaxNumberIcons;                          // +0x994

        mbShowingDriveThrus       = lbShowingDriveThrus;              // +0xA9F4
        mbAllowDriveThruSelection = lbShowingDriveThrus && lbAllowDriveThruSelection; // +0x7080
        mbAllowRivalSelection     = false;                            // +0x7081
        mbAllowPlayerSelection    = (leOwnerId != E_CRASHNAV_MAP_ONLINE_SELECT_ROUTE); // +0xA1B1

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :313 (non-gating)
        // RoadSignIconManager::Prepare rides the parked icon slice (the 64-sign pool);
        // the visibility broadcast + flag store are the real recovered effects.
        mRoadSignIconManager.SetSignsVisible(lbUseRoadSigns ? 1 : 0);
        mbUseRoadSigns = lbUseRoadSigns;                              // +0xA1B0

        if (leEventIconDisplayType != GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT)
        {
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :321 (non-gating)
            // [UI-gate] EventIconManager::Prepare -- parked with the icon slice (the
            // embedded manager is not modelled); covered by the one-shot print above.
        }
        meEventIconDisplayType = leEventIconDisplayType;              // +0xA9F0

        mbShowingOnlineRoute   = false;   // +0xAA1F
        mbShowingPreRaceRoute  = false;   // +0xAA20
        mbShowingCrashNavRoute = false;   // +0xAA21
        mbIsActive             = true;    // +0xAA22
        miSelectedCheckpoint   = 0;       // +0xAA14
        mSelectedLightTriggerID = 0xFFFFFFFFu; // +0xAA0C
        muSelectedJunctionID   = 0;       // +0xAA10

        mOwnerId = leOwnerId;             // +0xAA00
    }

    return mOwnerId;
}

// @ 0x82520C40 -- give the icon set back. A no-op unless leOwnerId is the current
// owner; otherwise unregister the 64 road-sign object controllers + release the event
// icons (both ride the parked icon slice), raise mbIsActive and reset the owner id.
void MapIconManager::ReleaseResources(CgsGui::StateInterface* lpStateInterface, OwnerId leOwnerId)
{
    (void)lpStateInterface;
    if (leOwnerId != mOwnerId)
        return;

    // [UI-gate] the 64 ObjectController::UnRegister calls + EventIconManager::
    // ReleaseResources (when the display type is real) ride the parked icon slice.
    static bool sbLoggedReleasePark = false;
    if (!sbLoggedReleasePark && CgsDev::Log::gpDebugPrint != 0)
    {
        sbLoggedReleasePark = true;
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] PARK: MapIconManager::ReleaseResources controller/event-icon "
               "release skipped (apt icon pools unreconstructed)\n";
    }

    mbIsActive = true;   // +0xAA22 (the X360's stbx 1 -- the stored value is what it writes)
    ResetOwnerParameter();
}

// @ 0x82525EF8 -- the per-frame icon pass for the current owner.
void MapIconManager::Update()
{
    if (mOwnerId == E_SATNAV_MAP)
        UpdateSatNavIcons();
    else
        UpdateCrashNavIcons();
}

// @ 0x82525F18 -- show/hide the whole on-map icon set. Hiding also clears the used-icon
// count and re-runs the owner's icon pass so the components latch the hidden state.
void MapIconManager::SetIconsVisible(bool lbVisible)
{
    mbIconsVisible = lbVisible;   // +0xAA1E
    if (!lbVisible)
    {
        miNumUsedIcons = 0;       // +0x990
        if (mOwnerId == E_SATNAV_MAP)
            UpdateSatNavIcons();
        else
            UpdateCrashNavIcons();
    }
}

// @ 0x825023D0 -- refresh the per-icon sat-nav info set from the incoming id-199 event.
// ⭐ H3c FIX: the X360 keeps TWO flags per record -- r31 (apply the zoomed-window bounds
// test) and r28 (wanted) -- and the earlier transcription collapsed them into one, which
// DROPPED the player record. Console truth: the PLAYER record is latched aside into
// mPlayerIconInfo AND still appended to the used set (bounds test skipped); marked-man /
// network-rival records skip the bounds test in SMALL mode (the clamped edge arrow) and
// are bounds-tested only in LARGE mode; only the rival filter can skip a record outright.
void MapIconManager::UpdateSatNavInfo(const GuiEventUpdateSatNav* lpSatNavEvent)
{
    CGS_ASSERT(lpSatNavEvent != 0, "NULL != lpSatNavInfo");   // :354 (non-gating)

    if (mbShowingOnlineRoute || mbShowingPreRaceRoute)        // +0xAA1F / +0xAA20
        return;

    const s32 liNumIcons = lpSatNavEvent->miNumIcons;         // event +0x900
    const Vector4& lv4Bounds = MapTransform::GetZoomedWorldRect();   // @0x82FB3440
    const s32 liGameMode = mpGuiCache->GetGameMode();         // cache +0x9E58

    for (s32 liIcon = 0; liIcon < liNumIcons; ++liIcon)
    {
        if (miNumUsedIcons >= miMaxNumberIcons)
            break;

        const SatNavIconInfo& lrIcon = lpSatNavEvent->maIconInfo[liIcon];
        bool lbBoundsTest = true;   // X360 r31
        bool lbWanted     = true;   // X360 r28

        switch (lrIcon.GetIconTypeByte())
        {
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:   // 0
            mPlayerIconInfo = lrIcon;   // the +0x960 aside copy
            lbBoundsTest = false;       // the player record is ALWAYS kept
            break;

        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_MARKED_MAN:   // 1
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL: // 2
            if (meIconSizeMode == E_ICONSIZE_SMALL)   // +0xAA04 == 0
                lbBoundsTest = false;   // small mode: kept unclipped (edge-clamped arrow)
            break;

        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL:        // 3
        {
            const s32 liRaceCarIndex = lrIcon.GetActiveRaceCarIndex();
            const bool lbFilteredMode =
                liGameMode == 3 || liGameMode == 7 || liGameMode == 9 ||
                liGameMode == 5 || liGameMode == 8;
            if (lbFilteredMode
                || liRaceCarIndex < 0
                || mpGuiCache->HasRaceCarFinished(
                       static_cast<EActiveRaceCarIndex>(lrIcon.GetActiveRaceCarIndex()))
                || !IsActiveRival(&lrIcon))
            {
                if (liGameMode != -1)
                    continue;              // skip the icon entirely (the X360 LABEL_38 hop)
                if (!IsActiveRival(&lrIcon))
                    continue;
                lbBoundsTest = true;
            }
            else
            {
                lbBoundsTest = (liGameMode != 4);
            }
            break;
        }

        default:
            break;   // every other type passes straight to the bounds test
        }

        // The zoomed-window bounds test (the X360 lane picks over @0x82FB3440:
        // {minX, maxZ, maxX, minZ} in the default pi-rotated frame) -- only for the
        // records whose type asked for it; the rest append unconditionally.
        if (lbBoundsTest)
        {
            const Vector4& lv4Pos = lrIcon.GetPositionLane();
            if (lv4Bounds.x > lv4Pos.x || lv4Pos.x > lv4Bounds.z ||
                lv4Pos.z > lv4Bounds.y || lv4Bounds.w > lv4Pos.z)
                lbWanted = false;
        }

        if (!lbWanted)
            continue;

        CGS_ASSERT(miNumUsedIcons < miMaxNumberIcons,
                   "Trying to update too many icons");   // :456 (non-gating)
        mSatNavIconInfo[miNumUsedIcons] = lrIcon;        // the 48-byte record copy
        ++miNumUsedIcons;
    }
}

// @ 0x824F4458 -- adopt the id-200 sat-nav parameter record: five payload bytes into
// the flag tail (+0xAA19..+0xAA1D).
void MapIconManager::UpdateSatNavParams(const CgsModule::Event* lpParamsEvent)
{
    CGS_ASSERT(lpParamsEvent != 0, "lpParams");   // :766 (non-gating)
    const u8* lpuPayload = reinterpret_cast<const u8*>(lpParamsEvent);
    mbRivalFovFreeburn          = (lpuPayload[0] != 0);   // +0xAA19
    mbRivalFovRace              = (lpuPayload[1] != 0);   // +0xAA1A
    mbUseTrajectory             = (lpuPayload[2] != 0);   // +0xAA1B
    mbRotateSatNav              = (lpuPayload[3] != 0);   // +0xAA1C
    mbShowOffLineRivalsOnSatNav = (lpuPayload[4] != 0);   // +0xAA1D
}

// @ 0x824FAE60 -- a rival-type icon is "active" unless the mode is road-rage or the
// filter hides rivals.
bool MapIconManager::IsActiveRival(const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon) const
{
    const s8 li8Type = lpIcon->GetIconTypeByte();
    CGS_ASSERT(li8Type == 1 || li8Type == 2 || li8Type == 3,
               "Icon is not even a rival, never mind an active one");   // :3190 (non-gating)

    return mpGuiCache->GetGameMode() != 3
        && meIconFilterMode != E_ICONFILTER_PLAYER_ONLY
        && meIconFilterMode != E_ICONFILTER_NO_RIVALS;
}

// ======================= H3c: the sat-nav icon pass, LANDED =======================
namespace
{
    const f32 KF_PI = 3.1415927f;

    // X360 .rdata @0x8206F838 / @0x8206F808 (read off the image, h3c_dump.txt) -- the
    // DWARF-named lobby-colour -> icon-state tables (BrnMapIconManager.h:318/319).
    const s32 KAE_LOBBY_COLOUR_TO_PLAYER_ICON[12] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
    // (Its rival twin @0x8206F808 = {14..25} belongs to the parked network-rival colour
    // arm -- recorded here, defined when that arm lands, so no unused-table warning.)

    // The inactive-set icon alpha: the X360 computes 128 * (1/255) * 100 per call.
    const f32 KF_INACTIVE_ALPHA = (128.0f * 0.0039215689f) * 100.0f;
}

// @ 0x824F4508 -- the qsort comparator (48-byte records): DESCENDING icon type -- the
// player (type 0) sorts LAST; equal NETWORKRIVAL records order by the player-team byte
// (team 2 after everything else).
int MapIconManager::IconDisplaySort(const void* lpA, const void* lpB)
{
    CGS_ASSERT(lpA != 0, "Invalid element pointer passed");   // :786 (non-gating)
    CGS_ASSERT(lpB != 0, "Invalid element pointer passed");   // :787 (non-gating)

    const GuiEventUpdateSatNav::SatNavIconInfo* lpIconA =
        static_cast<const GuiEventUpdateSatNav::SatNavIconInfo*>(lpA);
    const GuiEventUpdateSatNav::SatNavIconInfo* lpIconB =
        static_cast<const GuiEventUpdateSatNav::SatNavIconInfo*>(lpB);

    if (lpIconA->GetIconTypeByte() != lpIconB->GetIconTypeByte())
        return (lpIconA->GetIconTypeByte() > lpIconB->GetIconTypeByte()) ? -1 : 1;

    if (lpIconA->GetIconTypeByte() != GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
        return 0;

    return (lpIconA->GetPlayerTeamByte() == 2) ? 1 : -1;
}

// @ 0x82502940 -- the small-mode icon alpha by x/z-plane distance from the camera lane
// (the @0x82CDA450 vperm picks {x, z} from BOTH operands; the two function-local statics
// 50.0/100.0 collapse to their constants; 0.00046511628897860646 @0x82074AC4 == 1/2150).
f32 MapIconManager::CalculateAlpha(const Vector4& lv4IconPosition, const Vector4& lv4CameraPosition)
{
    const f32 lfDx = lv4CameraPosition.x - lv4IconPosition.x;
    const f32 lfDz = lv4CameraPosition.z - lv4IconPosition.z;
    const f32 lfDistSq = lfDx * lfDx + lfDz * lfDz;
    const f32 lfDist = (lfDistSq == 0.0f) ? 0.0f : sqrtf(lfDistSq);   // the vsel zero guard

    if (lfDist < 850.0f)
        return 100.0f;
    if (lfDist <= 2150.0f)
        return (50.0f - 100.0f) * ((lfDist - 850.0f) * 0.00046511628897860646f) + 100.0f;
    return 50.0f;
}

// @ 0x824FA320 -- the icon state for a rival / network-rival record.
s32 MapIconManager::GetSatNavIconStateForRival(const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon)
{
    const s8 li8Type = lpIcon->GetIconTypeByte();
    CGS_ASSERT(li8Type == 2 || li8Type == 3, "Unexpected sat nav icon type: ");   // :2248 (non-gating)

    if (li8Type == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :2253 (non-gating)
        const s32 liRaceCarIndex = lpIcon->GetActiveRaceCarIndex();
        CGS_ASSERT(liRaceCarIndex >= 0, "leRivalActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");   // :2256
        CGS_ASSERT(liRaceCarIndex < 8,  "leRivalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT"); // :2257

        // [UI-gate] the network-rival arm: the game-room player-info table walk (cache
        // +0xAD94, stride 0x138) + KAE_LOBBY_COLOUR_TO_RIVAL_ICON via
        // GetOnlinePlayerColourFromARCI -- ONLINE-only surface, unreconstructed. The
        // no-entry fall-through (state 0, invisible) is the X360's own miss path.
        static bool sbLoggedNetworkRivalPark = false;
        if (!sbLoggedNetworkRivalPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedNetworkRivalPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: GetSatNavIconStateForRival network-rival colour arm "
                   "(online game-room tables unreconstructed) -> invisible\n";
        }
        return 0;
    }

    // ---- the plain-rival arm ----
    bool lbFovCull = false;
    if (!mpGuiCache->IsOnlineStartInProgress())   // cache +0x4B4C
    {
        lbFovCull = mpGuiCache->GetInEventColouringGate()   // cache +0x4B4A
                        ? mbRivalFovRace                    // +0xAA1A
                        : mbRivalFovFreeburn;               // +0xAA19
    }

    const s32 liGameMode = mpGuiCache->GetGameMode();
    bool lbVisible = true;
    if (liGameMode == 3
        || (liGameMode == 6
            && mpGuiCache->GetEventPositionOfRaceCar(
                   static_cast<EActiveRaceCarIndex>(lpIcon->GetActiveRaceCarIndex()))
                   > mpGuiCache->GetOpponentsInEvent() + 1))
    {
        lbVisible = false;
    }
    if (!lbVisible)
        return 0;

    s32 liState = MapIconBrnBase::E_ICONSTATE_RIVAL;   // 14
    if (lbFovCull)
    {
        // Beyond 100m (@0x82F27FB0), a rival only stays visible inside the forward
        // 90-degree cone (acos of the heading-dot against half of 90deg-in-radians).
        const Vector4& lv4Icon   = lpIcon->GetPositionLane();
        const Vector4& lv4Player = mPlayerIconInfo.GetPositionLane();
        const f32 lfDx = lv4Icon.x - lv4Player.x;
        const f32 lfDy = lv4Icon.y - lv4Player.y;
        const f32 lfDz = lv4Icon.z - lv4Player.z;
        const f32 lfDistSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
        if (lfDistSq > 100.0f * 100.0f)
        {
            const f32 KF_RIVAL_FOV = 90.0f * 0.017453292f;   // the @0x82FB58E8 guarded static
            const f32 lfHeading = mPlayerIconInfo.GetRotation();
            // RotationY(heading) . (0,0,1) == {sin, 0, cos} (the row-2 pick).
            const f32 lfDirX = sinf(lfHeading);
            const f32 lfDirZ = cosf(lfHeading);
            const f32 lfInvLen = 1.0f / sqrtf(lfDistSq);
            const f32 lfDot = lfDirX * (lfDx * lfInvLen) + lfDirZ * (lfDz * lfInvLen);
            if (lfDot < 0.0f)
                return 0;
            if (acosf(lfDot) > KF_RIVAL_FOV * 0.5f)
                return 0;
        }
    }
    return liState;
}

// @ 0x82502738 -- append the current freeburn-challenge target icon. ONLINE free-burn
// lobby only (the entry assert); the body walks the challenge manager's current action
// (cache +0x406C) -> trigger id -> WorldDataController::GetTriggerVolumeRegion, then
// appends a type-6 record at the region position. [UI-gate] the challenge-manager /
// trigger-region surface is unreconstructed -- parked loudly; unreachable offline.
void MapIconManager::UpdateFreeburnChallengeIcons()
{
    CGS_ASSERT(mpGuiCache->GetGameMode() == E_MODE_ONLINE_FREE_BURN_LOBBY,
               "mpGuiCache->GetGameMode() == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY");   // :683 (non-gating)

    static bool sbLogged = false;
    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
    {
        sbLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] PARK: MapIconManager::UpdateFreeburnChallengeIcons @0x82502738 "
               "(challenge-manager surface unreconstructed; online-lobby only)\n";
    }
}

// @ 0x82511C88 -- refresh the world-derived icons into the used set.
void MapIconManager::UpdateWorldIcons()
{
    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // :478 (non-gating)

    const Vector4& lv4Bounds = MapTransform::GetZoomedWorldRect();   // @0x82FB3440 {minX, maxZ, maxX, minZ}
    const s32 liGameMode = mpGuiCache->GetGameMode();
    const bool lbRoadRageOrMarkedMan = (liGameMode == E_MODE_ROAD_RAGE
                                        || liGameMode == E_MODE_MARKED_MAN);

    if (liGameMode != -1 || mbShowingCrashNavRoute || mbShowingOnlineRoute)
    {
        // [UI-gate] the landmark passes: the crash-nav/online landmark-list walk
        // (WorldDataController landmark table + ShouldDisplayLandmark) and the in-event
        // checkpoint walk (GetCheckpointsInEvent + GetLandmarkInfoFromIndex). Their
        // whole dependency family (the landmark state machine, IsTracked/IsStart/
        // IsFinish/IsPending, GuiTracker's tracked list) rides one parked slice with
        // the case-4 consumer in UpdateSatNavIcons -- no producer, no consumer, both
        // logged. Unreachable in offline freeburn (mode -1, no routes shown).
        static bool sbLoggedLandmarkPark = false;
        if (!sbLoggedLandmarkPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedLandmarkPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: MapIconManager::UpdateWorldIcons landmark/checkpoint "
                   "passes (landmark slice unreconstructed)\n";
        }
    }

    // ---- the drive-through pass: in-view appends + the nearest volunteers ----
    s32 liNearestJunkyard = -1;
    s32 liNearestBodyShop = -1;
    f32 lfBestJunkyardDistSq = 3.4028235e38f;
    f32 lfBestBodyShopDistSq = 3.4028235e38f;

    if (!mbShowingOnlineRoute && !mbShowingPreRaceRoute && mbShowingDriveThrus)
    {
        const s32 liNumDriveThroughs = mpGuiCache->GetNumberOfDriveThroughs();
        const Vector4& lv4Player = mPlayerIconInfo.GetPositionLane();   // +0x960

        for (s32 liDriveThrough = 0; liDriveThrough < liNumDriveThroughs; ++liDriveThrough)
        {
            if (miNumUsedIcons >= miMaxNumberIcons)
                break;

            const SatNavIconInfo* lpDriveThrough = mpGuiCache->GetDriveThrough(liDriveThrough);
            if (lpDriveThrough->IsHiddenDriveThru())
                continue;
            const s8 li8Type = lpDriveThrough->GetIconTypeByte();
            if (lbRoadRageOrMarkedMan
                && li8Type == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PAINT_SHOP)
                continue;

            const Vector4& lv4Pos = lpDriveThrough->GetPositionLane();
            const bool lbInView = (lv4Pos.x >= lv4Bounds.x && lv4Bounds.z >= lv4Pos.x &&
                                   lv4Bounds.y >= lv4Pos.z && lv4Pos.z >= lv4Bounds.w);
            if (lbInView)
            {
                if (li8Type == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD)   // 7
                {
                    lfBestJunkyardDistSq = 0.0f;
                    liNearestJunkyard = -1;
                    // The junkyard only appends in freeburn (-1) / online lobby (15).
                    if (!(liGameMode == -1 || liGameMode == E_MODE_ONLINE_FREE_BURN_LOBBY))
                        continue;
                }
                else if (li8Type == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_BODYSHOP) // 9
                {
                    lfBestBodyShopDistSq = 0.0f;
                    liNearestBodyShop = -1;
                }
                mSatNavIconInfo[miNumUsedIcons] = *lpDriveThrough;
                ++miNumUsedIcons;
            }
            else if (!mpGuiCache->GetInEventColouringGate()
                     && mpGuiCache->IsCarUnlockPending()
                     && li8Type == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNKYARD)
            {
                const f32 lfDx = lv4Pos.x - lv4Player.x;
                const f32 lfDy = lv4Pos.y - lv4Player.y;
                const f32 lfDz = lv4Pos.z - lv4Player.z;
                const f32 lfDistSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
                if (lfDistSq < lfBestJunkyardDistSq)
                {
                    lfBestJunkyardDistSq = lfDistSq;
                    liNearestJunkyard = liDriveThrough;
                }
            }
            else if (li8Type == GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_BODYSHOP)
            {
                const bool lbWantNearestBodyShop =
                    (mpGuiCache->GetShowNearestBodyShop()
                     && (liGameMode == -1 || liGameMode == E_MODE_ONLINE_FREE_BURN_LOBBY))
                    || lbRoadRageOrMarkedMan;
                if (lbWantNearestBodyShop)
                {
                    const f32 lfDx = lv4Pos.x - lv4Player.x;
                    const f32 lfDy = lv4Pos.y - lv4Player.y;
                    const f32 lfDz = lv4Pos.z - lv4Player.z;
                    const f32 lfDistSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
                    if (lfDistSq < lfBestBodyShopDistSq)
                    {
                        lfBestBodyShopDistSq = lfDistSq;
                        liNearestBodyShop = liDriveThrough;
                    }
                }
            }
        }

        // The nearest volunteers append at the walk's end (the X360 LABEL_76 block runs
        // from BOTH exits -- loop end and the max-icons break -- without re-checking the
        // cap; the record array holds 50 against a 16 cap, so the append is bounded).
        if (liNearestJunkyard != -1)
        {
            mSatNavIconInfo[miNumUsedIcons] = *mpGuiCache->GetDriveThrough(liNearestJunkyard);
            ++miNumUsedIcons;
        }
        if (liNearestBodyShop != -1)
        {
            mSatNavIconInfo[miNumUsedIcons] = *mpGuiCache->GetDriveThrough(liNearestBodyShop);
            ++miNumUsedIcons;
        }
    }
}

// @ 0x82522588 -- the sat-nav pass: the road-sign / freeburn-challenge / world-icon
// refreshes, AddTeamToNetworkRivals, the IconDisplaySort qsort, then the per-record
// state machine driving the 16 apt SatNavMapIcon components, and the invisible tail
// over the unused components. [H3c LANDED -- the named parks inside are the online /
// crash-route / landmark / LARGE-map arms, each one-shot logged.]
void MapIconManager::UpdateSatNavIcons()
{
    typedef GuiEventUpdateSatNav::SatNavIconInfo SatNavIconInfo;

    // [DIAG] NOT IN THE X360 BINARY -- see the per-icon print below.
    static s32 siDiagTick = 0;
    const bool gbSatNavIconDiag = ((siDiagTick++ % 300) == 0);

    s32 liExtraIcons = 0;   // X360 v96 -- the route start-point icon reserves pool slot 0

    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // :1533 (non-gating)

    if (mbUseRoadSigns)                                   // +0xA1B0
        mRoadSignIconManager.Update();
    if (mpGuiCache->GetGameMode() == E_MODE_ONLINE_FREE_BURN_LOBBY)   // 15
        UpdateFreeburnChallengeIcons();
    UpdateWorldIcons();

    CGS_ASSERT(miNumUsedIcons >= 0, "miNumUsedIcons >= 0");                       // :1552 (non-gating)
    CGS_ASSERT(miNumUsedIcons <= KI_SATNAV_MAX_ICONS,
               "miNumUsedIcons <= KI_SATNAV_MAX_ICONS");                          // :1553 (non-gating)
    CGS_ASSERT(miMaxNumberIcons >= 0, "Max icons is negative");                   // :1554 (non-gating)

    const s32 liNumIcons = (miNumUsedIcons >= miMaxNumberIcons) ? miMaxNumberIcons
                                                                : miNumUsedIcons;   // X360 v95
    if (gbSatNavIconDiag && CgsDev::Log::gpDebugPrint != 0)
        *CgsDev::Log::gpDebugPrint
            << "[satnav-icon] tick: used=" << miNumUsedIcons << " max=" << miMaxNumberIcons
            << " draw=" << liNumIcons << " sizeMode=" << static_cast<s32>(meIconSizeMode)
            << " visible=" << (mbIconsVisible ? 1 : 0) << "\n";
    AddTeamToNetworkRivals();
    qsort(mSatNavIconInfo, static_cast<size_t>(liNumIcons), sizeof(SatNavIconInfo),
          IconDisplaySort);

    // ---- the crash-nav / online route START-POINT icon (pool slot 0) ----
    const bool lbLightTriggerValid =
        !(((mSelectedLightTriggerID & 0xFFFF00u) == 0xFFFF00u)
          || ((mSelectedLightTriggerID & 0xFFu) == 0xFFu));
    if (mbShowingOnlineRoute && lbLightTriggerValid && miSelectedCheckpoint > 0)
    {
        // [UI-gate] the event-start lookup by light-trigger id (the cache's +0x5690
        // CgsArray) is unreconstructed -- online-route screens only. Parked loudly;
        // liExtraIcons stays 0 (the slot is not reserved).
        static bool sbLoggedStartPark = false;
        if (!sbLoggedStartPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedStartPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: UpdateSatNavIcons online-route start-point icon "
                   "(event-start array unreconstructed)\n";
        }
    }
    else if ((mbShowingPreRaceRoute || mbShowingCrashNavRoute) && muSelectedJunctionID != 0)
    {
        // [UI-gate] the junction start-point lookup -- crash-nav/pre-race route screens
        // only; same parked slice as above.
        static bool sbLoggedJunctionPark = false;
        if (!sbLoggedJunctionPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedJunctionPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: UpdateSatNavIcons junction start-point icon "
                   "(junction lookup unreconstructed)\n";
        }
    }

    // v124: the camera lane the alpha fade measures against (perm {x,z} @0x82CDA450 --
    // CalculateAlpha takes the raw lane and picks x/z itself here).
    const Vector4& lv4Camera = mpGuiCache->GetWorldCameraPosition();   // cache +0x4AE0

    s32 liIcon = 0;
    for (; liIcon < liNumIcons; ++liIcon)
    {
        CGS_ASSERT(liIcon + liExtraIcons < miMaxNumberIcons,
                   "Run out of icons to show");   // :1627 (non-gating)

        s32 liState = MapIconBrnBase::E_ICONSTATE_INVISIBLE;   // 0
        bool lbNearEventIcon = false;                          // X360 v22
        f32 lfStackOffsetY = 0.0f;   // X360 v126 lane 1 -- only the (parked) case-4
                                     // checkpoint-stacking arms raise it; kept so the
                                     // commit tail matches the X360 shape.

        SatNavMapIcon& lrIcon = mSatNavMapIcons[liIcon + liExtraIcons].mIcon;
        const SatNavIconInfo& lrRecord = mSatNavIconInfo[liIcon];

        const s8 li8IconType = lrRecord.GetIconTypeByte();
        CGS_ASSERT(li8IconType >= 0, "leIconType >= 0");                       // BrnGuiEventTypeDefs.h:1911
        CGS_ASSERT(li8IconType < SatNavIconInfo::E_SATNAVICON_MAX,
                   "leIconType < E_SATNAVICON_MAX");                           // BrnGuiEventTypeDefs.h:1912

        switch (li8IconType)
        {
        case SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:   // 0 -- THE ARROW
        {
            switch (mpGuiCache->GetGameMode())
            {
            case 10: case 12: case 14: case 15: case 17:
            {
                // [UI-gate] the online lobby-colour lookup (GetOnlinePlayerColourFromARCI
                // + KAE_LOBBY_COLOUR_TO_PLAYER_ICON) is unreconstructed -- online modes
                // only. Colour 0's mapping is the arm's fallback, logged once.
                static bool sbLoggedColourPark = false;
                if (!sbLoggedColourPark && CgsDev::Log::gpDebugPrint != 0)
                {
                    sbLoggedColourPark = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[UI-gate] PARK: UpdateSatNavIcons online player-colour arm "
                           "(GetOnlinePlayerColourFromARCI unreconstructed) -> colour 0\n";
                }
                liState = KAE_LOBBY_COLOUR_TO_PLAYER_ICON[0];
                break;
            }
            case 11: case 13: case 16:
                liState = MapIconBrnBase::E_ICONSTATE_PLAYER_ONLINE;    // 2
                break;
            default:
                liState = MapIconBrnBase::E_ICONSTATE_PLAYER_OFFLINE;   // 1 "Offline_Player_Yellow"
                break;
            }

            lrIcon.SetRotation(mbRotateSatNav ? 0.0f                                  // +0xAA1C
                                              : (KF_PI - lrRecord.GetRotation()));    // pi - rot

            // The LARGE-map event-icon proximity fade (freeburn big map only).
            if (meIconSizeMode == E_ICONSIZE_LARGE && mpGuiCache->GetGameMode() == -1)
            {
                Vector3 lv3Camera;
                lv3Camera.x = lv4Camera.x; lv3Camera.y = lv4Camera.y;
                lv3Camera.z = lv4Camera.z; lv3Camera.w = lv4Camera.w;
                const Vector2 lv2Player = MapTransform::WorldToDevice(lv3Camera, false);
                Vector2 lav2EventIcons[EventIconManager::KI_MAX_2DEVENTICONS];
                s32 liNumEventIcons = 0;
                mEventIconManager.GetEventIconPositions(lav2EventIcons, &liNumEventIcons);
                for (s32 liEvent = 0; liEvent < liNumEventIcons; ++liEvent)
                {
                    const f32 lfDx = lv2Player.x - lav2EventIcons[liEvent].x;
                    const f32 lfDy = lv2Player.y - lav2EventIcons[liEvent].y;
                    if (900.0f > lfDx * lfDx + lfDy * lfDy)
                    {
                        lrIcon.SetAlpha(50.0f);
                        lbNearEventIcon = true;
                        break;
                    }
                }
            }
            break;
        }

        case SatNavIconInfo::E_SATNAVICON_MARKED_MAN:   // 1
        {
            liState = MapIconBrnBase::E_ICONSTATE_RIVAL;   // 14
            if (mbRotateSatNav)
            {
                const GuiPlayerInfo* lpPlayerInfo = reinterpret_cast<const GuiPlayerInfo*>(
                    &mpGuiCache->GetWorldCameraPosition());
                CGS_ASSERT(lpPlayerInfo != 0, "lpPlayerInfo");   // :1776 (non-gating)
                lrIcon.SetRotation(lpPlayerInfo->mfOrientation - lrRecord.GetRotation());
            }
            else
            {
                lrIcon.SetRotation(KF_PI - lrRecord.GetRotation());
            }
            break;
        }

        case SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL:   // 2
        case SatNavIconInfo::E_SATNAVICON_RIVAL:          // 3
        {
            if (li8IconType == SatNavIconInfo::E_SATNAVICON_RIVAL
                && !mbShowOffLineRivalsOnSatNav                     // +0xAA1D
                && mpGuiCache->GetGameMode() == -1)
                break;   // offline freeburn hides plain rivals unless the 200-event flag is up

            if (!(mpGuiCache->GetGameMode() == 13
                  && mpGuiCache->GetCurrentOnlinePlayerTeam(
                         static_cast<EActiveRaceCarIndex>(
                             mpGuiCache->GetPlayerActiveRaceCarIndex())) == 2))
            {
                if (meIconSizeMode != E_ICONSIZE_SMALL)
                {
                    // [UI-gate] the LARGE-map rival arm: GetCrashNavIconStateForRival +
                    // the "CAR_%s" SetIconText naming -- big-map screens only, rides the
                    // crash-nav slice. Rotation still lands (the X360 sets it before the
                    // state); state stays invisible until the arm lands.
                    static bool sbLoggedLargeRivalPark = false;
                    if (!sbLoggedLargeRivalPark && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLoggedLargeRivalPark = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[UI-gate] PARK: UpdateSatNavIcons LARGE-map rival arm "
                               "(GetCrashNavIconStateForRival unreconstructed)\n";
                    }
                    lrIcon.SetRotation(KF_PI - lrRecord.GetRotation());
                }
                else if (mbRotateSatNav)
                {
                    const GuiPlayerInfo* lpPlayerInfo = reinterpret_cast<const GuiPlayerInfo*>(
                        &mpGuiCache->GetWorldCameraPosition());
                    CGS_ASSERT(lpPlayerInfo != 0, "lpPlayerInfo");   // :1739 (non-gating)
                    liState = GetSatNavIconStateForRival(&lrRecord);
                    lrIcon.SetRotation(lpPlayerInfo->mfOrientation - lrRecord.GetRotation());
                }
                else
                {
                    liState = GetSatNavIconStateForRival(&lrRecord);
                    lrIcon.SetRotation(KF_PI - lrRecord.GetRotation());
                }
            }
            break;
        }

        case SatNavIconInfo::E_SATNAVICON_LANDMARK:   // 4
        {
            // [UI-gate] the landmark state machine (tracked/start/finish/pending +
            // the checkpoint-stacking offsets) rides the parked landmark slice with
            // its producers in UpdateWorldIcons -- no records of this type can enter
            // the used set until that slice lands, so this arm is a coherent park.
            static bool sbLoggedLandmarkArmPark = false;
            if (!sbLoggedLandmarkArmPark && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedLandmarkArmPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] PARK: UpdateSatNavIcons landmark arm (case 4) "
                       "(landmark slice unreconstructed)\n";
            }
            break;
        }

        case SatNavIconInfo::E_SATNAVICON_FREEBURN_CHALLENGE:   // 6
            liState = MapIconBrnBase::E_ICONSTATE_SATNAV_FREEBURN_CHALLENGE;   // 40
            lrIcon.SetRotation(0.0f);
            break;

        case SatNavIconInfo::E_SATNAVICON_JUNKYARD:   // 7
            liState = (meIconSizeMode == E_ICONSIZE_SMALL)
                          ? MapIconBrnBase::E_ICONSTATE_SATNAV_JUNKYARD          // 41
                          : MapIconBrnBase::E_ICONSTATE_CRASHNAV_JUNKYARD;       // 36
            lrIcon.SetRotation(0.0f);
            break;

        case SatNavIconInfo::E_SATNAVICON_CAR_PARK:   // 8
            if (meIconSizeMode == E_ICONSIZE_SMALL)
                liState = MapIconBrnBase::E_ICONSTATE_SATNAV_CAR_PARK;           // 42
            lrIcon.SetRotation(0.0f);
            break;

        case SatNavIconInfo::E_SATNAVICON_BODYSHOP:   // 9
            liState = (meIconSizeMode == E_ICONSIZE_SMALL)
                          ? MapIconBrnBase::E_ICONSTATE_SATNAV_BODYSHOP          // 43
                          : MapIconBrnBase::E_ICONSTATE_CRASHNAV_BODYSHOP;       // 37
            lrIcon.SetRotation(0.0f);
            break;

        case SatNavIconInfo::E_SATNAVICON_GAS_STATION:   // 10
            liState = (meIconSizeMode == E_ICONSIZE_SMALL)
                          ? MapIconBrnBase::E_ICONSTATE_SATNAV_GAS_STATION       // 44
                          : MapIconBrnBase::E_ICONSTATE_CRASHNAV_GAS_STATION;    // 38
            lrIcon.SetRotation(0.0f);
            break;

        case SatNavIconInfo::E_SATNAVICON_PAINT_SHOP:   // 11
            liState = (meIconSizeMode == E_ICONSIZE_SMALL)
                          ? MapIconBrnBase::E_ICONSTATE_SATNAV_PAINT_SHOP        // 45
                          : MapIconBrnBase::E_ICONSTATE_CRASHNAV_PAINT_SHOP;     // 39
            lrIcon.SetRotation(0.0f);
            break;

        default:
            break;   // 5 junction / 12 tire shop / 13 road sign: no state change
        }

        // ---- the shared alpha + commit tail (X360 LABEL_150/152/159) ----
        const bool lbForcedInvisible = !mbIconsVisible;         // +0xAA1E
        if (lbForcedInvisible)
            liState = MapIconBrnBase::E_ICONSTATE_INVISIBLE;
        if (!lbForcedInvisible
            && liState != MapIconBrnBase::E_ICONSTATE_INVISIBLE
            && meIconSizeMode == E_ICONSIZE_SMALL)
        {
            lrIcon.SetAlpha(CalculateAlpha(lrRecord.GetPositionLane(), lv4Camera));
        }
        else
        {
            if (mbIsActive)                                     // +0xAA22
            {
                if (!lbNearEventIcon)
                    lrIcon.SetAlpha(100.0f);
            }
            else
            {
                lrIcon.SetAlpha(KF_INACTIVE_ALPHA);             // 128/255 * 100
            }
        }

        {
            Vector3 lv3Record;
            const Vector4& lv4Pos = lrRecord.GetPositionLane();
            lv3Record.x = lv4Pos.x; lv3Record.y = lv4Pos.y;
            lv3Record.z = lv4Pos.z; lv3Record.w = lv4Pos.w;
            Vector2 lv2Device = MapTransform::WorldToDevice(lv3Record, true);
            lv2Device.y += lfStackOffsetY;   // the (parked) checkpoint-stacking lift
            lrIcon.SetPosition(lv2Device);
        }
        lrIcon.SetState(static_cast<MapIconBrnBase::IconState>(liState));
        // [DIAG] NOT IN THE X360 BINARY -- H3c arrow proof: what state + device position
        // each pool component actually receives. Once per 300 manager ticks, all icons.
        if (gbSatNavIconDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            const Vector2 lv2Now = lrIcon.GetPosition();
            *CgsDev::Log::gpDebugPrint
                << "[satnav-icon] slot " << (liIcon + liExtraIcons)
                << " type " << static_cast<s32>(li8IconType)
                << " state " << liState
                << " dev(" << lv2Now.x << "," << lv2Now.y << ")\n";
        }
        lrIcon.Update();   // the X360 slot-+0x20 commit -- a folded-empty no-op for sat-nav icons
    }

    // ---- the online START-POINT icon at the route head (checkpoint == 0) ----
    s32 liUsedComponents = liIcon + liExtraIcons;
    if (mbShowingOnlineRoute && lbLightTriggerValid && miSelectedCheckpoint == 0)
    {
        // [UI-gate] same parked event-start lookup as the head block; the component
        // it would drive is left to the invisible tail below.
        static bool sbLoggedEndStartPark = false;
        if (!sbLoggedEndStartPark && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLoggedEndStartPark = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: UpdateSatNavIcons online-route tail start-point icon "
                   "(event-start array unreconstructed)\n";
        }
    }

    // ---- hide every unused pool component (X360 LABEL_172 tail) ----
    for (s32 liPool = liUsedComponents; liPool < miMaxNumberIcons; ++liPool)
    {
        SatNavMapIcon& lrPoolIcon = mSatNavMapIcons[liPool].mIcon;
        if (lrPoolIcon.GetState() != MapIconBrnBase::E_ICONSTATE_INVISIBLE)
        {
            lrPoolIcon.SetState(MapIconBrnBase::E_ICONSTATE_INVISIBLE);
            lrPoolIcon.Update();
        }
    }
}

// @ 0x825212C0 -- the crash-nav pass (the 50-icon pool + landmark state machines).
void MapIconManager::UpdateCrashNavIcons()
{
    static bool sbLogged = false;
    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
    {
        sbLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] PARK: MapIconManager::UpdateCrashNavIcons @0x825212C0 (the apt "
               "icon-pool drive) is unreconstructed\n";
    }
}

// (Inline-folded on the X360; the header's SetZoomFactor note.) Feed the map's
// zoom-derived icon scale into the embedded road-sign manager.
void MapIconManager::SetZoomFactor(f32 lfZoomFactor)
{
    mRoadSignIconManager.SetZoomFactor(lfZoomFactor);
}

}
