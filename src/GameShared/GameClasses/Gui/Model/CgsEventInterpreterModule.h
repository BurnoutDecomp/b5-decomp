#pragma once

// CgsGui::EventInterpreterModule - the GUI event router. A single-buffered module that
// owns a fixed pool of observer records (plain + priority) and a set of variable event
// queues, and shuttles GUI events between producers (the GUI flows/components, via the
// EventObserver state interfaces) and the outbound resource/view/event channels.
//
// SOURCE LADDER for this reconstruction:
//   1. BURNOUT_X360_ARTIST.XEX (authoritative behaviour + member offsets). The 11 X360
//      bodies that live in this TU are listed against their addresses in the .cpp.
//   2. DecFIGS DWARF (declaration shape): the nested-type member list / order, the two
//      stage enums, and the class member layout
//      (references/DecFIGS/dwarfdump/.../CgsEventInterpreterModule.h). KI_MAX_OBSERVERS==4
//      and KI_MAX_EVENTS_PER_OBSERVER==576 / BitArray<576u> are X360 values (CgsGuiEvent.h).
//   3. Feb-2007 partial source (style + inlined-helper shapes):
//      references/Feb-2007/.../Gui/Model/CgsEventInterpreterModule.h -- the nested observer
//      structs' inline Construct/Register/UnRegister/IsRegistered bodies are restored from it.
//
// NOTE ON THE TWO HOMES: the nested helper types sMapEntry / ObjectEventObserver and the
// prepare-stage operator++ also appear in a sibling partial header
// (GameShared/GameClasses/Gui/CgsEventInterpreterModule.h) that backs the bit-op-only TUs.
// This header is the canonical full-class home the DWARF cites
// (CgsEventInterpreterModule.h:53); it is self-contained and only #included by this module's
// own .cpp.

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::Event / VariableEventQueue
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Containers/CgsHashTable.h"          // CgsContainers::HashTable / HashTableElement
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                  // KI_MAX_OBSERVERS / KI_MAX_EVENTS_PER_OBSERVER, GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/CgsEventObserver.h"       // CgsGui::EventObserver (+ ProcessEvents virtual)
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.h" // EventInterpreterModuleIO::InputBuffer/OutputBuffer

namespace CgsGui
{
    // --- GUI event-type vocabulary used by this module -----------------------------------
    // X360-pinned values: the priority records publish E_GUI_PRIORITY_REMOVAL==2 /
    // E_GUI_PRIORITY_BLOCKING==3 (ProcessInEvents stores+AddEvent(type 2 size 4) for the
    // removal record, type 3 for the blocking record), and the outbound dispatch switch in
    // ProcessOutEvents is a jump table over E_GUI_OUT_REGISTER_FOR_EVENT(34)..
    // E_GUI_OUT_INTERNAL_EVENT(42). The intermediate names come from the Feb-2007 enum;
    // only the load-bearing ids are pinned explicitly.
    enum CgsEventType
    {
        E_GUI_INVALID                           = -1,
        E_GUI_REGISTER_EVENT                    = 0,
        // (one further register-class id sits here in the X360 build; PRIORITY_REMOVAL==2)
        E_GUI_PRIORITY_REMOVAL                  = 2,
        E_GUI_PRIORITY_BLOCKING                 = 3,
        E_GUI_ACTIVE_USER_INDEX                 = 4,

        // Outbound (E_GUI_OUT_*) request ids, X360-pinned by the ProcessOutEvents jump table.
        E_GUI_OUT_REGISTER_FOR_EVENT            = 34,
        E_GUI_OUT_UNREGISTER_FOR_EVENT          = 35,
        E_GUI_OUT_PRIORITY_REGISTER_FOR_EVENT   = 36,
        E_GUI_OUT_PRIORITY_UNREGISTER_FOR_EVENT = 37,
        E_GUI_OUT_PRIORITY_STOP_BLOCKING        = 38,
        E_GUI_OUT_REQUEST_RESOURCE              = 39,
        E_GUI_OUT_EVENT                         = 40,
        E_GUI_OUT_VIEW_STATE                    = 41,
        E_GUI_OUT_INTERNAL_EVENT                = 42,
    };

    // The priority-removal / priority-blocking on-queue records. Each is a GUI event header
    // followed by the single event id the observer wants removed / blocked (Feb-2007
    // CgsGuiEventTypeDefs.h). They are stored inline in each ObjectEventObserver and pushed
    // verbatim (size 4 payload) by ProcessInEvents.
    struct GuiEventPriorityRemoval : public GuiEvent<E_GUI_PRIORITY_REMOVAL>
    {
        s32 miRemovedEventId;
    };

    struct GuiEventPriorityBlocking : public GuiEvent<E_GUI_PRIORITY_BLOCKING>
    {
        s32 miBlockingEventId;
    };

    class EventInterpreterModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        typedef EventInterpreterModuleIO::InputBuffer::GuiEventInputQueue GuiEventInputQueue;
        typedef EventInterpreterModuleIO::OutputBuffer                    OutputBuffer;
        typedef EventInterpreterModuleIO::InputBuffer                     InputBuffer;

        // The interpreter's own per-frame working queues. The X360 sizes them at
        // VariableEventQueue<18432,16>: mInternalEventQueue is the spill queue for
        // E_GUI_OUT_INTERNAL_EVENT records, and each mInProcessQueues[i] (one per observer)
        // is the per-observer in-flight queue ProcessInEvents fills then hands the observer.
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;

        // CgsEventInterpreterModule.h:56 (DWARF). The stages Prepare() steps through.
        enum EventInterpreterPrepareStage
        {
            E_PREPARE_START   = 0,
            E_PREPARE_MANAGER = 1,
            E_PREPARE_DONE    = 2,
        };

        // CgsEventInterpreterModule.h:63 (DWARF). The stages Release() steps through.
        enum EventInterpreterReleaseStage
        {
            E_RELEASE_START   = 0,
            E_RELEASE_MANAGER = 1,
            E_RELEASE_DONE    = 2,
        };

        // ----- module virtuals (DWARF order) -------------------------------------------
        void Construct() override;
        bool Prepare() override;
        bool Release() override;
        void Destruct() override;

        virtual void PreWorldUpdate(InputBuffer* lpInput, OutputBuffer* lpOutput);
        virtual void Update(InputBuffer* lpInput, OutputBuffer* lpOutput);

        virtual void PreUpdateObservers();
        virtual void UpdateObservers();

        // ----- producer-facing API ------------------------------------------------------
        void AddGuiEvents(const GuiEventInputQueue& lrEventQueue);
        void AddGuiEvent(const CgsModule::Event* lpEvent, s32 liEventId, s32 liEventSize);

        bool RegisterForEvent(EventObserver* lpEventObserver, s32 liEventType);
        void UnRegisterForEvent(EventObserver* lpEventObserver, s32 liEventType);
        void UnRegisterForAllEvents(EventObserver* lpEventObserver);

        bool RegisterPriorityEvent(EventObserver* lpEventObserver, s32 liEventType,
                                   const s32* lpaEventOverrides, u32 luOverrideCount);
        bool StopPriorityEventBlocking(EventObserver* lpEventObserver);
        bool UnregisterPriorityEvent(EventObserver* lpEventObserver, s32 liEventType);

    private:
        // The override mask carried by a priority registration: which event ids this entry
        // suppresses while it is the active priority owner. @ 0x8284E870 (sMapEntry ctor).
        struct sMapEntry
        {
            CgsContainers::BitArray<KI_MAX_EVENTS_PER_OBSERVER> mOverriddenEventListBitArray;

            sMapEntry()
            {
                mOverriddenEventListBitArray.UnSetAll();
            }

            sMapEntry(const s32* lpaEventTypeOverrides, u32 luOverrideCount)
            {
                mOverriddenEventListBitArray.UnSetAll();
                for (u32 luIndex = 0; luIndex < luOverrideCount; ++luIndex)
                {
                    mOverriddenEventListBitArray.SetBit(
                        static_cast<u32>(lpaEventTypeOverrides[luIndex]));
                }
            }
        };

        // A plain observer slot: the observer plus the bitset of events it subscribes to,
        // plus the two inline priority records ProcessInEvents fills and re-publishes.
        struct ObjectEventObserver
        {
            EventObserver*                                      mpObserver;       // +0x00
            CgsContainers::BitArray<KI_MAX_EVENTS_PER_OBSERVER> mEventListBitArray; // +0x08

            GuiEventPriorityRemoval  mRemovedEvent;   // +0x58 (event id at +0x58)
            GuiEventPriorityBlocking mBlockingEvent;  // +0x5C (event id at +0x5C)

            void Construct()
            {
                mpObserver = 0;
                mEventListBitArray.UnSetAll();
            }

            void RegisterForEvent(s32 liEventType)
            {
                CGS_ASSERT((liEventType > E_GUI_INVALID) &&
                           (liEventType <= KI_MAX_EVENTS_PER_OBSERVER),
                           "Cannot register for invalid events");
                mEventListBitArray.SetBit(static_cast<u32>(liEventType));
            }

            void UnRegisterForEvent(s32 liEventType)
            {
                CGS_ASSERT((liEventType > E_GUI_INVALID) &&
                           (liEventType <= KI_MAX_EVENTS_PER_OBSERVER),
                           "Cannot register for invalid events");
                mEventListBitArray.UnSetBit(static_cast<u32>(liEventType));
            }

            bool IsRegisteredForEvent(s32 liEventType) const
            {
                CGS_ASSERT((liEventType > E_GUI_INVALID) &&
                           (liEventType <= KI_MAX_EVENTS_PER_OBSERVER),
                           "Cannot register for invalid events");
                return mEventListBitArray.IsBitSet(static_cast<u32>(liEventType));
            }
        };

        // A priority observer slot: like a plain slot but with a per-event override table and
        // a "blocking" flag. The override table is backed by an inline element pool.
        struct PriorityObjectEventObserver
        {
            static const u32 KU_MAX_HASH_ELEMENTS = 10;

            EventObserver*                                      mpObserver;          // +0x00
            CgsContainers::BitArray<KI_MAX_EVENTS_PER_OBSERVER> mEventListBitArray;  // +0x08
            CgsContainers::HashTable<s32, sMapEntry, 7>         mEventOverrideHashTable;
            bool                                                mbBlocking;
            CgsContainers::HashTableElement<s32, sMapEntry>     maHashElementPool[KU_MAX_HASH_ELEMENTS];
            u32                                                 muHashElementPoolCount;

            void Construct()
            {
                mpObserver = 0;
                mEventListBitArray.UnSetAll();
                mbBlocking = false;
                mEventOverrideHashTable.Init();
                muHashElementPoolCount = 0;
            }

            // The X360 dropped the Feb-2007 by-ref BitArray out-param; the lookup of the
            // override mask is folded into IsEventBlocked at the call site.
            bool IsRegisteredForEvent(s32 liEventType) const
            {
                CGS_ASSERT((liEventType > E_GUI_INVALID) &&
                           (liEventType <= KI_MAX_EVENTS_PER_OBSERVER),
                           "Cannot register for invalid events");
                return mEventListBitArray.IsBitSet(static_cast<u32>(liEventType));
            }
        };

        // ----- private helpers (X360 bodies in this TU) ---------------------------------
        void RemoveObjectFromObserverList(EventObserver* lpEventObserver);

        // Returns the index of the plain slot owning lpEventObserver (or -1 if not present)
        // and writes the slot pointer through lppFoundObserver.
        s32 FindObserverInEventObserverList(EventObserver* lpEventObserver,
                                            ObjectEventObserver** lppFoundObserver);
        s32 FindPriorityObserverInEventObserverList(EventObserver* lpEventObserver,
                                                    PriorityObjectEventObserver** lppFoundObserver);

        void ProcessInEvents(GuiEventInputQueue* lpCurrentEvents);
        void ProcessOutEvents(OutputBuffer* lpOutput, bool lbIsPreWorldUpdate);

        // True iff some plain observer owns liEvent as a priority event; on a hit, the owning
        // slot's observer pointer is written through lppOwnerObserver (via FindObserver...).
        bool IsPriorityEvent(s32 liEvent, ObjectEventObserver** lppOwnerObserver);

        void SetBlocking(const ObjectEventObserver* lpObjectEventObserver);

        // True iff liEvent is currently blocked by some blocking priority observer; on a hit
        // the blocking observer pointer is written through lrpBlockingEventObserver.
        bool IsEventBlocked(s32 liEvent, EventObserver*& lrpBlockingEventObserver) const;

        // ----- members (DWARF layout; X360 offsets in comments) -------------------------
        EventInterpreterPrepareStage mePrepareStage;   // +0x228
        EventInterpreterReleaseStage meReleaseStage;   // +0x22C

        s32                 mnNumberEventObservers;                       // +0x230
        ObjectEventObserver maEventObservers[KI_MAX_OBSERVERS];           // +0x238 (stride 0x60)

        s32                         mnNumberPriorityEventObservers;       // +0x3B8
        PriorityObjectEventObserver maPriorityEventObservers[KI_MAX_OBSERVERS]; // +0x3C0 (stride 0x480)

        GuiEventQueue mInternalEventQueue;                  // +0x15C0
        GuiEventQueue mInProcessQueues[KI_MAX_OBSERVERS];   // +0x5DD0 (stride 0x4810)
    };

    // @ 0x828470D0 (CgsEventInterpreterModule.h:303). Post-increment the prepare-stage index:
    // capture the current value, advance the reference by one, assert it has not overrun
    // E_PREPARE_DONE, and return the value held before the increment.
    EventInterpreterModule::EventInterpreterPrepareStage operator++(
        EventInterpreterModule::EventInterpreterPrepareStage& lrStage, int);

    // @ 0x82847130 (CgsEventInterpreterModule.h:304). The release-stage counterpart.
    EventInterpreterModule::EventInterpreterReleaseStage operator++(
        EventInterpreterModule::EventInterpreterReleaseStage& lrStage, int);
}
