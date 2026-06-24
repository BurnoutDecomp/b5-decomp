#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics member, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the single X360-emitted accessor:
//
//   Get() @ 0x822BA8B8  -> &mOutputPayload (this + 822896 == 0xC8E70), asserts write-lock (bit 3)
//
// The asm tests the write-lock bit (`lbz r11,0(this); extrwi r11,r11,1,28` == bit 3 of the
// status byte == IsBufferLockedForWriting()), and on failure fires the assert via the
// streamed message "Not locked for writing\n" against
// ..\\gamesource\\world\\entitymodules\\worldentitymodule\\BrnWorldEntityModuleIO.h:249. It
// then returns the member address (this + 822896, computed by the asm as
// `addis r3,this,0xD; addi r3,r3,-0x7190` == this + 0xD0000 - 0x7190 == this + 0xC8E70).
// Modelled as the house CGS_ASSERT + member-address return (the streamed "\n" is dropped from
// the stringized condition, as in the sibling PropEntityIO::OutputBuffer_PreScene accessors).

namespace BrnWorld
{
namespace WorldEntityIO
{
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics, mOutputPayload) == 822896,
                      "mOutputPayload @822896");
    }

    // X360 0x822BA8B8: write-lock; return this + 822896.
    OutputBuffer_PostPhysics::OutputPayloadStorage* OutputBuffer_PostPhysics::Get()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mOutputPayload;
    }
}
}
