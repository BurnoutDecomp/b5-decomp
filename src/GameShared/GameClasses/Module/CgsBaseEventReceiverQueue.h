#pragma once

#include "types.hpp"

// CgsModule::BaseEventReceiverQueue -- a per-frame event receiver queue: a linear (drain-then-Clear)
// buffer of variable-size event records over an external backing buffer. Each record is
// [s32 type][s32 size][size bytes of payload][pad to alignment]. The base holds the ring state; the
// derived EventReceiverQueue<BUFSIZE,ALIGN> supplies the embedded backing buffer.
//
// Reconstructed from the X360: AddEvent 0x82663928, GetNextEvent 0x82204690, Clear 0x821F1D50
// (CgsEventReceiverQueue.h). Field mapping recovered from those bodies (the previous in-tree names
// were placeholders): [0] buffer base, [1] write offset, [2] count, [3] start offset, [4] capacity,
// [5] alignment. The base pointer is x64-width here (the X360 stored a 32-bit address).
namespace CgsModule
{
    struct Event;   // the shared event base (defined in CgsVariableEventQueue.h); used opaquely (by pointer)

    class BaseEventReceiverQueue
    {
    public:
        // Bind the backing buffer + sizing, then Clear to the empty (aligned) state. Called by the
        // derived EventReceiverQueue<BUFSIZE,ALIGN>::Construct.
        void Construct(u8* lpBuffer, s32 liCapacity, s32 liAlignment)
        {
            mpBuffer    = lpBuffer;
            miCapacity  = liCapacity;
            miAlignment = liAlignment;
            Clear();
        }

        // @ 0x821F1D50 - reset to empty: count 0; start/write offset = the alignment-adjusted base so
        // the first event's payload lands aligned.
        BaseEventReceiverQueue* Clear();

        // @ 0x82663928 - append an event (copies liSize payload bytes); record = [type][size][payload][pad].
        bool AddEvent(const Event* lpEvent, s32 liType, s32 liSize);

        // Read the first queued event (X360 ProcessReceiverQueue inlines this at miStartOffset).
        s32 GetFirstEvent(const Event** lppEventData, s32* lpiSize) const;

        // @ 0x82204690 - given the previous event's payload pointer, return the next event (-1 at end).
        s32 GetNextEvent(const Event* lpPrevData, const Event** lppNextData, s32* lpiSize) const;

        s32 GetCount() const { return miCount; }

        // ADDITIVE GROW (ColourCalibrationScreen::Update @0x8246AA28, whose baked assert
        // text "mReceiverQueue.GetLength() == 1" attests the original accessor NAME):
        // the DWARF-era spelling of the count read.
        s32 GetLength() const { return miCount; }

    protected:
        u8*  mpBuffer;        // [0] backing buffer base address (set by Construct)
        s32  miWriteOffset;   // [1] current write byte-offset (also the drain end)
        s32  miCount;         // [2] queued event count
        s32  miStartOffset;   // [3] first-event byte-offset (set by Clear)
        s32  miCapacity;      // [4] backing buffer size in bytes
        s32  miAlignment;     // [5] event record alignment
    };

    // EventReceiverQueue<BUFSIZE,ALIGN> -- the base plus its embedded backing buffer. The X360 emits
    // each instantiation; the generic body is inline here (matches the GuiShared.h forward decl
    // `template <s32,s32> class EventReceiverQueue;`).
    template <s32 BUFSIZE, s32 ALIGN>
    class EventReceiverQueue : public BaseEventReceiverQueue
    {
    public:
        void Construct() { BaseEventReceiverQueue::Construct(maBuffer, BUFSIZE, ALIGN); }

    private:
        u8 maBuffer[BUFSIZE];
    };
}
