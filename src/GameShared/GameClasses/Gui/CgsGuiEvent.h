#pragma once

#include "types.hpp"
#include <cstddef>   // offsetof (GuiEventWrapper::SetRawEvent)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// GUI event vocabulary recovered from the DecFIGS DWARF (CgsGuiEvent.h):
//   - GuiEvent<N>          : the base of every concrete GUI event; N is the
//                            compile-time event-type id.
//   - GuiEventQueueBase<N,A>: a VariableEventQueue specialised for GUI events.
//   - GuiStackEventQueue    : owns one large GUI queue (the StateInterface output).
namespace CgsGui
{
    // Queue capacities (bytes) and the record alignment, from the X360 build.
    const s32 KI_INPUT_QUEUE_SIZE_KB  = 32768;
    const s32 KI_LARGE_QUEUE_SIZE_KB  = 65536;
    const s32 KI_MEDIUM_QUEUE_SIZE_KB = 16384;
    const s32 KI_SMALL_QUEUE_SIZE_KB  = 4096;
    const s32 KI_TINY_QUEUE_SIZE_KB   = 256;
    const s32 KI_QUEUE_ALIGNMENT      = 16;

    // Observer-fanout caps (DecFIGS CgsGuiEvent.h:41-42). The GUI event router holds at
    // most KI_MAX_OBSERVERS observer slots and queues at most KI_MAX_EVENTS_PER_OBSERVER
    // events per observer per frame. Declared additively alongside the queue-size set.
    const s32 KI_MAX_OBSERVERS           = 4;
    const s32 KI_MAX_EVENTS_PER_OBSERVER = 600; // ARTIST8284EA60/EAC8, not DecFIGS576

    // GuiEvent<N> (DecFIGS CgsGuiEvent.h:58): the base of every GUI event. It derives from the
    // empty CgsModule::Event and has no data; its one member, GetEventType() const (h:265),
    // returns N. A bare tag is therefore one unwritten byte and a derived event's record is its
    // payload alone: ARTIST's GuiModule::AddGuiEvent<T> instances (0x823CEB70..0x823DAE68)
    // queue sizeof(payload), e.g. 0x823D2660 AddGuiEvent<GuiEvent<91>> size 1, the Payback
    // tags 178/180/183 (0x823D7570/0x823D7628/0x823D2FB8) and PFX 497/500 (0x8250D828/
    // 0x823CEFA4) size 1. The {size, type, offset} prefix of the GUI output channels is
    // GuiEventWrapper's (below), never the event's.
    template <s32 EventTypeId>
    class GuiEvent : public CgsModule::Event
    {
    public:
        s32 GetEventType() const { return EventTypeId; }
    };

    // GuiEventWrapper<T, N> (DecFIGS CgsGuiEvent.h:77): the record the GUI output channels queue,
    // built on the stack by the StateInterface publishers through the channel subclasses of
    // CgsGuiStateInterface.h (GuiEventOut = 40, GuiOutViewState = 41, GuiOutInternalEvent = 42).
    // It derives from the empty GuiEvent<N>, so its GetEventType() is the channel, and boxes the
    // payload after three words:
    //   +0x00 miOutEventSize   (h:80)           sizeof(T)
    //   +0x04 miOutEventType   (h:81)           T::GetEventType()
    //   +0x08 miOutEventOffset (h:93, private)  offset of mOutEvent: 12, or 16 for an align-8 T
    //   +0x0C mOutEvent        (h:94, private)  (ARTIST 0x824367D8 <GuiAudioEvent>: offset 16)
    // DWARF declares no constructor, only SetRawEvent (h:217) and GetRawEvent (h:238).
    template <class T, s32 leEventType>
    struct GuiEventWrapper : public GuiEvent<leEventType>
    {
        s32 miOutEventSize;
        s32 miOutEventType;

    private:
        s32 miOutEventOffset;
        T   mOutEvent;

    public:
        const CgsModule::Event* GetRawEvent() const;
        void SetRawEvent(const T& lEvent);   // DWARF h:217: const T&
    };

    // DecFIGS CgsGuiEvent.h:217; PS3 lines 221..224 inline in every publisher (e.g.
    // LargeCarComponent::ReleaseResources 0x3D8740..0x3D8778): the payload copy, then the offset,
    // size and type words. ARTIST stores them in that order too (OutputGuiEvent
    // <GuiEventRequestTraining> 0x82436840: payload, 0xC, 4, 0x23C).
    // The offset is the compile-time offset of mOutEvent, not a runtime address difference: ARTIST
    // gives two wrappers inlined into one publisher the same stack slot (e.g. CarSelectMain::OnEnter
    // 0x824C8920 builds both records at r1+0x50; 126 GUI functions do this), which the compiler
    // only does when the wrapper's address is not taken inside SetRawEvent.
    template <class T, s32 leEventType>
    void GuiEventWrapper<T, leEventType>::SetRawEvent(const T& lEvent)
    {
        mOutEvent        = lEvent;
        miOutEventOffset = static_cast<s32>(offsetof(GuiEventWrapper, mOutEvent));
        miOutEventSize   = static_cast<s32>(sizeof(T));
        miOutEventType   = lEvent.GetEventType();
    }

    // DecFIGS CgsGuiEvent.h:238: the boxed payload, miOutEventOffset bytes into the record.
    template <class T, s32 leEventType>
    const CgsModule::Event* GuiEventWrapper<T, leEventType>::GetRawEvent() const
    {
        return reinterpret_cast<const CgsModule::Event*>(reinterpret_cast<const u8*>(this) +
                                                         miOutEventOffset);
    }

    // GuiEventQueueBase adds no storage and no behaviour over VariableEventQueue<N,A>:
    // every member is a header-inline forwarder (DecFIGS CgsGuiEvent.h:281..434). ARTIST
    // has no GuiEventQueueBase function at all; its callers call the VariableEventQueue
    // body directly (e.g. StateLoadingHelper::Construct @0x824FD9D0 ->
    // VariableEventQueue<4096,16>::Construct, CustomRendererManager::Construct).
    template <s32 BUFSIZE, s32 ALIGN>
    class GuiEventQueueBase : public CgsModule::VariableEventQueue<BUFSIZE, ALIGN>
    {
    public:
        void Construct()
        {
            CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::Construct();
        }
        bool Prepare()
        {
            return CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::Prepare();
        }
        bool Release()
        {
            return CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::Release();
        }
        void Destruct()
        {
            CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::Destruct();
        }
        void Clear()
        {
            CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::Clear();
        }
        s32 GetLength() const
        {
            return CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::GetLength();
        }
        // DecFIGS CgsGuiEvent.h:378 (body h:380): queue one typed GUI event. ARTIST inlines it
        // into every CgsGui::GuiModule::AddGuiEvent<T> instance (PS3 DecFIGS inlining: the
        // AddEvent call of 0x34B9C8 comes from CgsGuiEvent.h:380 inside CgsGuiModule.h:284..288;
        // ARTIST e.g. 0x823D4490 AddEvent(&event, 403, 32)).
        template <class T>
        void AddGuiEvent(const T& lEvent)
        {
            // The tree's flat payload records do not all derive from GuiEvent<N> (an empty
            // CgsModule::Event) yet, so the record pointer is converted explicitly.
            CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), lEvent.GetEventType(), sizeof(T));
        }
        void AddGuiEvent(const CgsModule::Event* lpEvent, s32 liType, s32 liSize);
        s32 GetFirstEvent(const CgsModule::Event** lppEvent, s32* lpiSize) const
        {
            return CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::GetFirstEvent(lppEvent, lpiSize);
        }
        s32 GetNextEvent(const CgsModule::Event* lpEvent, const CgsModule::Event** lppNextEvent, s32* lpiSize) const
        {
            return CgsModule::VariableEventQueue<BUFSIZE, ALIGN>::GetNextEvent(lpEvent, lppNextEvent, lpiSize);
        }
    };

    // Named GUI event-queue sizes (bytes), from the X360 queue-size constants.
    typedef GuiEventQueueBase<65536, 16> GuiEventQueueLarge;
    typedef GuiEventQueueBase<16384, 16> GuiEventQueueMedium;
    typedef GuiEventQueueBase<4096, 16>  GuiEventQueueSmall;
    typedef GuiEventQueueBase<256, 16>   GuiEventQueueTiny;

    // DecFIGS CgsGuiEvent.h:189 -- the unqualified "GuiEventQueue" name aliases the medium
    // (16384) queue (matching the GuiEventQueueLarge/Small naming, this is the default).
    typedef GuiEventQueueBase<16384, 16> GuiEventQueue;

    // The original GuiStackEventQueue derives from CgsModule::IOBuffer; the in-scope
    // GUI-state code only uses the nested GuiEventQueueLarge typedef, so the IOBuffer
    // base is intentionally omitted here to avoid a transitive module-IO cascade.
    struct GuiStackEventQueue
    {
        typedef CgsGui::GuiEventQueueLarge GuiEventQueueLarge;

        GuiEventQueueLarge mGuiEventQueue;
    };
}

// Complete the existing GUI-state queue spelling. The Interpreter owns these
// actual queue objects, and EventObserver/BrnBaseFlow pass them by this type.
// ARTIST uses the18432-byte specialization; this wrapper adds no fields.
namespace InputBuffer
{
    class GuiEventQueue : public CgsModule::VariableEventQueue<18432, 16> {};
}
