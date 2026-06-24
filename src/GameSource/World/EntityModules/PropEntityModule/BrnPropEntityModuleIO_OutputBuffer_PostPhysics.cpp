#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::PropEntityIO::OutputBuffer_PostPhysics member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 3 X360-emitted accessors:
//
//   GetPropInputInterface() const @ 0x827A2000  -> &mPropInputInterface  (+833008),  read  (bit 4)
//   GetSceneInputInterface()      @ 0x822B9BD0  -> &mSceneInputInterface (+820912),  write (bit 3)
//   GetPropInputInterface()       @ 0x822B9FC0  -> &mPropInputInterface  (+833008),  write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`); the
// non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1), `extrwi r11,r11,1,28`) --
// matching CgsModule::IOBuffer's IsBufferLockedForReading()/IsBufferLockedForWriting(). The X360
// asserts the lock state (streaming "Not locked for reading/writing\n", a non-gating tripwire at
// BrnPropEntityModuleIO.h:739/742/748), then returns the member's address: this + 820912
// (`addis rN,0xD; addi rN,-0x7950`) for mSceneInputInterface, this + 833008
// (`addis rN,0xD; addi rN,-0x4A10`) for mPropInputInterface.
//
// The read-lock and write-lock GetPropInputInterface both return the same +833008 member (the
// const/non-const overload pair); GetSceneInputInterface returns the lower +820912 member. The
// DWARF (BrnPropEntityModuleIO.h:752/753) lays mSceneInputInterface before mPropInputInterface, so
// Scene is the lower offset -- consistent with the asm return offsets.

namespace BrnWorld
{
namespace PropEntityIO
{
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics, mSceneInputInterface) == 820912, "mSceneInputInterface @820912");
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropInputInterface)  == 833008, "mPropInputInterface @833008");
    }

    // X360 0x827A2000: read-lock; return this + 833008.
    const OutputBuffer_PostPhysics::PropInputInterfaceStorage* OutputBuffer_PostPhysics::GetPropInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropInputInterface;
    }

    // X360 0x822B9BD0: write-lock; return this + 820912.
    OutputBuffer_PostPhysics::SceneInputInterfaceStorage* OutputBuffer_PostPhysics::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // X360 0x822B9FC0: write-lock; return this + 833008.
    OutputBuffer_PostPhysics::PropInputInterfaceStorage* OutputBuffer_PostPhysics::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropInputInterface;
    }
}
}
