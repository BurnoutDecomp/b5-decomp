// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_SendSpecificPreSetRacesModes.cpp
//
// THE MAP MENU PRESET-RACES PRODUCER --
//     BrnGameState::GameStateModule::SendSpecificPreSetRacesModesAction
//
// This is the ONE function in the shipped image that ever reaches
// SpecificGameModeEventInterface::AddEvent, i.e. the ONE thing that ever puts a record in a
// preset-event table -- the GameState-side one it builds here, and, one copy hop later, the
// GUI cache's own. Until it landed that table was permanently empty on this build:
// GuiCache::HandleSpecificPreSetRacesEvent never ran, the cache's online-finish-point bitmask
// stayed zero, and the map menu's "which junctions can I start THIS event type from" list was
// empty on every boot.
//
// THE CHAIN, end to end:
//   GameStateModule::ProcessGameEvents, game-event 28 (the GUI preset-races request)
//       -> liEventType = the request payload's first word, lpOutput = the output buffer
//       -> SendSpecificPreSetRacesModesAction  (this function)
//       -> OutputBuffer::SetSpecificGameModeEventInterfaceIsValid(true)
//   BrnGameModule::BridgeGameStateToGui copies the whole interface out of the buffer and
//       queues it as GuiEventSpecificPresetRaces
//   GuiCache::RecEvent -> GuiCache::HandleSpecificPreSetRacesEvent
//
// THE GUI EVENT ID IS 194. Recorded here because the previous wave wrote 190 into
// GuiCache::RecEvent's arm for it. The queue-side event id is baked into the
// AddGuiEvent<GuiEventSpecificPresetRaces> instantiation and it is 194; 190 is the id of a
// different, 8-byte record (GuiOverlayShowingNotification, whose own instantiation bakes it).
// See this lane's report for the evidence and the consequence.
//
// WHAT IT DOES, leg by leg:
//   * ProgressionManager::GetProgressionData() -- and note it is the NULL-TOLERANT accessor:
//     the shipped code tests the resource slot's raw word first and yields NULL rather than
//     firing the resource container's own assert.
//   * TriggerQueryManager::GetTrafficData() and ::GetTriggerData().
//   * the three "lp<Resource>" asserts on those three answers (cpp:5992/5993/5994).
//   * the local interface is Construct'd (count sentinel -> 0) before the first AddEvent.
//   then, for every LIGHT TRIGGER of every HULL of the loaded TrafficData:
//     * id = LightTriggerId::Set(hull, triggerIndex), inlined, with its two baked bounds
//       asserts (BrnTrafficLightTrigger.h:211/:212)
//     * box = TrafficData::GetJunctionLogicBoxForTrafficLight(id)   [assert cpp:6011]
//     * eventJunctionId = box->GetEventJunctionID() -- KEEP ONLY junctions that carry one. The
//       test is SIGNED, so the -1 "this junction has no event" sentinel and every other
//       negative word drop out together.
//     * the open-coded EventJunction scan of ProgressionData's junction table for that id
//       (the same scan three other bodies in the tree carry), then:
//           liEventType <  10 -> junction->GetOfflineEvent(), and GetEvent(event->GetMode())
//                                (see THE OVERRIDE WORD below)
//           liEventType >= 10 -> junction->GetOnlineEvent(),  and
//                                GetOnlin(event->GetOnlineMode())
//     * KEEP ONLY the junctions whose mapped id == liEventType.
//     * liNumCheckpoints = event->GetCheckpointCount()   [assert <= 16, cpp:6040]
//     * for each checkpoint: landmark = TriggerData::FindLandmark(checkpoint.GetLandmarkId())
//       [assert cpp:6045], and the landmark's REGION INDEX is the LandmarkIndex that goes in.
//     * AddEvent(eventJunctionId, lightTriggerId, landmarks, liNumCheckpoints)
//   then the whole interface is copied into the output buffer and its valid flag raised.
//
// THE LANDMARK STORED IS THE REGION INDEX, NOT THE CHECKPOINT'S LANDMARK ID. Easy to get
// backwards: the checkpoint record carries a CgsID-shaped landmark id, which is only the KEY
// into the track's trigger table; the halfword that goes into the event record is the resolved
// Landmark's TriggerRegion::miRegionIndex at +0x28. That is what makes the GUI side's
// LandmarkIndex a small dense index it can look a map icon up by, and it is why the lookup
// through TriggerData has to happen here rather than on the GUI side.
//
// THE EVENT TYPE IS BOTH THE FILTER AND THE ANSWER SPACE. liEventType is an already-mapped
// event id -- the same space ProgressionManager::GetEvent / GetOnlin answer in (0/3/4/5/7/8
// offline, 10/11/13 online) -- NOT a raw RaceEventData mode byte. The ">= 10" test is what
// picks the offline vs the online slot of each junction, and it works precisely because the
// online ids start at 10.
//
// THE OVERRIDE WORD, and it is not a deviation this lane invented. The offline arm reads a
// game-type word off the module first and only falls back to the event's own mode when that
// word holds 6:
//     liGameType = <the module's game-type override>;
//     if (liGameType == 6) liGameType = lpRaceEvent->GetMode();
//     liMappedEventType = ProgressionManager::GetEvent(liGameType);
// That word has no member and no writer anywhere in the image -- this function,
// CheckIfPlayerIsAtJunctionWithAnEvent and StartModeAtLights are its only appearances, and
// GameStateModule_gSR_00.cpp already carries the full FLAG for it. Its retail value is the 6
// sentinel ("no override": RaceEventData::EModeType tops out at 5). Reproduced the same way
// that TU reproduces it -- the sentinel path taken unconditionally, the expression written out
// above. DELETE-WHEN ProgressionDebugComponent is reconstructed.
//
// A SHIPPED QUIRK REPRODUCED, NOT CORRECTED: when the mapped id matches but the event has ZERO
// checkpoints, the record is still added, with an empty landmark set. The GUI consumer's own
// finish-point scan is what then reads such a record at index -1 (documented on its side);
// "fixing" either half would change which bits the online-finish-point mask carries.
//
// [FLAG PC bring-up] THREE NULL GUARDS THE SHIPPED CODE DOES NOT HAVE. It asserts each of the
// three resources and then dereferences it regardless. It can afford that because all three
// are resident long before the map menu can be opened; on PC any of them can still be
// outstanding, and a null here is an AV rather than a degraded table. Publishing an
// EMPTY-but-valid table would be the silent-drop shape -- the GUI would then resolve nothing
// and never be told why -- so the message is not sent at all and the valid flag stays false,
// exactly as it is before the request arrives.
// DELETE-WHEN the three acquires are proven complete before the map menu can request this.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer + the interface accessor
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // SpecificGameModeEventInterface
#include "GameSource/GameState/BrnGameStateTypes.h"                     // BrnGameState::LandmarkIndex

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"           // TrafficData / GetHull / the junction lookup
#include "SharedClasses/Traffic/BrnTrafficHull.h"                       // Hull::muNumLightTriggers
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"               // KU_LIGHT_TRIGGER_ID_OWNER_TAG
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"            // BrnTraffic::KU_MAX_HULLS
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"        // JunctionLogicBox::GetEventJunctionID
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::FindLandmark
#include "SharedClasses/Trigger/BrnLandmark.h"                          // Landmark -> TriggerRegion::GetRegionIndex
#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData junction table
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // EventJunction / RaceEventData / CheckpointData
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // Assert::BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint

namespace BrnGameState
{

namespace
{
    // Verbatim baked assert paths for this function's asserts.
    const char* const KAC_LIGHT_TRIGGER_H =
        "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficLightTrigger.h";
    const char* const KAC_RACE_EVENT_DATA_H =
        "..\\..\\..\\SharedClasses\\Progression/BrnRaceEventData.h";
    const char* const KAC_GAMESTATEMODULE_CPP =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp";

    // The ">= 10" split between the junction's offline and online event slot. 10 is the
    // first online id ProgressionManager::GetOnlin answers with.
    const s32 KI_FIRST_ONLINE_EVENT_TYPE = 10;

    // The EventJunction lookup the shipped code open-codes here for the fourth time in the
    // image (count word +0x1C, table base +0x18, 16-byte stride, keyed on the junction's own
    // id word). Routed through the named ProgressionData accessors so no body touches those
    // two words directly -- the same shape GameStateModule_gSR_00.cpp's own copy carries.
    const BrnProgression::EventJunction* FindEventJunctionById(
            const BrnProgression::ProgressionData* lpProgressionData, u32 luEventJunctionID)
    {
        if (lpProgressionData == 0)
        {
            return 0;
        }
        const u32 luCount = lpProgressionData->GetEventJunctionCount();
        for (u32 lu = 0; lu < luCount; ++lu)
        {
            const BrnProgression::EventJunction* lpJunction = lpProgressionData->GetEventJunction(lu);
            if (lpJunction != 0 && lpJunction->GetID() == luEventJunctionID)
            {
                return lpJunction;
            }
        }
        return 0;
    }

    // ========================================================================
    // The per-junction record build. Written once because the shipped code emits it once --
    // both the offline and the online arm fall into the same tail; inlining it twice would be
    // a transcription of the branch layout, not of the code. A file-static free helper rather
    // than a member, because the shipped image publishes no symbol for it.
    // ========================================================================
    void AddPreSetRaceEvent(GameStateModuleIO::SpecificGameModeEventInterface& lrEvents,
                            const BrnProgression::RaceEventData&               lrRaceEvent,
                            const BrnTrigger::TriggerData&                     lrTriggerData,
                            s32                                                liEventJunctionID,
                            u32                                                luLightTriggerId)
    {
        // The record's landmark array is 16 wide, and this is the assert that says so.
        const s32 liNumCheckpoints = lrRaceEvent.GetCheckpointCount();
        if (liNumCheckpoints > GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("liNumCheckpoints<=KI_MAX_LANDMARKS_IN_MODE",
                                       KAC_GAMESTATEMODULE_CPP, 6040);
            CgsDev::Assert::EndAssert();
        }

        // [FLAG PC bring-up] THE CLAMP IS NOT IN THE SHIPPED CODE. The assert above is its ONLY
        // protection for the 32-byte landmark scratch, and CGS_ASSERT does not halt this
        // build, so an over-long authored event would run the loop off the end of the array --
        // and then hand the same over-long count to AddEvent, whose Event::Construct copies that
        // many LandmarkIndex into a 16-slot member. Clamped here, at both the loop bound and the
        // count that goes into the record, so the two stay consistent.
        // DELETE-WHEN the assert is a halting one on this build.
        s32 liCount = liNumCheckpoints;
        if (liCount > GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE)
        {
            liCount = GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE;
        }

        // The 32-byte landmark scratch: 16 LandmarkIndex, filled front to back.
        LandmarkIndex laLandmarkIndices[GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE];

        // ZERO CHECKPOINTS IS NOT A REJECTION -- see the banner. The loop is skipped
        // and the record is still added, with an empty landmark set.
        for (s32 liCheckpoint = 0; liCheckpoint < liCount; ++liCheckpoint)
        {
            // The bound assert GetCheckpointData owns, baked at BrnRaceEventData.h:953. The
            // shipped code open-codes it at this site and then reads the table directly, so it
            // is restated here rather than delegated.
            if (liCheckpoint < 0 || liCheckpoint >= lrRaceEvent.GetCheckpointCount())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "liCheckpointIndex >= 0 && liCheckpointIndex < miCheckpointCount",
                    KAC_RACE_EVENT_DATA_H, 953);
                CgsDev::Assert::EndAssert();
            }

            const BrnProgression::CheckpointData* const lpCheckpoint =
                lrRaceEvent.GetCheckpointData(liCheckpoint);

            const BrnTrigger::Landmark* const lpLandmark =
                (lpCheckpoint != 0)
                    ? lrTriggerData.FindLandmark(static_cast<CgsID>(lpCheckpoint->GetLandmarkId()))
                    : 0;
            CGS_ASSERT(lpLandmark != 0, "lpLandmark");   // :6045

            // [FLAG PC bring-up] the shipped code reads the landmark's +0x28 halfword with no
            // null test and stores whatever comes back. The slot still has to be written -- the
            // record's
            // landmark count covers it either way -- so an unresolved landmark records region
            // index 0 rather than dereferencing a null. A divergence, stated rather than hidden.
            // DELETE-WHEN the trigger table is proven to resolve every authored checkpoint.
            laLandmarkIndices[liCheckpoint] =
                LandmarkIndex((lpLandmark != 0) ? lpLandmark->GetRegionIndex() : 0);
        }

        // The record: the event junction id, the packed light-trigger id, the scratch array
        // and the landmark count.
        lrEvents.AddEvent(liEventJunctionID, luLightTriggerId, laLandmarkIndices, liCount);
    }
}

// ============================================================================
// SendSpecificPreSetRacesModesAction. See the file banner.
// ============================================================================
void GameStateModule::SendSpecificPreSetRacesModesAction(
        s32                              liEventType,
        GameStateModuleIO::OutputBuffer* lpOutput)
{
    // The three resource fetches, in the shipped order.
    const BrnProgression::ProgressionData* const lpProgressionData =
        mProgressionManager.GetProgressionData();
    const BrnTraffic::TrafficData* const lpTrafficData  = mTriggerQueryManager.GetTrafficData();
    const BrnTrigger::TriggerData* const lpTriggerData  = mTriggerQueryManager.GetTriggerData();

    CGS_ASSERT(lpProgressionData != 0, "lpProgressionData");   // :5992
    CGS_ASSERT(lpTrafficData     != 0, "lpTrafficData");       // :5993
    CGS_ASSERT(lpTriggerData     != 0, "lpTriggerData");       // :5994

    // [FLAG PC bring-up] NOT IN THE SHIPPED BUILD -- see the banner's THREE NULL GUARDS.
    if (lpOutput == 0 || lpProgressionData == 0 || lpTrafficData == 0 || lpTriggerData == 0)
    {
        static bool sbNoResourcesLogged = false;
        if (!sbNoResourcesLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbNoResourcesLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[preset-races] SKIPPED: SendSpecificPreSetRacesModesAction is missing a "
                   "resource (progression/traffic/trigger); the GUI preset-event table stays "
                   "empty this request.\n";
        }
        return;
    }

    // The stack-local interface: the Array's unconstructed sentinel is stamped at entry and
    // cleared to a live length of zero once the three asserts are past. Both lines below.
    GameStateModuleIO::SpecificGameModeEventInterface lEvents;
    lEvents.Construct();

    const u32 luNumHulls = lpTrafficData->muNumHulls;   // +0x02
    for (u32 luHull = 0; luHull < luNumHulls; ++luHull)
    {
        const BrnTraffic::Hull* const lpHull = lpTrafficData->GetHull(luHull);
        if (lpHull == 0)
        {
            continue;   // [GUARD] GetHull carries the shipped bounds assert
        }

        const u32 luNumLightTriggers = lpHull->muNumLightTriggers;   // +0x0E
        for (u32 luTrigger = 0; luTrigger < luNumLightTriggers; ++luTrigger)
        {
            // BrnTraffic::LightTriggerId::Set(luHull, luTrigger), inlined. Both bounds asserts
            // are baked at BrnTrafficLightTrigger.h:211/:212 and both are reproduced -- this
            // code PACKS the handle, so a fire means the shipped lane graph changed shape.
            // (The shipped compare is against the SCALED index, luHull * 4 vs 1600, i.e. the
            // same KU_MAX_HULLS == 400.)
            if (luHull >= BrnTraffic::KU_MAX_HULLS)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("luHull < KU_MAX_HULLS", KAC_LIGHT_TRIGGER_H, 211);
                CgsDev::Assert::EndAssert();
            }
            if (luTrigger >= 256u)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("luLightTriggerIndex < 256", KAC_LIGHT_TRIGGER_H, 212);
                CgsDev::Assert::EndAssert();
            }

            const u32 luLightTriggerId =
                (luHull << 8) | BrnTraffic::KU_LIGHT_TRIGGER_ID_OWNER_TAG | luTrigger;

            const BrnTraffic::JunctionLogicBox* const lpJunctionLogicBox =
                lpTrafficData->GetJunctionLogicBoxForTrafficLight(luLightTriggerId);
            CGS_ASSERT(lpJunctionLogicBox != 0, "lpJunctionLogicBox");   // :6011
            if (lpJunctionLogicBox == 0)
            {
                continue;   // [GUARD] -- the shipped code reads +0x38 right after its assert
            }

            // The +0x38 compare is SIGNED, so the -1 "this junction has no event" sentinel and
            // any other negative word both drop out.
            const s32 liEventJunctionID = static_cast<s32>(lpJunctionLogicBox->GetEventJunctionID());
            if (liEventJunctionID <= -1)
            {
                continue;
            }

            const BrnProgression::EventJunction* const lpEventJunction =
                FindEventJunctionById(lpProgressionData, static_cast<u32>(liEventJunctionID));
            if (lpEventJunction == 0)
            {
                continue;   // the shipped scan simply falls out of the loop
            }

            // The offline / online slot choice, and the mode -> event-id map on each side.
            s32 liMappedEventType = -1;
            if (liEventType >= KI_FIRST_ONLINE_EVENT_TYPE)
            {
                const BrnProgression::RaceEventData* const lpRaceEvent =
                    lpEventJunction->GetOnlineEvent();                    // +0x08
                if (lpRaceEvent == 0)
                {
                    continue;
                }
                liMappedEventType = mProgressionManager.GetOnlin(
                    static_cast<u32>(lpRaceEvent->GetOnlineMode()));      // +0xED
                if (liMappedEventType != liEventType)
                {
                    continue;
                }
                AddPreSetRaceEvent(lEvents, *lpRaceEvent, *lpTriggerData,
                                   liEventJunctionID, luLightTriggerId);
            }
            else
            {
                const BrnProgression::RaceEventData* const lpRaceEvent =
                    lpEventJunction->GetOfflineEvent();                   // +0x04
                if (lpRaceEvent == 0)
                {
                    continue;
                }
                // THE OVERRIDE WORD -- see the banner. 6 is the retail sentinel, so the
                // fallback (the event's own mode byte) is the path taken.
                const s32 liDataGameType = static_cast<s32>(lpRaceEvent->GetMode());
                liMappedEventType        = mProgressionManager.GetEvent(liDataGameType);
                if (liMappedEventType != liEventType)
                {
                    continue;
                }
                AddPreSetRaceEvent(lEvents, *lpRaceEvent, *lpTriggerData,
                                   liEventJunctionID, luLightTriggerId);
            }
        }
    }

    // The publish copy, spelled as the assignment of the named member it lands on (the
    // interface is a pointer-free POD; its _AssertLayout pins the 0x1E18 width that both the
    // producer's copy and the bridge's use).
    lpOutput->GetSpecificGameModeEventInterface() = lEvents;

    // And the flag the GameState->Gui bridge polls.
    lpOutput->SetSpecificGameModeEventInterfaceIsValid(true);

    // [DIAG] NOT IN THE SHIPPED BUILD -- the one line that proves the table filled, once per
    // request. DELETE-WHEN the preset-races path has a regression test behind it.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[preset-races] published " << static_cast<s32>(lEvents.GetNumEvents())
            << " preset events for event type " << liEventType
            << " from " << static_cast<s32>(luNumHulls) << " hulls\n";
    }
}

} // namespace BrnGameState
