#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"

#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsBundleLoaderModuleIO.h"

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::InputBuffer_Record member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the two X360-emitted record accessors (the
// pool-receive queue at +4).
//
//   GetPoolReceiveQueue() const @ 0x828E1FE8, read-locked  (bit 4)  (:112)
//   GetPoolReceiveQueue()       @ 0x828E2090, write-locked (bit 3)  (:113)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)) and tests its lock bit (read: extrwi
// ...,27 == (status>>4)&1 == eStatusLockedForRead; write: extrwi ...,28 == (status>>3)&1 ==
// eStatusLockedForWrite), asserts when clear, then returns this + 4.

namespace CgsResource
{
namespace BundleLoaderIO
{
    void InputBuffer_Record::Construct()
    {
        IOBuffer::Construct();
        mPoolReceiveQueue.Construct();
    }

    void InputBuffer_Record::Destruct() { IOBuffer::Destruct(); }

    const InputBuffer_Record::PoolReceiveQueue* InputBuffer_Record::GetPoolReceiveQueue() const
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForReading(),
            "Not locked for reading\n", CGS_ARTIST_FILE, 112);
        return &mPoolReceiveQueue;
    }

    InputBuffer_Record::PoolReceiveQueue* InputBuffer_Record::GetPoolReceiveQueue()
    {
        CGS_ASSERT_STREAM_AT(IsBufferLockedForWriting(),
            "Not locked for writing\n", CGS_ARTIST_FILE, 113);
        return &mPoolReceiveQueue;
    }

    // X360 0x828E1FE8: read-lock; return this + 4.
    const InputBuffer_Record::RecordStorage* InputBuffer_Record::GetRecord() const
    {
        return GetPoolReceiveQueue();
    }

    // X360 0x828E2090: write-lock; return this + 4.
    InputBuffer_Record::RecordStorage* InputBuffer_Record::GetRecord()
    {
        return GetPoolReceiveQueue();
    }
}
}

#undef CGS_ARTIST_FILE
