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
        // X360 32-bit byte offsets retired 2026-07-24: the buffer now holds the real
        // typed members (several contain host pointers), so per the x64 gate the
        // layout is semantic-by-NAME; the X360 offsets live as comments in the header.
    }

    // X360 0x822BA180: write-lock; return this + 4.
    ResourceRequestInterface*
    OutputBuffer_Prepare::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResourceRequestInterface;
    }

    // X360 0x827A2728: read-lock; return this + 4. DWARF BrnWorldEntityModuleIO.h:72 -- the
    // const overload; the asm tests the read-lock bit (`extrwi r11,r11,1,27` == bit 4 ==
    // IsBufferLockedForReading()) and fires "Not locked for reading". Called by
    // WorldModule::BridgeWorldResourceRequestsToOutput_Prepare.
    const ResourceRequestInterface*
    OutputBuffer_Prepare::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mResourceRequestInterface;
    }

    // X360 0x827BBC50: write-lock; return this + 0x1020. DWARF BrnWorldEntityModuleIO.h:76.
    // ⭐ DE-FUDGED 2026-08-10. This accessor used to be a SINGLE overload guarded
    // `IsBufferLockedForReading() || IsBufferLockedForWriting()`, on the reasoning that the
    // one attested call site (WorldModule::Prepare's stage-8 fail path) reads under a read
    // lock. The const twin was inferred to exist and then merged away. It DOES exist, and
    // 0x827BBC50 is the non-const one: IDA has no export record for it at all (an export
    // hole -- cf. the standing "missing-from-JSON != nonexistent" rule), so it was lifted
    // straight out of the image after proving the decoder 41/41 against its exported
    // neighbour. `rlwinm r11,r11,29,31,31` == bit 3 == IsBufferLockedForWriting(), the
    // string pointer is exactly 24 bytes below the "reading" one
    // (len("Not locked for writing\n")+1), and it fires against :76.
    // This is the THIRD time this const/non-const accessor family has been met here; the
    // previous wave's 927-assert regression was calling the wrong twin of the scene-manager
    // pair. Both overloads are now real, with the console's own per-overload tripwire.
    SceneInputInterface*
    OutputBuffer_Prepare::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // X360 0x827BBBA8: read-lock; return this + 0x1020. DWARF BrnWorldEntityModuleIO.h:75 --
    // the const overload (`rlwinm r11,r11,28,31,31` == bit 4 == IsBufferLockedForReading(),
    // "Not locked for reading"). Called by WorldModule::PrepareWorldCollision @0x827C9478
    // under the scene-input/world-entity LockBuffersForIO bracket, and by
    // WorldModule::Prepare's stage-8 fail path.
    const SceneInputInterface*
    OutputBuffer_Prepare::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }
}
}
