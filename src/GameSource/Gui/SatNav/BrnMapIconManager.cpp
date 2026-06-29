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
// All branch conditions and the compared constants come from the X360 asm/pseudocode; the
// game-mode and icon-type literals are resolved to their canonical enumerators (DecFIGS
// DWARF): EGameModeType (BrnGameStateSharedIO.h) and SatNavIconType (BrnGuiEventTypeDefs.h).

#include "GameSource/Gui/SatNav/BrnMapIconManager.h"
#include "GameSource/Gui/BrnGuiCache.h"               // BrnGui::GuiCache accessors (drive-throughs / team / mode)
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::EGameModeType
#include "GameSource/BurnoutConstants.h"               // EActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Message::gxMessageFilterFlags / CgsDev::Log::gpDebugPrint

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

}
