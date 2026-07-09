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

    // GuiEventWrapper<T, leEventType> (DecFIGS CgsGuiEvent.h:77). The stack record the GUI
    // output path (CgsGui::StateInterface::OutputGuiEvent<T> and its sibling OutputViewState /
    // OutputInternalState channels) builds and hands to VariableEventQueue::AddEvent. It boxes an
    // arbitrary payload T, recording the payload's byte size (miOutEventSize), its own
    // GetEventType() (miOutEventType) and where it sits relative to the record (miOutEventOffset).
    //
    // LAYOUT -- from the X360 ARTIST asm (rung 1, authoritative). Every OutputGuiEvent<T> body
    // (e.g. @0x824367D8 OutputGuiEvent<GuiAudioEvent>) stack-builds
    //     +0x00  miOutEventSize   (s32) = sizeof(T)
    //     +0x04  miOutEventType   (s32) = T::GetEventType()
    //     +0x08  miOutEventOffset (s32) = offsetof(this, mOutEvent)
    //     +0x0C  T  mOutEvent            (aligned to alignof(T): +0x0C for align<=4, +0x10 for align 8)
    // then calls AddEvent(&record, /*channel*/ leEventType, /*size*/ sizeof(record)). There is NO
    // GuiEvent<leEventType> base in front of miOutEventSize: the asm's offset word is 12 for an
    // align<=4 payload and 16 for an align-8 payload (GuiAudioEvent @0x824367D8), which is exactly
    // offsetof(mOutEvent) for this 3-word header -- a GuiEvent<N> base (12 bytes) would force
    // offset 24. AddEvent writes its own CBufferEntry{id,size} queue header from the channel arg;
    // it does not prepend a GuiEvent header. The leEventType template arg is the AddEvent channel
    // (40 = OutputGuiEvent, 41 = OutputViewState, 42 = OutputInternalState) and names the record
    // type; it is not a stored field. Wrapper instances seen in the DWARF include
    // GuiEventWrapper<CgsGui::GuiEventPlayAptMovie,41>, GuiEventWrapper<BrnGui::GuiChallengeSelectedEvent,40>,
    // GuiEventWrapper<BrnGui::GuiAudioEvent,40>, GuiEventWrapper<CgsGui::GuiEvent<155>,40>, etc.
    template <class T, s32 leEventType>
    class GuiEventWrapper
    {
    public:
        s32 miOutEventSize;    // +0x00  sizeof(T)
        s32 miOutEventType;    // +0x04  T::GetEventType()
        s32 miOutEventOffset;  // +0x08  offsetof(mOutEvent)
        T   mOutEvent;         // +0x0C  payload (aligned to alignof(T))

        // Box the payload: copy it in and capture its size / type / record-relative offset. The
        // X360 body copies the event bytes into the record then writes the three header words; this
        // copy-constructs mOutEvent (so payloads without a default constructor still work -- the asm
        // never default-constructs the payload) and fills the header. miOutEventOffset is the
        // "offsetof via a 1-based dummy" idiom the X360 build emits for the offset word.
        explicit GuiEventWrapper(T& lrEvent)
            : miOutEventSize(static_cast<s32>(sizeof(T)))
            , miOutEventType(lrEvent.GetEventType())
            , miOutEventOffset(static_cast<s32>(reinterpret_cast<size_t>(
                  reinterpret_cast<char*>(&(reinterpret_cast<GuiEventWrapper*>(1)->mOutEvent)) - 1)))
            , mOutEvent(lrEvent)
        {
        }

        // The AddEvent channel this record is queued under (leEventType); names the record type.
        s32 GetChannel() const { return leEventType; }

        // The boxed payload lives miOutEventOffset bytes past the record's own address.
        const CgsModule::Event* GetRawEvent() const
        {
            return reinterpret_cast<const CgsModule::Event*>(
                reinterpret_cast<const unsigned char*>(this) + miOutEventOffset);
        }
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

    // GuiEventLoadRequest (DWARF CgsGuiEventTypes.h: GuiEventLoadRequest is the
    // GuiEvent<39>-based load-request event). The X360 GUI model input buffer pushes it
    // onto its small load-request queue with CgsModule::VariableEventQueue<4096,16>::AddEvent(
    // &request, /*type*/39, /*size*/24) (see CgsGui::ModelIO::InputBuffer::AddResourceRequests
    // @0x8250C658): so the event-type id is 39 and the on-queue record is 24 bytes. The
    // GuiEvent<39> base supplies the 12-byte event header (muHeader0/muEventType/muHeader2);
    // the remaining 12 bytes are the request payload. The payload's exact field breakdown is
    // not recovered from the available DWARF, so it is modelled as an opaque 12-byte span here
    // (honest placeholder -- the 24-byte total is the X360 fact; the inner field names are not).
    struct GuiEventLoadRequest : public GuiEvent<39>
    {
        u8 maRequestPayload[12]; // +12..+23 (opaque; total record size 24, X360 AddEvent size arg)
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
