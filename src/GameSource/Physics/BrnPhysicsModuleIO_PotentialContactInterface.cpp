#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // gpDebugPrint / gxMessageFilterFlags (AddEvent(u32) overflow warning)

// BrnPhysics::PhysicsModuleIO::PotentialContactInterface member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the five X360-emitted methods:
//
//   Construct()                @ 0x825A96C8  -- mark IOBuffer constructed, null mpQueue, Construct 14 queues
//   SetConstQueue()            @ 0x825A03C8  write-lock -> store the const source queue pointer
//   AddEvent()                 @ 0x825E72F0  write-lock -> forward into maCustomEventQueues[0].AddEventSafe
//   GetLength() const          @ 0x825A0498  read-lock  -> queue0 live count + mpQueue live count
//   GetEvent(s32) const        @ 0x825A0578  read-lock  -> checked element across mpQueue + custom queues
//
// Lock strings carry the trailing \n per X360 rodata (aNotLockedForRe / aNotLockedForWr);
// the "mpQueue != NULL"/"lpQueue != NULL"/"liIndex < GetLength()" tripwires do NOT.

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    // X360 0x825A96C8: mark the IOBuffer constructed (stb 1,0), null mpQueue (stw 0,4),
    // then Construct() each of the 14 inline custom PotentialContact queues.
    void PotentialContactInterface::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mpQueue = nullptr;
        for (s32 liQueue = 0; liQueue < KI_CUSTOM_QUEUE_COUNT; ++liQueue)
        {
            maCustomEventQueues[liQueue].Construct();
        }
    }

    // X360 0x825A03C8: write-lock tripwire, then a lpQueue != NULL tripwire, then store the
    // supplied const source queue pointer into mpQueue (this+4). The store is unconditional.
    void PotentialContactInterface::SetConstQueue(const InPotentialContactQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpQueue != nullptr, "lpQueue != NULL");
        mpQueue = lpQueue;
    }

    // X360 0x825E72F0: write-lock tripwire, then a mpQueue != NULL tripwire, then forward the
    // event into the first custom queue via its bounds-gated AddEventSafe. DWARF gives void;
    // the X360 tail-calls AddEventSafe and the void caller ignores its bool result.
    void PotentialContactInterface::AddEvent(const CgsSceneManager::SceneManagerIO::PotentialContact& lEvent)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(mpQueue != nullptr, "mpQueue != NULL");
        maCustomEventQueues[0].AddEventSafe(lEvent);
    }

    // ⭐ ADDED 2026-08-14 (walls leg 3, harvest wave). X360 0x825E73D0 (the out-of-line copy of
    // the console-inline DWARF :147 overload; assert BrnPhysicsModuleIO.h:596, warning gated on
    // `miLength >= 0x800` + message filter bit 0). No lock tripwire -- the console body has none
    // (its three callers all run under the physics update's write lock). The bool AddEventSafe
    // result is dropped exactly as the console drops r3.
    void PotentialContactInterface::AddEvent(u32 luQueueID,
                                             const CgsSceneManager::SceneManagerIO::PotentialContact& lrEvent)
    {
        CGS_ASSERT(luQueueID < static_cast<u32>(KI_CUSTOM_QUEUE_COUNT),
                   "luQueueID < (uint32_t)E_NUM_CUSTOM_QUEUE_TYPES");            // BrnPhysicsModuleIO.h:596

        CustomPotentialContactQueue& lrQueue = maCustomEventQueues[luQueueID];
        if (lrQueue.GetLength() >= 2048 && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "PHYSICS WARNING: Run out of space in contact queue "
                                       << luQueueID << "\n";
        }
        lrQueue.AddEventSafe(lrEvent);
    }

    // X360 0x825A0498: read-lock tripwire, then a mpQueue != NULL tripwire, then returns the
    // live count of the first custom queue (this+0x18) plus the live count of the const source
    // queue (mpQueue+8).
    s32 PotentialContactInterface::GetLength() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        CGS_ASSERT(mpQueue != nullptr, "mpQueue != NULL");
        return maCustomEventQueues[0].GetLength() + mpQueue->GetLength();
    }

    // X360 0x825A0578: checked event accessor across the union of the const input queue (mpQueue)
    // and the 14 inline custom event queues. Read-locked.
    //
    // Assert read-lock; assert mpQueue != NULL; assert liIndex < GetLength(). Then split liIndex on
    // mpQueue->GetLength() (miLength @ +8): indices below it read from mpQueue; the remainder
    // indexes the custom-queue aggregate at this+0x10 (&maCustomEventQueues[0]) -- the X360 makes a
    // SINGLE GetEvent(remainder) call on the first custom queue, which reads across the contiguous
    // inline queues by design.
    const CgsSceneManager::SceneManagerIO::PotentialContact&
    PotentialContactInterface::GetEvent(s32 liIndex) const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        CGS_ASSERT(mpQueue != nullptr, "mpQueue != NULL");
        CGS_ASSERT(liIndex < GetLength(), "liIndex < GetLength()");

        const s32 liQueueLength = mpQueue->GetLength();
        if (liIndex >= liQueueLength)
        {
            const CustomPotentialContactQueue* lpCustom =
                reinterpret_cast<const CustomPotentialContactQueue*>(&maCustomEventQueues[0]);
            return lpCustom->GetEvent(liIndex - liQueueLength);
        }
        return mpQueue->GetEvent(liIndex);
    }

    // X360 0x825A06A0 (DWARF :159; ADDED 2026-08-06, bridge de-facade wave): the ContactId-
    // keyed accessor. GetQueueId() carries its own bounds tripwire (BrnContactId.h:171, fired
    // inline in the X360 body); a non-zero queue id indexes maCustomEventQueues[qid] directly
    // (asm: this + 163856*qid + 16) with the fire-and-continue length tripwire
    // (BrnPhysicsModuleIO.h:674) before the queue's own checked GetEvent; queue id 0 routes
    // through GetEvent(s32) @0x825A0578 (the mpQueue/custom split). Sole caller:
    // PhysicsModule::ProcessContactSpy @0x825AB5A4, resolving a spy's muTag.
    const CgsSceneManager::SceneManagerIO::PotentialContact&
    PotentialContactInterface::GetEvent(ContactId lContactId) const
    {
        const u32 luQueueId      = static_cast<u32>(lContactId.GetQueueId());
        const u16 lu16EventIndex = lContactId.GetEventIndex();

        if (luQueueId != 0u)   // != E_QUEUE_TYPE_EXTERNAL_FROM_SCENE_CONTACTS
        {
            CGS_ASSERT(lu16EventIndex < maCustomEventQueues[luQueueId].GetLength(),
                       "lu16EventIndex < maCustomEventQueues[leQueueId].GetLength()");   // BrnPhysicsModuleIO.h:674
            return maCustomEventQueues[luQueueId].GetEvent(static_cast<s32>(lu16EventIndex));
        }

        return GetEvent(static_cast<s32>(lu16EventIndex));
    }
}
}
