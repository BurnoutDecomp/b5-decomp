#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"

#include <cstring>   // memcpy

// CgsGui::EventInterpreterModule -- the 11 X360-attested bodies that live in this TU,
// reconstructed from BURNOUT_X360_ARTIST.XEX ("ARTIST"/"Breaker", Jan-2008):
//
//   FindObserverInEventObserverList  @ 0x82847D78
//   IsPriorityEvent                  @ 0x8284FAB8
//   IsEventBlocked                   @ 0x82852DA0
//   PreUpdateObservers               @ 0x82847C98
//   UpdateObservers                  @ 0x82847BB8
//   Prepare                          @ 0x82847A48
//   Release                          @ 0x82847B00
//   SetBlocking                      @ 0x82847FC8
//   StopPriorityEventBlocking        @ 0x82847F08
//   ProcessInEvents                  @ 0x8285B448
//   ProcessOutEvents                 @ 0x8285E1D0
//
// Behaviour + member offsets are read off the X360 assembly; the nested-observer inline
// helpers and signature shapes come from the DecFIGS DWARF and the Feb-2007 partial source.
// The other module methods (Construct/Destruct/PreWorldUpdate/Update/Add*/Register*/etc.) are
// declared in the header but bodied in their own TUs; their declarations resolve the callees
// this TU references.

namespace CgsGui
{

// ---------------------------------------------------------------------------------------
// operator++ (prepare/release stage post-increment).  @ 0x828470D0 / 0x82847130
// Capture the current value, advance the reference, assert it has not overrun the DONE
// terminal, and return the value held before the increment.
// ---------------------------------------------------------------------------------------
EventInterpreterModule::EventInterpreterPrepareStage operator++(
    EventInterpreterModule::EventInterpreterPrepareStage& lrStage, int)
{
    const EventInterpreterModule::EventInterpreterPrepareStage leOldStage = lrStage;
    lrStage = static_cast<EventInterpreterModule::EventInterpreterPrepareStage>(leOldStage + 1);
    CGS_ASSERT(lrStage <= EventInterpreterModule::E_PREPARE_DONE,
               "leEnumIndex <= EventInterpreterModule::E_PREPARE_DONE");
    return leOldStage;
}

EventInterpreterModule::EventInterpreterReleaseStage operator++(
    EventInterpreterModule::EventInterpreterReleaseStage& lrStage, int)
{
    const EventInterpreterModule::EventInterpreterReleaseStage leOldStage = lrStage;
    lrStage = static_cast<EventInterpreterModule::EventInterpreterReleaseStage>(leOldStage + 1);
    CGS_ASSERT(lrStage <= EventInterpreterModule::E_RELEASE_DONE,
               "leEnumIndex <= EventInterpreterModule::E_RELEASE_DONE");
    return leOldStage;
}

// ---------------------------------------------------------------------------------------
// Prepare  @ 0x82847A48
// Step the prepare-stage state machine: from START kick the stage forward (operator++), run
// the base ModuleSingleBuffered::Prepare, then bump the stage again to MANAGER; at MANAGER
// fall straight through to the tail; reject any out-of-range stage. On (re-)completion clear
// the release stage so a later Release() starts clean.
// ---------------------------------------------------------------------------------------
bool EventInterpreterModule::Prepare()
{
    if (mePrepareStage == E_PREPARE_START)
    {
        // START: open the stage, run the base prepare, then advance to MANAGER.
        mePrepareStage++;                               // START -> MANAGER
        if (!CgsModule::ModuleSingleBuffered::Prepare())
        {
            return false;
        }
        mePrepareStage++;                               // MANAGER -> DONE
    }
    else if (mePrepareStage == E_PREPARE_MANAGER)
    {
        // MANAGER: re-entry mid-prepare -- run the base prepare and advance to DONE.
        if (!CgsModule::ModuleSingleBuffered::Prepare())
        {
            return false;
        }
        mePrepareStage++;                               // MANAGER -> DONE
    }
    else if (mePrepareStage > E_PREPARE_DONE)
    {
        // Stage past DONE is unrecognised: assert and reject. (DONE itself falls straight
        // through to the tail with no base-prepare and no stage bump.)
        CGS_ASSERT(false, "Unkown prepare stage");
        return false;
    }

    meReleaseStage = E_RELEASE_START;
    return true;
}

// ---------------------------------------------------------------------------------------
// Release  @ 0x82847B00
// The mirror of Prepare over the release-stage state machine.
// ---------------------------------------------------------------------------------------
bool EventInterpreterModule::Release()
{
    if (meReleaseStage == E_RELEASE_START)
    {
        meReleaseStage++;                               // START -> MANAGER
        if (!CgsModule::ModuleSingleBuffered::Release())
        {
            return false;
        }
        meReleaseStage++;                               // MANAGER -> DONE
    }
    else if (meReleaseStage == E_RELEASE_MANAGER)
    {
        if (!CgsModule::ModuleSingleBuffered::Release())
        {
            return false;
        }
        meReleaseStage++;                               // MANAGER -> DONE
    }
    else if (meReleaseStage > E_RELEASE_DONE)
    {
        CGS_ASSERT(false, "Unkown release stage");
        return false;
    }

    mePrepareStage = E_PREPARE_START;
    return true;
}

// ---------------------------------------------------------------------------------------
// FindObserverInEventObserverList  @ 0x82847D78
// Walk the plain-observer slots looking for the one owning lpEventObserver. Writes the slot
// pointer through lppFoundObserver and returns its index, or -1 when absent. The walk asserts
// it never reaches an empty (null-observer) slot before exhausting the live count.
// ---------------------------------------------------------------------------------------
s32 EventInterpreterModule::FindObserverInEventObserverList(EventObserver* lpEventObserver,
                                                            ObjectEventObserver** lppFoundObserver)
{
    s32  liIndex = 0;
    bool lbFound = false;
    *lppFoundObserver = 0;

    const s32 liCount = mnNumberEventObservers;
    if (liCount > 0)
    {
        ObjectEventObserver* lpSlot = &maEventObservers[0];
        while (true)
        {
            if (lpSlot->mpObserver == lpEventObserver)
            {
                lbFound = true;
                *lppFoundObserver = &maEventObservers[liIndex];
                break;
            }
            if (lpSlot->mpObserver == 0)
            {
                CGS_ASSERT(false,
                    "Should not reach a null value while searching the event observer list: ");
                break;
            }
            ++liIndex;
            ++lpSlot;
            if (liIndex >= liCount)
            {
                break;
            }
        }
    }

    if (!lbFound)
    {
        return -1;
    }
    return liIndex;
}

// ---------------------------------------------------------------------------------------
// IsPriorityEvent  @ 0x8284FAB8
// True iff some priority observer is registered for liEvent. On a hit, resolve the owning
// PLAIN slot for that priority observer (FindObserverInEventObserverList over its observer
// pointer) so the caller learns who owns the event.
// ---------------------------------------------------------------------------------------
bool EventInterpreterModule::IsPriorityEvent(s32 liEvent, ObjectEventObserver** lppOwnerObserver)
{
    s32 liIndex = 0;

    const s32 liCount = mnNumberPriorityEventObservers;
    if (liCount <= 0)
    {
        return false;
    }

    while (!maPriorityEventObservers[liIndex].IsRegisteredForEvent(liEvent))
    {
        ++liIndex;
        if (liIndex >= liCount)
        {
            return false;
        }
    }

    FindObserverInEventObserverList(maPriorityEventObservers[liIndex].mpObserver, lppOwnerObserver);
    return true;
}

// ---------------------------------------------------------------------------------------
// IsEventBlocked  @ 0x82852DA0
// Walk the priority observers; for each that is blocking AND holds override entries, scan its
// override-entry pool (maHashElementPool[0..muHashElementPoolCount)) and test whether liEvent
// is set in any entry's override mask. The first match blocks liEvent: write the owning
// observer pointer through lrpBlockingEventObserver and return true.
// (The X360 streams a dynamic CgsBitArray range assert for liEvent >= 600; reduced to the
// static CGS_ASSERT per project convention.)
// ---------------------------------------------------------------------------------------
bool EventInterpreterModule::IsEventBlocked(s32 liEvent, EventObserver*& lrpBlockingEventObserver) const
{
    const s32 liCount = mnNumberPriorityEventObservers;
    if (liCount <= 0)
    {
        return false;
    }

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        const PriorityObjectEventObserver& lrObserver = maPriorityEventObservers[liIndex];

        // Only blocking observers that actually hold override entries can block.
        if (!lrObserver.mbBlocking || lrObserver.muHashElementPoolCount == 0)
        {
            continue;
        }

        for (u32 luEntry = 0; luEntry < lrObserver.muHashElementPoolCount; ++luEntry)
        {
            CGS_ASSERT(static_cast<u32>(liEvent) < 600u, "luIndex < NUMBITS");

            const sMapEntry& lrMapEntry = lrObserver.maHashElementPool[luEntry].GetValue();
            if (lrMapEntry.mOverriddenEventListBitArray.IsBitSet(static_cast<u32>(liEvent)))
            {
                lrpBlockingEventObserver = lrObserver.mpObserver;
                return true;
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------------------------------
// SetBlocking  @ 0x82847FC8
// Mark every priority observer that shares lpObjectEventObserver's observer pointer as
// blocking. (lpObjectEventObserver must be non-null.)
// ---------------------------------------------------------------------------------------
void EventInterpreterModule::SetBlocking(const ObjectEventObserver* lpObjectEventObserver)
{
    CGS_ASSERT(lpObjectEventObserver != 0, "lpObjectEventObserver != NULL");

    const s32 liCount = mnNumberPriorityEventObservers;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        if (maPriorityEventObservers[liIndex].mpObserver == lpObjectEventObserver->mpObserver)
        {
            maPriorityEventObservers[liIndex].mbBlocking = true;
        }
    }
}

// ---------------------------------------------------------------------------------------
// StopPriorityEventBlocking  @ 0x82847F08
// Clear the blocking flag on the first priority observer that owns lpEventObserver and is
// currently blocking. Returns true if one was cleared. (lpEventObserver must be non-null.)
// ---------------------------------------------------------------------------------------
bool EventInterpreterModule::StopPriorityEventBlocking(EventObserver* lpEventObserver)
{
    CGS_ASSERT(lpEventObserver != 0, "lpEventObserver != NULL");

    const s32 liCount = mnNumberPriorityEventObservers;
    if (liCount <= 0)
    {
        return false;
    }

    s32 liIndex = 0;
    while (maPriorityEventObservers[liIndex].mpObserver != lpEventObserver ||
           !maPriorityEventObservers[liIndex].mbBlocking)
    {
        ++liIndex;
        if (liIndex >= liCount)
        {
            return false;
        }
    }

    maPriorityEventObservers[liIndex].mbBlocking = false;
    return true;
}

// ---------------------------------------------------------------------------------------
// PreUpdateObservers  @ 0x82847C98  (virtual)
// Forward PreWorldUpdate to every live plain observer, asserting none has a null observer.
// ---------------------------------------------------------------------------------------
void EventInterpreterModule::PreUpdateObservers()
{
    const s32 liCount = mnNumberEventObservers;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        ObjectEventObserver& lrSlot = maEventObservers[liIndex];
        CGS_ASSERT(lrSlot.mpObserver != 0, "Invalid Observer within an ObjectEventObserver");
        lrSlot.mpObserver->PreWorldUpdate();
    }
}

// ---------------------------------------------------------------------------------------
// UpdateObservers  @ 0x82847BB8  (virtual)
// Forward Update to every live plain observer, asserting none has a null observer.
// ---------------------------------------------------------------------------------------
void EventInterpreterModule::UpdateObservers()
{
    const s32 liCount = mnNumberEventObservers;
    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        ObjectEventObserver& lrSlot = maEventObservers[liIndex];
        CGS_ASSERT(lrSlot.mpObserver != 0, "Invalid Observer within an ObjectEventObserver");
        lrSlot.mpObserver->Update();
    }
}

// ---------------------------------------------------------------------------------------
// ProcessInEvents  @ 0x8285B448
// Route every event in lpCurrentEvents to each plain observer. The interpreter snapshots the
// observer slots, then for each observer clears its per-observer working queue and replays the
// inbound stream:
//   * priority events (IsPriorityEvent) that the owner itself owns are forwarded as-is, and if
//     the owner matches we also SetBlocking; priority events the observer is subscribed to but
//     does not own become a "priority removal" record (when the observer accepts removals);
//   * non-priority events the observer is subscribed to are forwarded, unless the event is
//     blocked (IsEventBlocked) -- a blocked event the observer owns the block for becomes a
//     "priority blocking" record; a blocked event owned by someone else becomes a removal
//     record (when the observer accepts removals), otherwise it is dropped.
// Finally each observer's first virtual (ProcessEvents) is invoked with its filled queue.
// ---------------------------------------------------------------------------------------
void EventInterpreterModule::ProcessInEvents(GuiEventInputQueue* lpCurrentEvents)
{
    // Snapshot the four plain-observer slots (the X360 memcpy's 0x180 == 4*0x60 bytes).
    ObjectEventObserver laObserverSnapshot[KI_MAX_OBSERVERS];
    memcpy(laObserverSnapshot, maEventObservers, sizeof(laObserverSnapshot));

    const s32 liObserverCount = mnNumberEventObservers;
    for (s32 liObserver = 0; liObserver < liObserverCount; ++liObserver)
    {
        ObjectEventObserver& lrSlot = laObserverSnapshot[liObserver];
        CGS_ASSERT(lrSlot.mpObserver != 0, "Invalid Observer within an ObjectEventObserver");

        GuiEventQueue& lrWorkingQueue = mInProcessQueues[liObserver];
        lrWorkingQueue.Clear();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpCurrentEvents->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != 0)
        {
            ObjectEventObserver* lpOwnerObserver = 0;
            const bool lbIsPriority = IsPriorityEvent(liEventType, &lpOwnerObserver);
            const bool lbObserverOwnsPriority =
                lbIsPriority && (lpOwnerObserver->mpObserver == lrSlot.mpObserver);

            if (lrSlot.IsRegisteredForEvent(liEventType))
            {
                EventObserver* lpBlockingObserver = 0;
                if (lbIsPriority)
                {
                    lrSlot.mRemovedEvent.miRemovedEventId = liEventType;
                    const bool lbBlocked = IsEventBlocked(liEventType, lpBlockingObserver);
                    if (lbObserverOwnsPriority || !lbBlocked)
                    {
                        lrWorkingQueue.AddEvent(lpEvent, liEventType, liEventSize);
                        if (lbObserverOwnsPriority)
                        {
                            SetBlocking(&lrSlot);
                        }
                    }
                    else if (lrSlot.mEventListBitArray.IsBitSet(static_cast<u32>(E_GUI_PRIORITY_REMOVAL)))
                    {
                        // Tell the observer the priority event was removed from under it.
                        lrWorkingQueue.AddEvent(&lrSlot.mRemovedEvent, E_GUI_PRIORITY_REMOVAL, 4);
                    }
                }
                else
                {
                    if (IsEventBlocked(liEventType, lpBlockingObserver))
                    {
                        if (lrSlot.mpObserver != lpBlockingObserver)
                        {
                            if (lrSlot.mEventListBitArray.IsBitSet(static_cast<u32>(E_GUI_PRIORITY_REMOVAL)))
                            {
                                lrSlot.mRemovedEvent.miRemovedEventId = liEventType;
                                lrWorkingQueue.AddEvent(&lrSlot.mRemovedEvent, E_GUI_PRIORITY_REMOVAL, 4);
                            }
                            goto nextEvent;
                        }
                        // The observer owns the block: emit a blocking record then the event.
                        lrSlot.mBlockingEvent.miBlockingEventId = liEventType;
                        lrWorkingQueue.AddEvent(&lrSlot.mBlockingEvent, E_GUI_PRIORITY_BLOCKING, 4);
                    }
                    lrWorkingQueue.AddEvent(lpEvent, liEventType, liEventSize);
                }
            }

        nextEvent:
            liEventType = lpCurrentEvents->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        lrSlot.mpObserver->ProcessEvents(&lrWorkingQueue);
    }
}

// ---------------------------------------------------------------------------------------
// ProcessOutEvents  @ 0x8285E1D0
// Drain the outbound events each plain observer has queued (its StateInterface output queue,
// at mpObserver+16) into a single large staging queue, then dispatch each by type:
//   E_GUI_OUT_REGISTER_FOR_EVENT / UNREGISTER / PRIORITY_* / STOP_BLOCKING -> the matching
//     interpreter API call;
//   E_GUI_OUT_REQUEST_RESOURCE -> push onto the output buffer's resource queue;
//   E_GUI_OUT_EVENT            -> push the boxed raw event onto the output buffer's event queue;
//   E_GUI_OUT_VIEW_STATE       -> push the boxed raw event onto the output buffer's view queue;
//   E_GUI_OUT_INTERNAL_EVENT   -> push the boxed raw event onto the interpreter's internal queue;
//   anything else (only during pre-world-update) -> push onto the output event queue.
// ---------------------------------------------------------------------------------------
void EventInterpreterModule::ProcessOutEvents(OutputBuffer* lpOutput, bool lbIsPreWorldUpdate)
{
    // The X360 staging queue is a CgsModule::VariableEventQueue<65536,16> (the large GUI queue);
    // it gathers every observer's output queue then is drained by type below.
    CgsModule::VariableEventQueue<65536, 16> lStagingQueue;
    lStagingQueue.Construct();

    // Gather every observer's outbound queue into the staging queue. The X360 reaches each
    // observer's queue at mpObserver+16 (its StateInterface's large output queue); accessed here
    // by name through EventObserver::GetOutputEventQueue().
    const s32 liObserverCount = mnNumberEventObservers;
    for (s32 liObserver = 0; liObserver < liObserverCount; ++liObserver)
    {
        ObjectEventObserver& lrSlot = maEventObservers[liObserver];
        CGS_ASSERT(lrSlot.mpObserver != 0, "Invalid Observer within an ObjectEventObserver");

        GuiStackEventQueue::GuiEventQueueLarge* lpObserverOutQueue =
            lrSlot.mpObserver->GetOutputEventQueue();
        CGS_ASSERT(lpObserverOutQueue != 0,
                   "Invalid observer output Queue in EventInterpreterModule::ProcessOutEvents");

        lStagingQueue.Append(*lpObserverOutQueue);
        lpObserverOutQueue->Clear();
    }

    // The E_GUI_OUT_* records below are external serialised event blobs the GUI states packed
    // into the variable event queue (each record's field offsets are baked by the producing
    // StateInterface emitter, not by any C++ type visible here). They are read by word offset
    // off the queue record exactly as the X360 does -- this is the documented serialised-blob
    // exception, not a member-access offset hack.
    const CgsModule::Event* lpEvent = 0;
    s32 liEventSize = 0;
    s32 liEventType = lStagingQueue.GetFirstEvent(&lpEvent, &liEventSize);

    while (lpEvent != 0)
    {
        switch (liEventType)
        {
        case E_GUI_OUT_REGISTER_FOR_EVENT:
        {
            // serialised record: { s32 eventType@+0; EventObserver* observer@+4 }
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            RegisterForEvent(reinterpret_cast<EventObserver*>(lpPayload[1]), lpPayload[0]);
            break;
        }
        case E_GUI_OUT_UNREGISTER_FOR_EVENT:
        {
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            UnRegisterForEvent(reinterpret_cast<EventObserver*>(lpPayload[1]), lpPayload[0]);
            break;
        }
        case E_GUI_OUT_PRIORITY_REGISTER_FOR_EVENT:
        {
            // payload: { s32 eventType@+0; EventObserver* observer@+0x964;
            //            const s32 overrides[]@+4; u32 overrideCount@+0x968 }
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            RegisterPriorityEvent(reinterpret_cast<EventObserver*>(lpPayload[0x968 / 4]),
                                  lpPayload[0],
                                  &lpPayload[1],
                                  static_cast<u32>(lpPayload[0x964 / 4]));
            break;
        }
        case E_GUI_OUT_PRIORITY_UNREGISTER_FOR_EVENT:
        {
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            UnregisterPriorityEvent(reinterpret_cast<EventObserver*>(lpPayload[1]), lpPayload[0]);
            break;
        }
        case E_GUI_OUT_PRIORITY_STOP_BLOCKING:
        {
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            StopPriorityEventBlocking(reinterpret_cast<EventObserver*>(lpPayload[0]));
            break;
        }
        case E_GUI_OUT_REQUEST_RESOURCE:
        {
            OutputBuffer::GuiEventQueueSmall* lpResourceQueue = lpOutput->GetResourceEventQueue();
            CGS_ASSERT(lpResourceQueue != 0,
                       "Invalid output queue in EventInterpreterModule::ProcessOutEvents");
            lpResourceQueue->AddEvent(lpEvent, liEventType, liEventSize);
            break;
        }
        case E_GUI_OUT_EVENT:
        {
            OutputBuffer::GuiEventQueue* lpEventQueue = lpOutput->GetOutEventQueue();
            CGS_ASSERT(lpEventQueue != 0,
                       "Invalid output queue in EventInterpreterModule::ProcessOutEvents");
            // The boxed raw event lives at lpEvent + offset-word (payload[2]).
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            const CgsModule::Event* lpRawEvent = reinterpret_cast<const CgsModule::Event*>(
                reinterpret_cast<const u8*>(lpEvent) + lpPayload[2]);
            CGS_ASSERT(lpRawEvent != 0,
                       "Invalid raw event in EventInterpreterModule::ProcessOutEvents");
            lpEventQueue->AddEvent(lpRawEvent, lpPayload[1], lpPayload[0]);
            break;
        }
        case E_GUI_OUT_VIEW_STATE:
        {
            OutputBuffer::GuiEventQueueLarge* lpViewQueue = lpOutput->GetViewEventQueue();
            CGS_ASSERT(lpViewQueue != 0,
                       "Invalid output queue in EventInterpreterModule::ProcessOutEvents");
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            const CgsModule::Event* lpRawEvent = reinterpret_cast<const CgsModule::Event*>(
                reinterpret_cast<const u8*>(lpEvent) + lpPayload[2]);
            CGS_ASSERT(lpRawEvent != 0,
                       "Invalid raw event in EventInterpreterModule::ProcessOutEvents");
            lpViewQueue->AddEvent(lpRawEvent, lpPayload[1], lpPayload[0]);
            break;
        }
        case E_GUI_OUT_INTERNAL_EVENT:
        {
            const s32* lpPayload = reinterpret_cast<const s32*>(lpEvent);
            const CgsModule::Event* lpRawEvent = reinterpret_cast<const CgsModule::Event*>(
                reinterpret_cast<const u8*>(lpEvent) + lpPayload[2]);
            CGS_ASSERT(lpRawEvent != 0,
                       "Invalid raw event in EventInterpreterModule::ProcessOutEvents");
            mInternalEventQueue.AddEvent(lpRawEvent, lpPayload[1], lpPayload[0]);
            break;
        }
        default:
            if (lbIsPreWorldUpdate)
            {
                OutputBuffer::GuiEventQueue* lpEventQueue = lpOutput->GetOutEventQueue();
                CGS_ASSERT(lpEventQueue != 0,
                           "Invalid output queue in EventInterpreterModule::ProcessOutEvents");
                lpEventQueue->AddEvent(lpEvent, liEventType, liEventSize);
            }
            else
            {
                CGS_ASSERT(false,
                    "Unexpected output event type in EventInterpreterModule::ProcessOutEvents");
            }
            break;
        }

        liEventType = lStagingQueue.GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }

    lStagingQueue.Destruct();
}

}
