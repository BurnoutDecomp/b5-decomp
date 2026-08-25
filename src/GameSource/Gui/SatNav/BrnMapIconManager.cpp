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
// NAMED GATE (this slice's honest boundary): UpdateSatNavIcons @0x82522588 and
// UpdateCrashNavIcons @0x825212C0 drive the 50/16-element APT icon pools plus the
// road-sign / event-icon / world-icon passes -- none of those pools is reconstructed
// yet, so both legs are one-shot-logged parks, NOT silent stubs. The sat-nav MINIMAP
// (map/mask/player arrow) renders through SatNavRenderer and does not pass through
// these legs; only the on-map apt icons wait on them.
//
// All branch conditions and the compared constants come from the X360 asm/pseudocode; the
// game-mode and icon-type literals are resolved to their canonical enumerators (DecFIGS
// DWARF): EGameModeType (BrnGameStateSharedIO.h) and SatNavIconType (BrnGuiEventTypeDefs.h).

#include "GameSource/Gui/SatNav/BrnMapIconManager.h"
#include "GameSource/Gui/BrnGuiCache.h"               // BrnGui::GuiCache accessors (drive-throughs / team / mode)
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameSource/BurnoutConstants.h"               // EActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Message::gxMessageFilterFlags / CgsDev::Log::gpDebugPrint
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // MapTransform::GetZoomedWorldRect (the icon bounds test)

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
            // [UI-gate] the per-icon component pass: SPrintf "%s%d" names, the
            // crash-nav component Construct + state reset per icon, and (icons 0..15)
            // the sat-nav FlaptIconComponent Construct + SatNavMapIcon::Prepare +
            // GotoAndStopLabel(0). The pools are not reconstructed -- parked loudly.
            static bool sbLoggedIconPark = false;
            if (!sbLoggedIconPark && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedIconPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] PARK: MapIconManager::SetOwnerParameters icon-component "
                       "pass skipped (apt icon pools unreconstructed; owner "
                    << static_cast<s32>(leOwnerId) << ", " << liMaxNumberIcons << " icons)\n";
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

// @ 0x825023D0 -- refresh the per-icon sat-nav info set from the incoming id-199 event:
// route-frozen gate, then per icon -- the player record is latched aside, marked-man /
// network-rival records need the LARGE icon mode, plain rivals pass the mode/finished/
// active filter -- and every accepted record inside the current zoomed world window is
// copied into the used set.
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
        bool lbWanted = true;

        switch (lrIcon.GetIconTypeByte())
        {
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:   // 0
            mPlayerIconInfo = lrIcon;   // the +0x960 aside copy
            lbWanted = false;
            break;

        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_MARKED_MAN:   // 1
        case GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL: // 2
            if (meIconSizeMode == E_ICONSIZE_SMALL)   // +0xAA04 == 0
                lbWanted = false;
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
                lbWanted = true;
            }
            else
            {
                lbWanted = (liGameMode != 4);
            }
            break;
        }

        default:
            break;   // every other type passes straight to the bounds test
        }

        if (!lbWanted)
            continue;

        // The zoomed-window bounds test (the X360 lane picks over @0x82FB3440:
        // {minX, maxZ, maxX, minZ} in the default pi-rotated frame).
        const Vector4& lv4Pos = lrIcon.GetPositionLane();
        if (lv4Bounds.x > lv4Pos.x || lv4Pos.x > lv4Bounds.z ||
            lv4Pos.z > lv4Bounds.y || lv4Bounds.w > lv4Pos.z)
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

// ---- the two per-owner icon passes: NAMED GATES (see the banner) --------------------
// @ 0x82522588 -- the sat-nav pass: RoadSignIconManager::Update, the freeburn-challenge
// + world-icon refreshes, AddTeamToNetworkRivals, an IconDisplaySort qsort, the rival /
// tracked / drive-through state machines and the 16 apt SatNavMapIcon component drives.
// Everything after the sort operates on the unreconstructed apt icon pools.
void MapIconManager::UpdateSatNavIcons()
{
    static bool sbLogged = false;
    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
    {
        sbLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] PARK: MapIconManager::UpdateSatNavIcons @0x82522588 (the apt "
               "icon-pool drive) is unreconstructed; the minimap map/mask/arrow render "
               "through SatNavRenderer and do not need it\n";
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
