#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::InputBuffer_PostPhysics accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the two X360-emitted GetGameActionQueue overloads
// (DWARF BrnWorldEntityModuleIO.h:212 struct):
//
//   GetGameActionQueue() const @ 0x822BA768  -> &mGameActionQueue (this + 4),
//                                               asserts read-lock  (bit 4) [DWARF :223]
//   GetGameActionQueue()       @ 0x827A2D28  -> &mGameActionQueue (this + 4),
//                                               asserts write-lock (bit 3) [DWARF :224]
//
// The const getter tests the read-lock bit (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4
// == IsBufferLockedForReading()) and fires "Not locked for reading"; the non-const getter
// tests the write-lock bit (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting())
// and fires "Not locked for writing". Both return the member address (this + 4). The
// streamed "\n" is dropped from the stringized condition, as in the sibling
// OutputBuffer_Prepare accessor.

namespace BrnWorld
{
namespace WorldEntityIO
{
    void InputBuffer_PostPhysics::_AssertLayout()
    {
        // X360 32-bit byte offsets retired 2026-07-24: typed members (host pointers)
        // -> semantic-by-NAME layout; the X360 offsets live as comments in the header.
    }

    // X360 0x822BA768: read-lock; return this + 4.
    const InputBuffer_PostPhysics::GameActionQueue*
    InputBuffer_PostPhysics::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameActionQueue;
    }

    // X360 0x827A2D28: write-lock; return this + 4.
    InputBuffer_PostPhysics::GameActionQueue*
    InputBuffer_PostPhysics::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGameActionQueue;
    }
}
}
