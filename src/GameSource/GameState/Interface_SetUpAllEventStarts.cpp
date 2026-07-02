#include "GameSource/GameState/BrnGameStateSharedIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface
//
// The per-mode "set up all event starts" interface: a fixed Array<EventStart,175>
// keyed by event index. Direct twin of SpecificGameModeEventInterface (same 175
// capacity, same linear-scan-then-Append shape); the EventStart element is 48 bytes.
//
//   AddEventStart    X360 0x82361398
//   AppendEventStart X360 0x8235B7D8  (Hex-Rays truncated symbol "...::EventSta";
//                                      the inlined generic Array<EventStart,175>::Append)
//
// X360-authoritative behaviour. The original streamed the dynamic
// "Array container out of space, Length: <n>, Capacity: <c>" message into the assert
// buffer; the equivalent static CGS_ASSERT condition is used here (the same condition the
// generic Array<T,N>::Append asserts), matching the project's container assert convention.
// =============================================================================

namespace BrnGameState
{
namespace GameStateModuleIO
{

// X360 0x8235B7D8 - inlined Array<EventStart,175>::Append (void: CgsArray.h Append asserts
// the array was Construct/Clear'd and has room, copies the record into the next free slot,
// bumps the count). The inlined ASM never repoints r3 off its function-entry value on the
// non-firing path (0x8235B8D0 falls straight through from entry with r3 untouched since the
// `mr r29, r3` at 0x8235B7E4) -- i.e. the X360 "return" here is a void-Append inlining
// artifact that yields the SetUpAllEventStartsInterface `this` pointer, NOT the new element.
// Reproduced verbatim (asm-authoritative) rather than "corrected" to the new element.
SetUpAllEventStartsInterface::EventStart*
SetUpAllEventStartsInterface::AppendEventStart(const EventStart& lrEventStart)
{
    maEventStarts.Append(lrEventStart);
    return reinterpret_cast<EventStart*>(this);
}

// X360 0x82361398 - linear-scan AddEventStart.
SetUpAllEventStartsInterface::EventStart*
SetUpAllEventStartsInterface::AddEventStart(const u8* lpLeadingBlock, s32 liEventIndex, s32 liEventID,
                                            s32 liWord10, s32 liWord1C, s16 liWord20)
{
    // Linear scan of the live elements for a record already keyed to this event index.
    const u32 luCount = maEventStarts.GetLength();
    for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
    {
        EventStart& lrExisting = maEventStarts.GetItem(luIndex);
        if (lrExisting.GetEventIndex() == liEventIndex)
        {
            // Found: the event id must match the one already recorded.
            CGS_ASSERT(liEventID == lrExisting.GetEventID(),
                       "luEventID == maEventStarts.GetItem( luEventIndex ).GetEventID()");
            return &lrExisting;
        }
    }

    // Not found: assert room then build a fresh record from the caller's fields and append it.
    CGS_ASSERT(maEventStarts.GetLength() < maEventStarts.GetSize(),
               "maEventStarts.GetLength() < maEventStarts.GetCapacity()");

    EventStart lEventStart;
    for (u32 luByte = 0; luByte < sizeof(lEventStart.maLeadingBlock); ++luByte)
    {
        lEventStart.maLeadingBlock[luByte] = lpLeadingBlock[luByte];
    }
    lEventStart.miWord10     = liWord10;
    lEventStart.miEventIndex = liEventIndex;
    lEventStart.miEventID    = liEventID;
    lEventStart.miWord1C     = liWord1C;
    lEventStart.miWord20     = liWord20;
    // X360-authoritative: the not-found path returns whatever AppendEventStart/EventSta
    // returns (0x823614E8 -> LABEL_15 falls straight through to the epilogue with r3
    // untouched), which per AppendEventStart above is `this`, not the new element.
    return AppendEventStart(lEventStart);
}

} // namespace GameStateModuleIO
} // namespace BrnGameState
