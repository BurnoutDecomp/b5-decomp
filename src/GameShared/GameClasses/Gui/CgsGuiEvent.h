#pragma once

#include "types.hpp"
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
    const s32 KI_MAX_EVENTS_PER_OBSERVER = 576;

    // Base of every GUI event. The X360 emitters fill a small fixed header before
    // pushing the record onto a queue: muHeader0 / muHeader2 are queue bookkeeping
    // words (their precise meaning is not recovered from the leak), and muEventType
    // carries the GuiEvent<N> template id. EventTypeId is the compile-time id.
    template <s32 EventTypeId>
    struct GuiEvent : public CgsModule::Event
    {
        u32 muHeader0;
        u32 muEventType;
        u32 muHeader2;

        explicit GuiEvent(u32 luHeader0 = 0, u32 luHeader2 = 0)
            : muHeader0(luHeader0)
            , muEventType(static_cast<u32>(EventTypeId))
            , muHeader2(luHeader2)
        {
        }

        s32 GetEventType() const { return EventTypeId; }
    };

    // GuiEventWrapper<T, leEventType> (DecFIGS CgsGuiEvent.h:77). A GuiEvent<leEventType>
    // that boxes an arbitrary payload T, recording where the payload sits relative to the
    // wrapper (miOutEventOffset), its byte size (miOutEventSize) and its own GetEventType()
    // (miOutEventType). The GUI output path stores the wrapper by value on a queue and later
    // hands observers the inner T back via GetRawEvent(). Layout from the DWARF
    // (CgsGuiEvent.h:80/81/93/94): two public bookkeeping words, then the private offset word,
    // then the payload, all after the GuiEvent<leEventType> 12-byte base:
    //     base GuiEvent<leEventType>   (+0x00 .. +0x0B: muHeader0/muEventType/muHeader2)
    //     +0x0C  miOutEventSize   (s32)
    //     +0x10  miOutEventType   (s32)
    //     +0x14  miOutEventOffset (s32)
    //     +0x18  T  mOutEvent
    // The wrapper instances seen in the X360 build (DWARF) include
    // GuiEventWrapper<CgsGui::GuiEventPlayAptMovie,41>, GuiEventWrapper<BrnGui::GuiChallengeSelectedEvent,40>,
    // GuiEventWrapper<BrnGui::GuiAudioEvent,40>, GuiEventWrapper<CgsGui::GuiEvent<155>,40>, etc.
    template <class T, s32 leEventType>
    class GuiEventWrapper : public GuiEvent<leEventType>
    {
    public:
        s32 miOutEventSize;
        s32 miOutEventType;

        // DWARF CgsGuiEvent.h:238 -- the boxed payload lives miOutEventOffset bytes past
        // the wrapper's own address; reinterpret it back to the queued Event base.
        const CgsModule::Event* GetRawEvent() const
        {
            return reinterpret_cast<const CgsModule::Event*>(
                reinterpret_cast<const unsigned char*>(this) + miOutEventOffset);
        }

        // DWARF CgsGuiEvent.h:217 -- copy the payload in and capture its size/type plus the
        // wrapper-relative offset of mOutEvent (the standard "offsetof via a 1-based dummy"
        // idiom the X360 build emits for the offset word).
        void SetRawEvent(T& lrEvent)
        {
            typedef CgsGui::GuiEventWrapper<T, leEventType> WrappedEventType;

            mOutEvent        = lrEvent;
            miOutEventOffset = static_cast<s32>(reinterpret_cast<size_t>(
                reinterpret_cast<char*>(&(reinterpret_cast<WrappedEventType*>(1)->mOutEvent)) - 1));
            miOutEventSize   = static_cast<s32>(sizeof(T));
            miOutEventType   = lrEvent.GetEventType();
        }

    private:
        s32 miOutEventOffset;
        T   mOutEvent;
    };

    template <s32 BUFSIZE, s32 ALIGN>
    class GuiEventQueueBase : public CgsModule::VariableEventQueue<BUFSIZE, ALIGN>
    {
    public:
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void Clear();
        s32  GetLength() const;
        void AddGuiEvent(const CgsModule::Event* lpEvent, s32 liType, s32 liSize);
        s32  GetFirstEvent(const CgsModule::Event** lppEvent, s32* lpiSize) const;
        s32  GetNextEvent(const CgsModule::Event* lpEvent, const CgsModule::Event** lppNextEvent, s32* lpiSize) const;
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
