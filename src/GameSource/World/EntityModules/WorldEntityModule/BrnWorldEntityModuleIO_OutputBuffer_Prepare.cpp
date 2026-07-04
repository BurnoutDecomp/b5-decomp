#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::OutputBuffer_Prepare member, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the single X360-emitted accessor:
//
//   GetResourceRequestInterface() @ 0x822BA180  -> &mResourceRequestInterface (this + 4),
//                                                  asserts write-lock (bit 3)
//
// The asm tests the write-lock bit (`lbz r11,0(this); extrwi r11,r11,1,28` == bit 3 of
// the status byte == IsBufferLockedForWriting()), and on failure fires the assert via the
// streamed message "Not locked for writing\n" against
// ..\\gamesource\\world\\entitymodules\\worldentitymodule\\BrnWorldEntityModuleIO.h:73.
// It then returns the member address (this + 4). Modelled as the house CGS_ASSERT +
// member-address return (the streamed "\n" is dropped from the stringized condition, as in
// the sibling PropEntityIO::OutputBuffer_PreScene accessors).

namespace BrnWorld
{
namespace WorldEntityIO
{
    void OutputBuffer_Prepare::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_Prepare, mResourceRequestInterface) == 4,
                      "mResourceRequestInterface @4");
    }

    // X360 0x822BA180: write-lock; return this + 4.
    OutputBuffer_Prepare::ResourceRequestInterfaceStorage*
    OutputBuffer_Prepare::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResourceRequestInterface;
    }

    // X360 0x827A2728: read-lock; return this + 4. DWARF BrnWorldEntityModuleIO.h:72 -- the
    // const overload; the asm tests the read-lock bit (`extrwi r11,r11,1,27` == bit 4 ==
    // IsBufferLockedForReading()) and fires "Not locked for reading". Called by
    // WorldModule::BridgeWorldResourceRequestsToOutput_Prepare.
    const OutputBuffer_Prepare::ResourceRequestInterfaceStorage*
    OutputBuffer_Prepare::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mResourceRequestInterface;
    }
}
}
