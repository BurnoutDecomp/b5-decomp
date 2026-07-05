#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsGui::StateInterface is the
// outward channel a GUI state drives: accessors over its shared access-pointers and
// allocator, plus the event emitters that push records onto its large output queue.
// Behaviour-faithful to the X360 pseudocode; the event byte sizes the X360 passed
// to AddEvent (20 / 16 / 24) are noted inline. The queue's AddEvent body is its own
// (out-of-line) ledger TU, resolved at link time.

namespace CgsGui
{
    // @ 0x8240E3E0
    GuiAccessPointers* StateInterface::GetAccessPointers()
    {
        CGS_ASSERT(mpAccessPointers != nullptr, "mpAccessPointers != NULL");
        return mpAccessPointers;
    }

    // @ 0x8240E4E0 - reads the metric-units flag off the language manager reached
    // through the access pointers.
    bool StateInterface::IsUsingMetricUnits()
    {
        CGS_ASSERT(mpAccessPointers != nullptr, "mpAccessPointers != NULL");
        CGS_ASSERT(GetAccessPointers()->mpLanguageManager != nullptr,
                   "GetAccessPointers()->mpLanguageManager != NULL");
        return GetAccessPointers()->mpLanguageManager->IsUsingMetricUnits();
    }

    // @ 0x82436F10 - queue a "play apt movie" view-state event (type 18, channel 41).
    void StateInterface::PlayAptMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        GuiEventPlayAptMovie lEvent;
        lEvent.mpacMovieName = lpacMovieName;
        lEvent.miLevelNum    = liLevelNum;
        mOutEventQueue.AddEvent(&lEvent, 41, sizeof(lEvent));   // X360 size 20
    }

    // @ 0x82476F98 - queue a "play apt loading movie" event (type 19, channel 40).
    void StateInterface::PlayLoadingScreen()
    {
        GuiEventPlayAptLoadingMovie lEvent;
        mOutEventQueue.AddEvent(&lEvent, 40, sizeof(lEvent));   // X360 size 16
    }

    // @ 0x82476FE0 - queue a "stop apt loading movie" event (type 20, channel 40).
    void StateInterface::StopLoadingScreen()
    {
        GuiEventStopAptLoadingMovie lEvent;
        mOutEventQueue.AddEvent(&lEvent, 40, sizeof(lEvent));   // X360 size 16
    }

    // @ 0x82436E40 - queue a resource load/unload request (channel 39).
    void StateInterface::RequestResource(const char* lpacFileName, ResourceRequestTypes leType,
                                         s32 liUserData, ResourceRequestLoadUnload leLoadUnload)
    {
        CGS_ASSERT(lpacFileName != nullptr, "Invalid file to load in StateInterface::RequestResource");

        GuiEventRequestResource lEvent;
        lEvent.meType       = leType;
        lEvent.meLoadUnload = leLoadUnload;
        lEvent.mpacFileName = lpacFileName;
        lEvent.miUserData   = liUserData;
        mOutEventQueue.AddEvent(&lEvent, 39, sizeof(lEvent));   // X360 size 24
    }

    // @ 0x82846F90 - install the event observer (the X360 asserts on a null pointer).
    void StateInterface::SetEventObserver(CgsGui::EventObserver* lpObserver)
    {
        CGS_ASSERT(lpObserver != nullptr,
                   "Invalid event observer pointer in StateInterface::SetEventObserver");
        mpObserver = lpObserver;
    }

    // Stand up the interface: zero the channel pointers + construct the output event queue. (Cast to the
    // VariableEventQueue base so the inline Construct is used, not the GuiEventQueueBase template decl.)
    void StateInterface::Construct()
    {
        mpObserver = nullptr;
        mpAccessPointers = nullptr;
        mpAllocator = nullptr;
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

    GuiStackEventQueue::GuiEventQueueLarge* StateInterface::GetOutputEventQueue()
    {
        return &mOutEventQueue;
    }

    // The X360 register/unregister emitters build a small fixed 8-byte record
    // { s32 miEventType; EventObserver* lpEventObserver; } per event id and push it
    // onto the output queue with the matching type tag. The DWARF declares these as
    // GuiEvent<N>-derived structs (GuiEventRegisterForEvents=34, UnRegister=35), but
    // the asm writes only the 8-byte {eventType, observer} payload AddEvent copies,
    // so the record is reconstructed at that exact width (size 8). The observer the
    // record carries is mpObserver (the interface's first member, read as field_0).
    namespace
    {
        struct RegisterEventRecord
        {
            s32                    miEventType;
            CgsGui::EventObserver* mpEventObserver;
        };

        // X360 wire-record byte sizes the asm passes to AddEvent. The on-target
        // pointer is 32-bit, so the {s32, observer*} record is 8 bytes and the
        // priority-register record is 4 + 2400 + 4 + 4 = 2412. On a 64-bit host the
        // C++ struct is wider (the trailing observer pointer); the X360 size is the
        // authoritative record width, so it is passed explicitly rather than via
        // host sizeof.
        const s32 KI_REGISTER_RECORD_SIZE          = 8;
        const s32 KI_PRIORITY_REGISTER_RECORD_SIZE = 2412;
    }

    // @ 0x8285B8A0 - emit one type-34 "register for event" record per event id.
    void StateInterface::RegisterForEvents(const s32* lpiEventIds, s32 liCount)
    {
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            RegisterEventRecord lRecord;
            lRecord.miEventType     = lpiEventIds[liIndex];
            lRecord.mpEventObserver = mpObserver;
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                    34, KI_REGISTER_RECORD_SIZE);
        }
    }

    // @ 0x8285B900 - emit one type-35 "unregister for event" record per event id.
    void StateInterface::UnRegisterForEvents(const s32* lpiEventIds, s32 liCount)
    {
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            RegisterEventRecord lRecord;
            lRecord.miEventType     = lpiEventIds[liIndex];
            lRecord.mpEventObserver = mpObserver;
            mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                    35, KI_REGISTER_RECORD_SIZE);
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
        struct PriorityRegisterRecord
        {
            s32                    miEventType;
            s32                    maiEventTypeOverridden[600];
            u32                    muOverrideCount;
            CgsGui::EventObserver* mpEventObserver;
        };

        PriorityRegisterRecord lRecord;
        lRecord.miEventType     = liPriority;
        lRecord.muOverrideCount = luCount;
        lRecord.mpEventObserver = mpObserver;
        memcpy(lRecord.maiEventTypeOverridden, lpiEventIds, 4u * luCount);
        mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                36, KI_PRIORITY_REGISTER_RECORD_SIZE);
    }

    // @ 0x8285B9C0 - emit a type-37 "priority unregister" record { eventId, observer },
    // size 8.
    void StateInterface::PriorityUnRegisterForEvent(s32 liPriority)
    {
        RegisterEventRecord lRecord;
        lRecord.miEventType     = liPriority;
        lRecord.mpEventObserver = mpObserver;
        mOutEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                37, KI_REGISTER_RECORD_SIZE);
    }
}
