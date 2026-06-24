#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::CgsGuiModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::CgsGuiModuleIO) bodies the three
// remaining X360-emitted OutputBuffer lock-checked handle accessors:
//
//   GetGuiResourceRequestQueue() const  @ 0x8284F2E0  -> read-lock (bit 4),  &mResourceRequestQueue (this+4)
//   GetGameActionQueue() const          @ 0x823B41D8  -> read-lock (bit 4),  &mGameActionQueue (this+0x5024)
//   GetGameActionQueue()                @ 0x824F7968  -> write-lock (bit 3),  &mGameActionQueue (this+0x5024)
//
// X360 store-for-store notes (all three share the recurring IOBuffer lock-guard prologue):
//   - Each reads the 1-byte IOBuffer status (lbz 0(this)) and tests a single lock bit:
//       0x8284F2E0 / 0x823B41D8 : extrwi r11,r11,1,27 -> PPC MSB0 bit 27 == LSB bit 4 ==
//                                 eStatusLockedForRead  ("Not locked for reading\n")
//       0x824F7968              : extrwi r11,r11,1,28 -> PPC MSB0 bit 28 == LSB bit 3 ==
//                                 eStatusLockedForWrite ("Not locked for writing\n")
//     -> CgsModule::IOBuffer::IsBufferLockedForReading()/IsBufferLockedForWriting().
//   - Each then returns a fixed member address:
//       0x8284F2E0 : `addi r3, this, 4`      == this+4      == &mResourceRequestQueue (DWARF :205)
//       0x823B41D8 : `addi r3, this, 0x5024` == this+20516  == &mGameActionQueue
//       0x824F7968 : `addi r3, this, 0x5024` == this+20516  == &mGameActionQueue
//     The +0x5024 offset is mResourceRequestQueue(+4, sizeof 2064) + mOutEvents(sizeof 18448):
//     4 + 2064 + 18448 == 20516 == 0x5024, the start of the trailing mGameActionQueue.
//   - The X360-baked d:\p4 CgsGuiModuleIO.h/.cpp file/line cites (h:319, h:335, cpp:198) are
//     discarded per project policy; CGS_ASSERT carries the stringized condition + __FILE__/__LINE__.

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    // X360 0x8284F2E0: const overload -- read-lock handle to the resource-request queue at this+4.
    const OutputBuffer::GuiResourceRequestQueueStorage* OutputBuffer::GetGuiResourceRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mResourceRequestQueue;
    }

    // X360 0x823B41D8: const overload -- read-lock handle to the trailing game-action queue at
    // this+0x5024.
    const OutputBuffer::GameActionQueueStorage* OutputBuffer::GetGameActionQueue() const
    {
        // Pin the +0x5024 getter return-offset (mGameActionQueue follows mResourceRequestQueue
        // @+4/sizeof 2064 and mOutEvents/sizeof 18448: 4 + 2064 + 18448 == 20516 == 0x5024).
        static_assert(offsetof(OutputBuffer, mGameActionQueue) == 0x5024, "mGameActionQueue @0x5024 (20516)");
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameActionQueue;
    }

    // X360 0x824F7968: non-const overload -- write-lock handle to the trailing game-action queue
    // at this+0x5024.
    OutputBuffer::GameActionQueueStorage* OutputBuffer::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGameActionQueue;
    }
}
}
