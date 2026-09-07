// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_SendSetUpAllDriveThrus.cpp
//
// ⭐⭐ [minimap blips, issue #9, 2026-09-07] THE DRIVE-THRU ICON TABLE PRODUCER.
// BrnGameState::GameStateModule::SendSetUpAllDriveThrusMessage.
//
// The first link of the chain that puts the gas-station / body-shop / paint-shop / junkyard /
// car-park icons on the sat-nav minimap and the big map:
//
//     THIS  (action 45, SetUpAllDriveThrusAction, 1112 bytes, once per dirty raise)
//       -> BrnGameModule::TranslateGameActionsToGuiEvents, the case-45 arm
//          (one GuiEventUpdateSatNav record per drive-thru, icon types 7..12, posted at the tail)
//       -> GuiCache::RecEvent case 199, the drive-thru arm (maDriveThroughInfo, dedupe by CgsID,
//          the 15-entry canned-position override)
//       -> MapIconManager::UpdateWorldIcons (the in-view append + the nearest junkyard /
//          body-shop volunteers) -> the 16 apt SatNavMapIcon components.
//
// Until it landed NOTHING on this build emitted a drive-thru icon record: the world bridge's
// per-frame 199 post carries cars only, so the cache's table stayed empty and the minimap drew
// the map and the player arrow and nothing else -- the screenshot in issue #9.
//
// WHAT THE CONSOLE DOES, leg by leg:
//   * walks the trigger data's generic-region table (count at +0x48, entries at +0x44, 56-byte
//     stride) and keeps sub-types 3 / 2 / 0 / 1 / 4 -- exactly GenericRegion::IsDriveThru(), the
//     same five-way test it inlines into DriveThruManager::Prepare, so it is called by name;
//   * asks the profile (this +0xBCA0 == mProgressionManager.mProfile) IsDriveThruDiscoverd(id,
//     type) with the region's id (the +0x24 word, sign-extended into the CgsID) and its type byte
//     (+0x36), and skips everything the player has not found yet;
//   * builds the 24-byte DriveThruInfo {x = region +0, z = region +8, id, type} and appends it,
//     asserting "Too many drive thrus in Generic Region list" past the 46th;
//   * posts the whole action (id 45, 1112 bytes) onto the output action queue.
//
// ⓘ ONLY DISCOVERED drive-thrus are published -- that is the shipped behaviour, not a gap. A fresh
//   profile starts with the starting body shop only (UnlockToProgressionRank registers it) and
//   gains blips as the player finds shops; the re-publish is driven by the dirty byte OnDriveThru
//   raises on every discovery.
// ⓘ The record's position is (x, z) only -- DriveThruInfo has no y; the bridge's case-45 arm
//   rebuilds the icon lane as {x, 0, z, 0}. The region position is the BoxRegion's own (the first
//   member of the region), i.e. GetBoxRegion()->GetPosition().
//
// ⚠️ [FLAG PC bring-up] ONE NULL GUARD the console does not have: the trigger data is reached
//   through TriggerQueryManager::GetTriggerData() (the same acquire SendSetupPlayerCarEvent uses;
//   the console dereferences its resource pointer raw), and on PC it can still be unbound on the
//   first PreWorldUpdate of a boot -- by which time the caller has already cleared the dirty
//   byte. So a null here RE-RAISES the byte (SetDriveThrusDirtyFlag) and skips the post; the next
//   frame retries. One-shot logged. DELETE-WHEN the module's resource binding precedes its first
//   PreWorldUpdate the way the console's does.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"        // GameActionQueue / E_ACTION_SET_UP_ALL_DRIVE_THRUS
#include "GameSource/GameState/BrnGameActions.h"              // SetUpAllDriveThrusAction
#include "GameSource/GameState/Progression/BrnProfile.h"      // Profile::IsDriveThruDiscoverd
#include "SharedClasses/Trigger/BrnTriggerData.h"             // TriggerData::GetGenericRegion / count
#include "SharedClasses/Trigger/BrnGenericRegion.h"           // GenericRegion::IsDriveThru / GetType
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // [DIAG] the publish lines

namespace BrnGameState
{

void GameStateModule::SendSetUpAllDriveThrusMessage(GameStateModuleIO::GameActionQueue* lpOutputActionQueue)
{
    // (the console brackets the body in a CPU perf-monitor start/stop pair on a member this
    //  build does not model)
    CGS_ASSERT(lpOutputActionQueue != 0, "lpOutputActionQueue");   // the console's own, non-gating
    if (lpOutputActionQueue == 0)
    {
        return;
    }

    const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
    if (lpTriggerData == 0)
    {
        // [FLAG PC bring-up] see the banner -- re-arm and retry next frame.
        mProgressionManager.SetDriveThrusDirtyFlag();
        static bool sbLogged = false;
        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[drivethru] SETUP deferred: trigger data not bound yet (dirty byte re-raised)\n";
        }
        return;
    }

    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != 0, "lpProfile");
    if (lpProfile == 0)
    {
        return;
    }

    // The action's Array<> count word is the one thing the console initialises in its frame local.
    GameStateModuleIO::SetUpAllDriveThrusAction lAction;
    lAction.maDriveThrus.Clear();

    s32       liNumDriveThrus = 0;
    const s32 liRegionCount   = lpTriggerData->GetGenericRegionCount();

    for (s32 liRegion = 0; liRegion < liRegionCount; ++liRegion)
    {
        // (the "liGenericRegionIndex < miGenericRegionCount" range assert rides GetGenericRegion)
        const BrnTrigger::GenericRegion* lpRegion = lpTriggerData->GetGenericRegion(liRegion);

        // The five-way sub-type keep test.
        if (!lpRegion->IsDriveThru())
        {
            continue;
        }

        const CgsID                            lDriveThruId = lpRegion->GetId();
        const BrnTrigger::GenericRegion::Type  leType       = lpRegion->GetType();
        if (!lpProfile->IsDriveThruDiscoverd(lDriveThruId, leType))
        {
            continue;
        }

        // {x, z, id, type} -> DriveThruInfo.
        const Vector3 lv3Position = lpRegion->GetBoxRegion()->GetPosition();

        GameStateModuleIO::SetUpAllDriveThrusAction::DriveThruInfo lInfo;
        lInfo.mfXCoord     = lv3Position.x;
        lInfo.mfZCoord     = lv3Position.z;
        lInfo.mDriveThruId = lDriveThruId;
        lInfo.meType       = leType;
        lAction.maDriveThrus.Append(lInfo);

        ++liNumDriveThrus;
        CGS_ASSERT(liNumDriveThrus <= 46,
                   "SendSetUpAllDriveThrusMessage: Too many drive thrus in Generic Region list");   // non-gating

        // [DIAG] NOT IN THE X360 BINARY -- the per-record witness the minimap_drivethru_blips
        // case pairs against the icon pass (world x/z straight off the region table; the icon
        // side prints what it drew). Bounded by the 46-record table, fires only on a publish.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[drivethru] SETUP rec id=" << static_cast<u32>(lDriveThruId)
                << " type=" << static_cast<s32>(leType)
                << " pos=(" << lv3Position.x << "," << lv3Position.z << ")\n";
        }
    }

    // The console posts 1112 bytes; the host record is the same size (46 * 24 + the count word +
    // pad), and it goes out by sizeof so producer advance == consumer stride.
    lpOutputActionQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lAction),
        GameStateModuleIO::E_ACTION_SET_UP_ALL_DRIVE_THRUS,
        static_cast<s32>(sizeof(lAction)));

    // [DIAG] NOT IN THE X360 BINARY -- the publish line (once per dirty raise: boot, discovery,
    // junkyard exit). Same `[drivethru]` tag as the manager's ladder so one grep reads it all.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[drivethru] SETUP: posted action 45 with " << liNumDriveThrus
            << " discovered drive-thru(s) of " << liRegionCount << " generic regions\n";
    }
}

} // namespace BrnGameState
