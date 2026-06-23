// BrnCursor.h
// Home of BrnGui::GuiCursor -- the GUI screen cursor component. Each frame it publishes
// its current position + state onto the owning GUI state's output event queue so the
// view layer can draw it.
//
// This slice reconstructs ONLY the out-of-line member the X360 ARTIST build emits:
//
//   Update @ 0x824941A8 -> build a 48-byte cursor event { header {32, 560, 16}, a 16-byte
//                          position lane, and two trailing state words } and push it onto
//                          mpStateInterface's large output queue with channel id 40
//                          (GuiEventOut). Asserts mpStateInterface is set first (non-fatal;
//                          DWARF GameSource/Gui/Flow/Screen/Components/BrnCursor.h:380).
//
// LAYOUT (X360 authoritative; offsets are the load store displacements in Update):
//   +0x88  mpStateInterface        -- CgsGui::StateInterface* (asserted non-null; the queue
//                                     is reached via it).            `lwz r,0x88(this)`
//   +0xA0  maPosition[4]           -- 16-byte position lane copied into the event by
//                                     lvx128/stvx128 (a plain 16-byte block move).
//   +0xE4  miState0                -- trailing state word 0 (event +0x20). `lwz r,0xE4(this)`
//   +0xE8  miState1                -- trailing state word 1 (event +0x24). `lwz r,0xE8(this)`
// Only the members Update touches are modelled, at their X360-proven offsets; the
// intervening bytes are explicitly-reserved spans so the touched members sit at the
// proven offsets. Honest layout boundary, not fabricated.

#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event base

namespace BrnGui
{
    // The cursor event Update publishes. The X360 fills a fixed 3-word header
    // { muHeader0 = 32, muEventType = 560, muHeader2 = 16 } then a 16-byte position lane
    // and two trailing state words, total 48 bytes, pushed with channel id 40. The
    // header word names mirror the CgsGui::GuiEvent base shape (header0 / type / header2).
    struct GuiEventCursorUpdate : public CgsModule::Event
    {
        u32 muHeader0;      // @0x00 = 32   (event +0x00)
        u32 muEventType;    // @0x04 = 560  (event +0x04, the cursor event type id)
        u32 muHeader2;      // @0x08 = 16   (event +0x08)
        u32 muReserved;     // @0x0C        (X360 leaves this gap word uninitialised)
        f32 maPosition[4];  // @0x10 .. 0x1F  (cursor position lane, copied from the cursor)
        s32 miState0;       // @0x20        (cursor state word 0)
        s32 miState1;       // @0x24        (cursor state word 1)
        u32 maTail[2];      // @0x28 .. 0x2F  (X360 leaves these tail words uninitialised)
    };

    // The screen cursor component. Only the members Update touches are modelled.
    class GuiCursor
    {
    public:
        // @ 0x824941A8 -- publish the cursor event onto the owning state's output queue.
        // Returns the queue AddEvent result (the X360 forwards it through r3).
        bool Update();

    private:
        u8 maHeadReserved0[0x88];           // @0x00 .. 0x87
        CgsGui::StateInterface* mpStateInterface; // @0x88  (asserted non-null)
        u8 maHeadReserved1[0xA0 - 0x8C];    // @0x8C .. 0x9F
        f32 maPosition[4];                  // @0xA0 .. 0xAF  (16-byte position lane)
        u8 maHeadReserved2[0xE4 - 0xB0];    // @0xB0 .. 0xE3
        s32 miState0;                       // @0xE4
        s32 miState1;                       // @0xE8
    };
}
