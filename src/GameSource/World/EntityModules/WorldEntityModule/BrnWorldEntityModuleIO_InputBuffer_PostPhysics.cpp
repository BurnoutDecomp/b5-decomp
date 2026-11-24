#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // the header-baked lock asserts

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
        CGS_ASSERT(IsBufferLockedForReading(), CgsDev::Assert::StreamMessageAt("Not locked for reading\n", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\entitymodules\\worldentitymodule\\BrnWorldEntityModuleIO.h", 223));
        return &mGameActionQueue;
    }

    // X360 0x827A2D28: write-lock; return this + 4.
    InputBuffer_PostPhysics::GameActionQueue*
    InputBuffer_PostPhysics::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), CgsDev::Assert::StreamMessageAt("Not locked for writing\n", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\entitymodules\\worldentitymodule\\BrnWorldEntityModuleIO.h", 224));
        return &mGameActionQueue;
    }
}
}

// DecFIGS BrnWorldEntityModuleIO.cpp:130: the whole body is the base teardown. ARTIST ICF-folds it into
// BrnWorld::PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0 (`b CgsModule::IOBuffer::Destruct`),
// the target DestroyIOBuffer<InputBuffer_PostPhysics> @0x827BA018 calls.
void BrnWorld::WorldEntityIO::InputBuffer_PostPhysics::Destruct()
{
    CgsModule::IOBuffer::Destruct();
}
