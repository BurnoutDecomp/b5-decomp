// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/CgsGuiModuleIO.h
//
// Canonical (DWARF) home for CgsGui::CgsGuiModuleIO::OutputBuffer (CgsGuiModuleIO.h).
// This is a MINIMAL-COMPLETE slice covering ONLY the OutputBuffer's X360-emitted
// accessors owned by the IO-OutputBuffers group:
//   GetGuiResourceRequestQueue() (write handle) @ 0x8284F388
//   GetOutEventQueue()           (read handle)  @ 0x823B4130
//   AddGuiOutEvents()            (bulk append)  @ 0x8250C718
//
// LAYOUT (DWARF CgsGuiModuleIO.h:140 + X360 getter return-offsets, authoritative):
//   base  CgsModule::IOBuffer                 (1-byte FlagSet status; +1..+3 pad)
//   +4    GuiResourceRequestQueue mResourceRequestQueue   (ResourceRequestQueue<2048>)
//   +2068 GuiEventQueue           mOutEvents              (VariableEventQueue<18432,16>)
//   +...  GameActionQueue         mGameActionQueue        (BaseGameActionQueue<13312>)
// The getter return-offsets pin +4 (mResourceRequestQueue, write-lock bit 3) and
// +2068/0x814 (mOutEvents, read-lock bit 4). AddGuiOutEvents bulk-appends a source
// GuiEventQueueSmall (VariableEventQueue<4096,16>) into mOutEvents via the X360
// VariableEventQueue<18432,16>::Append<4096,16> (CgsVariableEventQueue.h).
//
// FLAG (foreign types): GuiResourceRequestQueue (== ResourceRequestQueue<2048>, DWARF
// CgsGuiResourceModuleIO.h:193) and GameActionQueue (== BaseGameActionQueue<13312>,
// DWARF CgsGuiModuleIO.h:114) have their own owning homes elsewhere and are NOT
// reconstructed here. They are modelled as correctly-sized, correctly-placed opaque
// byte storage so the X360 member offsets (+4, +2068) are exact; when their real
// homes land this header should adopt the named types additively. mOutEvents uses the
// committed CgsModule::VariableEventQueue<18432,16> generic by name (the GUI out-event
// queue type GuiEventQueueBase<18432,16> derives from it; the Append target proves the
// 18432 size).
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // The out-event queue published to consumers: VariableEventQueue<18432,16>.
        // GuiEventQueueSmall == VariableEventQueue<4096,16> is the source bulk-appended in.
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;
        typedef CgsModule::VariableEventQueue<4096, 16>  GuiEventQueueSmall;

        // GuiResourceRequestQueue == ResourceRequestQueue<2048> (DWARF CgsGuiModuleIO.h:205,
        // CgsGuiResourceModuleIO.h:193). The X360 SetGuiResourceRequestQueue @0x8285AFB0
        // bulk-appends a source request queue into this member via
        // VariableEventQueue<2048,16>::Append<2048,16>, so the member IS a
        // VariableEventQueue<2048,16> (sizeof == 2064 -- identical to the prior opaque
        // 2064-byte storage, so the +4 placement and the OutputBuffer sizeof are preserved).
        // The ~25 typed request-builder methods of the real ResourceRequestQueue<2048> live
        // in their own home; modelled here as the thin VEQ-derived base it is. The name is
        // kept as GuiResourceRequestQueueStorage so the committed GetGuiResourceRequestQueue
        // accessor (CgsGuiModuleIO_OutputBuffer.cpp) is unchanged.
        struct GuiResourceRequestQueueStorage : public CgsModule::VariableEventQueue<2048, 16>
        {
        };

        // FLAG: BaseGameActionQueue<13312> (foreign home). Trailing member; sized to the
        // GameAction queue inline buffer plus its small bookkeeping header.
        struct GameActionQueueStorage
        {
            unsigned char maBytes[13312 + 16];
        };

        // ---- accessors owned/bodied by this group --------------------------------------
        // X360 0x8284F388: write-lock (bit 3) handle to the resource-request queue.
        GuiResourceRequestQueueStorage* GetGuiResourceRequestQueue();
        // X360 0x8285AFB0: write-lock (bit 3); asserts the source ptr is non-null, then
        // bulk-appends it into mResourceRequestQueue (VariableEventQueue<2048,16>::
        // Append<2048,16>). Returns the Append result (int/bool).
        int SetGuiResourceRequestQueue(const GuiResourceRequestQueueStorage* lpRequestQueue);
        // X360 0x823B4130: read-lock (bit 4) handle to the out-event queue.
        const GuiEventQueue* GetOutEventQueue() const;
        // X360 0x8250C718: write-lock (bit 3); asserts the source queue ptr is non-null,
        // then bulk-appends it into mOutEvents. Returns the Append result (int/bool).
        int AddGuiOutEvents(const GuiEventQueueSmall* lpSourceQueue);

        // Byte-offset pins (compiled in the embed check).
        static void _AssertLayout();

    private:
        u8                             maStatusPad[3];        // +1..+3 (force +4 placement)
        GuiResourceRequestQueueStorage mResourceRequestQueue; // +4    (DWARF :205)
        GuiEventQueue                  mOutEvents;            // +2068 (DWARF :207)
        GameActionQueueStorage         mGameActionQueue;      //       (DWARF :209)
    };
}
}
