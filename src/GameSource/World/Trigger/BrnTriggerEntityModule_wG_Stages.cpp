// ============================================================================
// BrnWorld::TriggerEntityModule -- the three per-frame WORLD stages, plus the
// remove-trigger drain they need.
//
// Partfile of the class's own TU (BrnTriggerEntityModule.cpp, same directory).
// Reconstructed bodies:
//   PreSceneUpdate             0x822EEBF0   PrePhysicsUpdate           0x822FA018
//   PostSceneUpdate            0x823082E0   ProcessRemoveTriggerEvents 0x822D9A90
//
// The three stage bodies are lock/drain/unlock spines. The lock order is NOT symmetric
// between them and is the console's -- see each body.
// FLAG: asserts carry the baked file/line verbatim via explicit BeginAssert/FireAssert/
// EndAssert like the sibling TU, but the console's StrStream message-building is dropped
// for the project's plain assert convention.
// ============================================================================
#include "BrnTriggerEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                               // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                           // CgsModule::IOBuffer lock pair
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                     // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"                     // CgsSceneManager::VolumeId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"             // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface (Remove* producers)
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"

namespace BrnWorld
{
namespace
{
    // The baked source path stamped into every assert in this class (verbatim, same string
    // the sibling TU carries).
    const char* const KAC_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
        "../World/EntityModules/TriggerEntityModule/BrnTriggerEntityModule.cpp";

    // E_ENTITYTYPE_TRIGGER -- the EntityId OWNER stamped on every trigger's scene entity.
    // Same pinned 4 the sibling TU documents (ProcessAddTriggerEvents packs it, and
    // ProcessLineTestFineResult asserts each hit's owner against it).
    const u32 KU_ENTITYTYPE_TRIGGER = 4;
}

// ---------------------------------------------------------------------------
// ProcessRemoveTriggerEvents -- 0x822D9A90.
//
// Drains the management input's fixed remove-trigger queue (EventQueue<InRemoveTriggerEvent,
// 256> at interface +131088). For each event: walk the SET bits of mUsedTriggerList looking
// for the slot whose Trigger record carries the event's gameplay handle (record +8), then
// retire that slot's scene registration and free the bit.
//
// Argument WIDTHS: RemoveVolumeInstance and RemoveVolume take the whole 64-bit scene id,
// RemoveEntity takes only the 32-bit entity word out of its high half -- the same split
// InSceneUpdateInterface records for ActiveRaceCar::RemoveFromScene.
//
// FLAG: the id is RECOMPUTED from the slot index rather than read back from the record.
// ProcessAddTriggerEvents derives it from the slot alone (packed EntityId in the high dword,
// low dword zero) but keeps it in a local; the record's leading 8 bytes are still spelled as
// unrecovered padding in BrnTriggerTypes.h. Same bits; DELETE-WHEN that field is named and
// Add stores it there.
// ---------------------------------------------------------------------------
void TriggerEntityModule::ProcessRemoveTriggerEvents(
    const TriggerEntityModuleIO::TriggerManagementInputInterface* lpInputInterface,
    OutputBuffer_PreScene::SceneInputInterface* lpOutSceneInputInterface)
{
    if (!lpInputInterface)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpInputInterface", KAC_FILE, 436);
        CgsDev::Assert::EndAssert();
    }
    if (!lpOutSceneInputInterface)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutSceneInputInterface", KAC_FILE, 437);
        CgsDev::Assert::EndAssert();
    }

    const TriggerEntityModuleIO::TriggerManagementInputInterface::RemoveTriggerQueue& lrQueue =
        lpInputInterface->GetRemoveTriggerEventQueue();

    // The console's loop counter is a plain int re-compared against the queue's live length
    // on every iteration (the length is not cached).
    for (s32 liEvent = 0; liEvent < lrQueue.GetLength(); ++liEvent)
    {
        const TriggerEntityModuleIO::InRemoveTriggerEvent& lrEvent = lrQueue.GetEvent(liEvent);

        // Lowest USED slot, then keep stepping to the next used slot until the handles match.
        s32 liTriggerIndex = mUsedTriggerList.GetFirstNonZeroBit();
        while (liTriggerIndex != CgsContainers::BitArray<KU_MAX_TRIGGERS>::KI_INVALID_BITINDEX
               && mTriggers[liTriggerIndex].GetHandle() != lrEvent.mTriggerID)
        {
            liTriggerIndex = mUsedTriggerList.GetNextNonZeroBit(liTriggerIndex);
        }

        if (liTriggerIndex == CgsContainers::BitArray<KU_MAX_TRIGGERS>::KI_INVALID_BITINDEX)
        {
            // The console appends the unfound handle to the message buffer here; the project
            // convention drops the formatting and keeps the literal prefix.
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Failed to find trigger to remove: ", KAC_FILE, 471);
            CgsDev::Assert::EndAssert();
            continue;
        }

        // The record's scene id: entity word in the HIGH dword, low dword zero.
        CgsSceneManager::EntityId lEntityId;
        lEntityId.Set(KU_ENTITYTYPE_TRIGGER, static_cast<u32>(liTriggerIndex), 0);
        const u64 lu64SceneId =
            static_cast<u64>(static_cast<u32>(lEntityId)) << 32;

        CgsSceneManager::VolumeInstanceId lVolumeInstanceId;
        lVolumeInstanceId.muId = lu64SceneId;

        lpOutSceneInputInterface->RemoveVolumeInstance(lVolumeInstanceId);
        lpOutSceneInputInterface->RemoveEntity(lEntityId, 0);
        lpOutSceneInputInterface->RemoveVolume(CgsSceneManager::VolumeId(lu64SceneId));

        mUsedTriggerList.UnSetBit(static_cast<u32>(liTriggerIndex));
    }
}

// ---------------------------------------------------------------------------
// PreSceneUpdate -- 0x822EEBF0.
//
// Lock order: WRITE the output first, then READ the input; unlock read-then-write.
// The scene-input accessor is called once per drain rather than hoisted, as the console
// does, and the input interface is taken through the CONST getter.
// REMOVE runs BEFORE ADD: a handle retired and re-armed in the same frame must not
// collide with its own stale registration.
// ---------------------------------------------------------------------------
void TriggerEntityModule::PreSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                          CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                          TriggerEntityModuleIO::InputBuffer_PreScene* lpInput,
                                          TriggerEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                                          BrnUpdateSet /*lUpdateSet*/ )
{
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    const TriggerEntityModuleIO::InputBuffer_PreScene* lpConstInput = lpInput;

    ProcessRemoveTriggerEvents(lpConstInput->GetInputInterface(),
                               lpOutput->GetSceneInputInterface());
    ProcessAddTriggerEvents(lpConstInput->GetInputInterface(),
                            lpOutput->GetSceneInputInterface());

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

// ---------------------------------------------------------------------------
// PrePhysicsUpdate -- 0x822FA018.
//
// Lock order here is the reverse of the other two stages: READ the input first, then
// WRITE the output. Unlock is read-then-write.
// ---------------------------------------------------------------------------
void TriggerEntityModule::PrePhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                            CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                            TriggerEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                                            TriggerEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                                            BrnUpdateSet /*lUpdateSet*/ )
{
    lpInput->LockForRead();
    lpOutput->LockForWrite();

    ProcessSceneQueryResults(lpInput, lpOutput);

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

// ---------------------------------------------------------------------------
// PostSceneUpdate -- 0x823082E0.
//
// Lock order: WRITE the output first, then READ the input; unlock read-then-write.
// ---------------------------------------------------------------------------
void TriggerEntityModule::PostSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                           CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                           TriggerEntityModuleIO::InputBuffer_PostScene* lpInput,
                                           TriggerEntityModuleIO::OutputBuffer_PostScene* lpOutput,
                                           BrnUpdateSet /*lUpdateSet*/ )
{
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    ProcessTriggerQueryEvents(lpInput, lpOutput);

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}
}
