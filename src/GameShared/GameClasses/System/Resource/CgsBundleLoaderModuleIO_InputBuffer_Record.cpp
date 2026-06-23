#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::InputBuffer_Record member function, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the one X360-emitted write-locked accessor:
//
//   GetRecord() @ 0x828E2090  -> &mRecord (this + 4), write-locked (bit 3)
//
// The X360 body loads the 1-byte FlagSet status (lbz 0(this)), tests the write-lock bit
// (extrwi r11,r11,1,28 == (status >> 3) & 1, i.e. CgsModule::IOBuffer::eStatusLockedForWrite),
// asserts "Not locked for writing" when clear, then returns this + 4. We return the named
// member's address (semantic parity, not byte-match).

namespace CgsResource
{
namespace BundleLoaderIO
{
    void InputBuffer_Record::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_Record, mRecord) == 0x0004, "mRecord @0x0004");
    }

    // X360 0x828E2090: write-lock; return this + 4.
    InputBuffer_Record::RecordStorage* InputBuffer_Record::GetRecord()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");   // :113
        return &mRecord;
    }
}
}
