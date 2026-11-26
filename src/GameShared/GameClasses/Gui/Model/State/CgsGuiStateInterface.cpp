#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsGui::StateInterface is the
// outward channel a GUI state drives: accessors over its shared access-pointers and
// allocator, plus the event emitters that push records onto its large output queue.
// Behaviour-faithful to the X360 pseudocode; the event byte sizes the X360 passed
// to AddEvent (20 / 16 / 24) are noted inline. The queue's AddEvent body is its own
// (out-of-line) ledger TU, resolved at link time.

namespace CgsGui
{
    // @ 0x8240E4E0 - reads the metric-units flag off the language manager reached
    // through the access pointers.
    bool StateInterface::IsUsingMetricUnits()
    {
        CGS_ASSERT_AT(GetAccessPointers()->mpLanguageManager != 0,
                      "GetAccessPointers()->mpLanguageManager != NULL", CGS_GUI_STATE_INTERFACE_H_FILE, 358);
        return GetAccessPointers()->mpLanguageManager->IsUsingMetricUnits();
    }

    // Stand up the interface: zero the channel pointers + construct the output event queue. (Cast to the
    // VariableEventQueue base so the inline Construct is used, not the GuiEventQueueBase template decl.)
    void StateInterface::Construct()
    {
        mpObserver = 0;
        mpAccessPointers = 0;
        mpAllocator = 0;
        static_cast<CgsModule::VariableEventQueue<65536, 16>&>(mOutEventQueue).Construct();
    }

    // Install the shared allocator + access-pointer bundle the interface hands its
    // states (GetAccessPointers / GetAllocator read them back). The X360 GUI stack
    // machinery prepares each pushed state's interface with the module's shared pair.
    void StateInterface::Prepare(rw::IResourceAllocator* lpAllocator, GuiAccessPointers* lpAccessPointers)
    {
        mpAllocator      = lpAllocator;
        mpAccessPointers = lpAccessPointers;
    }

    // Registration payloads use the canonical flat types in the header.
    // sizeof preserves full host pointers; native sizes remain8/2412.
    // @ 0x8285B8A0 - emit one type-34 "register for event" record per event id.
    void StateInterface::RegisterForEvents(const s32* lpiEventIds, s32 liCount)
    {
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            GuiEventRegisterForEvents lRecord;
            lRecord.lpEventObserver = mpObserver;
            lRecord.miEventType     = lpiEventIds[liIndex];
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                    34, static_cast<s32>(sizeof(lRecord)));
        }
    }

    // @ 0x8285B900 - emit one type-35 "unregister for event" record per event id.
    void StateInterface::UnRegisterForEvents(const s32* lpiEventIds, s32 liCount)
    {
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            GuiEventUnRegisterForEvents lRecord;
            lRecord.lpEventObserver = mpObserver;
            lRecord.miEventType     = lpiEventIds[liIndex];
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                    35, static_cast<s32>(sizeof(lRecord)));
        }
    }

    // @ 0x8285B960 - emit a type-36 "priority register" record: the event id, the
    // inline override-event list (luCount entries copied into a fixed 600-int slot),
    // the override count, then the observer. The X360 record is 2412 bytes: 4 (event)
    // + 2400 (override list) + 4 (count) + 4 (observer); only luCount*4 of the override
    // bytes are initialised before the copy (the tail is whatever the stack held), which
    // is faithful to the asm (it memcpy's 4*luCount into the slot and sends all 2412).
    void StateInterface::PriorityRegisterForEvent(s32 liPriority, const s32* lpiEventIds, u32 luCount)
    {
        GuiEventPriorityRegisterForEvents lRecord;
        lRecord.miEventType     = liPriority;
        lRecord.muOverrideCount = luCount;
        lRecord.lpEventObserver = mpObserver;
        memcpy(lRecord.miEventTypeOverridden, lpiEventIds, 4u * luCount);
        mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                36, static_cast<s32>(sizeof(lRecord)));
    }

    // @ 0x8285B9C0 - emit a type-37 "priority unregister" record { eventId, observer },
    // size 8.
    void StateInterface::PriorityUnRegisterForEvent(s32 liPriority)
    {
        GuiEventPriorityUnRegisterForEvents lRecord;
        lRecord.miEventType     = liPriority;
        lRecord.lpEventObserver = mpObserver;
        mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                37, static_cast<s32>(sizeof(lRecord)));
    }
}

