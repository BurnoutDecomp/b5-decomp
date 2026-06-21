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

// X360 0x8235B7D8 - inlined Array<EventStart,175>::Append: assert the array was
// Construct/Clear'd and has room, copy the record into the next free slot, bump the count,
// and return the newly added record.
SetUpAllEventStartsInterface::EventStart*
SetUpAllEventStartsInterface::AppendEventStart(const EventStart& lrEventStart)
{
    maEventStarts.Append(lrEventStart);
    return &maEventStarts.GetItem(maEventStarts.GetLength() - 1);
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
    // FLAG: the X360 append path returns the array BASE pointer (an inlined-void-Append
    // artifact leaving r3 = &maElements[0]); we return the NEW element for consistency with
    // the found path above (return &lrExisting) -- the intended semantic. Re-confirm against
    // the caller (GameStateModule::SendSetUpAllEventStartsMessage) when that TU is recovered.
    return AppendEventStart(lEventStart);
}

} // namespace GameStateModuleIO
} // namespace BrnGameState
