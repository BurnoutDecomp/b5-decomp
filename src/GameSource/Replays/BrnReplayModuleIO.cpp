#include "GameSource/Replays/BrnReplayModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnReplays::ReplayIO member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   ReplayIO_Buffer::GetOutputBuffer_PostSim  @ 0x823BB478  read-lock (bit 4) -> this+4
//   ReplayIO_Buffer::GetOutputBuffer_PreSim   @ 0x823BB128  read-lock (bit 4) -> this+1564
//   OutputBuffer_PostSim::AppendGameEventQueue@ 0x8265A798  write-lock (bit 3) -> Append into this+4
//
// Each accessor first checks the IOBuffer lock-state flag and asserts on violation
// exactly as the X360 bodies do (the original streams the file/line via CgsDev::Assert;
// CGS_ASSERT carries the stringized condition + __FILE__/__LINE__). The two ReplayIO
// phase getters take the READ lock (bit 4, "Not locked for reading"); AppendGameEventQueue
// takes the WRITE lock (bit 3, "Not locked for writing") on the post-sim sub-buffer.

namespace BrnReplays
{
namespace ReplayIO
{
    void OutputBuffer_PostSim::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostSim, mGameEventQueue) == 0x0004,
                      "mGameEventQueue @0x0004");
        static_assert(sizeof(OutputBuffer_PostSim) == 1560,
                      "OutputBuffer_PostSim stride == 1560 (ReplayIO this+4 .. this+1564)");
    }

    void ReplayIO_Buffer::_AssertLayout()
    {
        static_assert(offsetof(ReplayIO_Buffer, mPostSimBuffer) == 0x0004,
                      "mPostSimBuffer @0x0004 (GetOutputBuffer_PostSim -> this+4)");
        static_assert(offsetof(ReplayIO_Buffer, mPreSimBuffer) == 0x061C,
                      "mPreSimBuffer @0x061C (GetOutputBuffer_PreSim -> this+1564)");
    }

    // X360 0x8265A798: write-lock the post-sim buffer, then append the source game-event
    // queue into mGameEventQueue (VariableEventQueue<1536,16>::Append<1536,16>). The X360
    // body returns the Append result (int/bool).
    int OutputBuffer_PostSim::AppendGameEventQueue(const GameEventQueue* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mGameEventQueue.Append(*lpSourceQueue) ? 1 : 0;
    }

    // X360 0x823BB478: read-lock; returns the embedded post-sim output buffer (this+4).
    OutputBuffer_PostSim* ReplayIO_Buffer::GetOutputBuffer_PostSim()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPostSimBuffer;
    }

    // X360 0x823BB128: read-lock; returns the embedded pre-sim output buffer (this+1564).
    OutputBuffer_PreSim* ReplayIO_Buffer::GetOutputBuffer_PreSim()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPreSimBuffer;
    }
}
}
