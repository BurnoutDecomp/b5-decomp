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
// [2026-08-27 event-starts producer wave] The six parameters are named by role now that the ONE
// caller (GameStateModule::SendSetUpAllEventStartsMessage @0x823759D0) is bodied and its call setup
// @0x82375C7C..0x82375CAC names every argument -- see the EventStart banner in
// BrnGameStateSharedIO.h. No behaviour changed; only the spelling of the arguments.
SetUpAllEventStartsInterface::EventStart*
SetUpAllEventStartsInterface::AddEventStart(Vector3 lv3Position, s32 liEventIndex, s32 liEventID,
                                            u32 luLightTriggerId, s32 liCounty, s16 li16AISectionIndex)
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
    // The X360 stack-builds the record at var_90 in exactly this order: `stvx128 v127` (the
    // position lane) then the five scalars at +0x10/+0x14/+0x18/+0x1C/+0x20.
    lEventStart.mv3Position        = lv3Position;
    lEventStart.muLightTriggerId   = luLightTriggerId;
    lEventStart.miEventIndex       = liEventIndex;
    lEventStart.miEventID          = liEventID;
    lEventStart.miCounty           = liCounty;
    lEventStart.mi16AISectionIndex = li16AISectionIndex;
    // X360-authoritative: the not-found path returns whatever AppendEventStart/EventSta
    // returns (0x823614E8 -> LABEL_15 falls straight through to the epilogue with r3
    // untouched), which per AppendEventStart above is `this`, not the new element.
    return AppendEventStart(lEventStart);
}

// X360 0x824F7688 - live count of registered event-start records. The constructed-guard
// assert lives inside CgsArray.h::GetLength(). BrnGui::GuiCache::GetNumEventStarts
// (@0x824F8830) is a pure tail-forwarder to this via the embedded interface @GuiCache+0x5690.
u32 SetUpAllEventStartsInterface::GetNumEventStarts() const
{
    return maEventStarts.GetLength();
}

} // namespace GameStateModuleIO
} // namespace BrnGameState
