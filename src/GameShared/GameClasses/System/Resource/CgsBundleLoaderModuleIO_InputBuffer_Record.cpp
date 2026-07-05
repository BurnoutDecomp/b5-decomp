#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::InputBuffer_Record member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the two X360-emitted record accessors (the
// pool-receive queue at +4), exposed under both the DWARF name (GetPoolReceiveQueue) and the
// historical name (GetRecord):
//
//   GetRecord() const @ 0x828E1FE8  -> &mPoolReceiveQueue (this + 4), read-locked  (bit 4)  (:112)
//   GetRecord()       @ 0x828E2090  -> &mPoolReceiveQueue (this + 4), write-locked (bit 3)  (:113)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)) and tests its lock bit (read: extrwi
// ...,27 == (status>>4)&1 == eStatusLockedForRead; write: extrwi ...,28 == (status>>3)&1 ==
// eStatusLockedForWrite), asserts when clear, then returns this + 4.

namespace CgsResource
{
namespace BundleLoaderIO
{
    void InputBuffer_Record::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_Record, mPoolReceiveQueue) == 0x0004, "mPoolReceiveQueue @0x0004");
    }

    // X360 0x828E1FE8: read-lock; return this + 4.
    const InputBuffer_Record::RecordStorage* InputBuffer_Record::GetRecord() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");   // :112
        return &mPoolReceiveQueue;
    }

    // X360 0x828E2090: write-lock; return this + 4.
    InputBuffer_Record::RecordStorage* InputBuffer_Record::GetRecord()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");   // :113
        return &mPoolReceiveQueue;
    }
}
}
