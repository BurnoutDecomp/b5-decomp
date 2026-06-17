#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

namespace BrnGameState
{

// X360 0x82326538.
// Walk every landmark index currently armed for the active mode and post a matching
// "remove trigger" event onto the trigger-entity module's remove queue, then empty the array.
// The X360 reads the live-element count each iteration through the checked accessor (the
// inlined "Array used before Construct/Clear was called" assert at CgsArray.h:336 fires from
// GetLength()), builds the trigger id by OR-ing the region index with the landmark type-bits
// tag (0x38000000), and appends it. (The X360 return-of-the-last-result pointer is a register
// artifact; this is logically a void command.)
//
// lrRemoveTriggerQueue is the BrnWorld::TriggerEntityModuleIO remove-trigger queue. The X360
// caller (ModeManager::StartGameMode) passes it as the sub-object at +131088 of the trigger-
// entity input interface; that enclosing aggregate's full layout/name is not committed yet, so
// the queue is taken by reference here.
void TriggerQueryManager::ClearLandmarkIndexesForGameMode(
    CgsModule::EventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent, 256>& lrRemoveTriggerQueue)
{
    // High type-bits tag OR-ed into the region index to form the trigger id of a landmark
    // region (X360 baked constant 0x38000000).
    const u32 KU_LANDMARK_TRIGGER_ID_TYPE_BITS = 0x38000000u;

    for (u32 luIndex = 0; luIndex < maLandmarkIndexes.GetLength(); ++luIndex)
    {
        BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent lRemoveEvent;
        lRemoveEvent.mTriggerID =
            static_cast<u32>(static_cast<s32>(maLandmarkIndexes.GetItem(luIndex)))
            | KU_LANDMARK_TRIGGER_ID_TYPE_BITS;
        lrRemoveTriggerQueue.AddEvent(lRemoveEvent);
    }

    // Empty the array (X360: store 0 to the live-count word at this+1864).
    maLandmarkIndexes.Clear();
}

// X360 0x823265E8.
// Arm one landmark index for the active mode. If it is already present, do nothing; otherwise
// invalidate the cached trigger-query state and append it. The Hex-Rays 14-arg signature is a
// register-spill artifact of the inlined Array<>::Contains/Append — the real call takes a single
// 16-bit LandmarkIndex (a14 = a2 builds it on the stack and is passed by &a14 to both).
//
// When the AI message-filter bit is set the X360 logs "luLandmarkIndex: <value>\n" through the
// global debug-print stream (the committed operator<< form, matching CgsVariableEventQueue.cpp's
// filter-gated `*CgsDev::Log::gpDebugPrint << ...` precedent).
bool TriggerQueryManager::AddLandmarkIndexForGameMode(LandmarkIndex lLandmarkIndex)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "luLandmarkIndex: " << static_cast<s32>(lLandmarkIndex) << "\n";
    }

    if (maLandmarkIndexes.Contains(lLandmarkIndex))
    {
        return true;
    }

    // New index: mark the cached trigger-query state stale, then store it.
    mbTriggersUpdated = false;                  // X360: byte store 0 at this+1808
    maLandmarkIndexes.Append(lLandmarkIndex);
    return true;
}

// X360 0x82355D78.
// Return the trigger id of the traffic-light region the player is currently in. The X360
// asserts the precondition (the player really is in such a region) before reading the cached id;
// the assert source path/line are baked as BrnTriggerQueryManager.h:318 (discarded -> CGS_ASSERT
// fills __FILE__/__LINE__). The hidden-pointer return in Hex-Rays is the X360 rendering of
// returning the 4-byte handle by value.
LightTriggerId TriggerQueryManager::GetPlayerCurrentTrafficLightId() const
{
    CGS_ASSERT(IsPlayerInTrafficLightRegion(), "IsPlayerInTrafficLightRegion()");
    return mPlayerCurrentTrafficLightId;
}

}
