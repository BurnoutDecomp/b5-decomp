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

    // The original GuiStackEventQueue derives from CgsModule::IOBuffer; the in-scope
    // GUI-state code only uses the nested GuiEventQueueLarge typedef, so the IOBuffer
    // base is intentionally omitted here to avoid a transitive module-IO cascade.
    struct GuiStackEventQueue
    {
        typedef CgsGui::GuiEventQueueLarge GuiEventQueueLarge;

        GuiEventQueueLarge mGuiEventQueue;
    };
}
