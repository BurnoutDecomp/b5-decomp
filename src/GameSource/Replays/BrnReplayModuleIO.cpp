#include "GameSource/Replays/BrnReplayModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // memcpy

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

    // ====================================================================================
    // OutputBuffer_PreSim accessors (BrnReplayModuleIO.h:107-115). Each reads the IOBuffer
    // lock byte and asserts the correct lock state, then returns the embedded member.
    // ====================================================================================
    void OutputBuffer_PreSim::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PreSim, mStatusInterface) == 0x0004,
                      "mStatusInterface @0x0004 (GetStatusInterface -> this+4)");
        static_assert(offsetof(OutputBuffer_PreSim, mGameDataRequestInterface) == 0x061C,
                      "mGameDataRequestInterface @0x061C (GetGameDataRequestInterface -> this+0x61C)");
        static_assert(offsetof(OutputBuffer_PreSim, mGuiEventQueue) == 0x0A2C,
                      "mGuiEventQueue @0x0A2C (GetGuiEventQueue -> this+0xA2C)");
    }

    // X360 0x823BB080: read-lock; returns &mStatusInterface (this+4).
    const StatusInterface* OutputBuffer_PreSim::GetStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mStatusInterface;
    }

    // X360 0x8264E1D0: write-lock; returns &mStatusInterface (this+4).
    StatusInterface* OutputBuffer_PreSim::GetStatusInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mStatusInterface;
    }

    // X360 0x8264E278: write-lock; returns &mGameDataRequestInterface (this+0x61C).
    OutputBuffer_PreSim::GameDataRequestInterface* OutputBuffer_PreSim::GetGameDataRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGameDataRequestInterface;
    }

    // X360 0x823BB1D0: read-lock; returns &mGuiEventQueue (this+0xA2C).
    const OutputBuffer_PreSim::GuiEventQueue* OutputBuffer_PreSim::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiEventQueue;
    }

    // ====================================================================================
    // InputBuffer_PreSim accessors (BrnReplayModuleIO.h:71-78).
    // ====================================================================================
    void InputBuffer_PreSim::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PreSim, mGameActionQueue) == 0x0004,
                      "mGameActionQueue @0x0004 (AppendGameActionQueue -> this+4)");
        static_assert(offsetof(InputBuffer_PreSim, mTimerStatusInterface) == 0x3414,
                      "mTimerStatusInterface @0x3414 (Get/SetTimerStatusInterface -> this+0x3414)");
    }

    // X360 0x823C9A38: write-lock; bulk-append the source game-action queue into
    // mGameActionQueue (VariableEventQueue<13312,16>::Append<13312,16>).
    int InputBuffer_PreSim::AppendGameActionQueue(const GameActionQueue* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mGameActionQueue.Append(*lpSourceQueue) ? 1 : 0;
    }

    // X360 0x8264E128: read-lock; returns &mTimerStatusInterface (this+0x3414).
    const CgsSystem::TimerStatusInterface* InputBuffer_PreSim::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTimerStatusInterface;
    }

    // X360 0x823BAF70: write-lock; copy the source TimerStatusInterface (48 bytes: the game
    // and sim TimerStatus blocks) into mTimerStatusInterface (this+0x3414). The X360 body
    // copies it as two 24-byte runs (lwz/lfs/lfs/lbz/lwz/lfs per TimerStatus); a flat
    // memcpy of the whole 48-byte interface is the byte-faithful equivalent.
    void InputBuffer_PreSim::SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        memcpy(&mTimerStatusInterface, lpSource, sizeof(CgsSystem::TimerStatusInterface));
    }

    // ====================================================================================
    // InputBuffer_PostSim accessors (BrnReplayModuleIO.h:145-150).
    // ====================================================================================
    void InputBuffer_PostSim::_AssertLayout()
    {
        // X360 (32-bit) offsets: mRequestInterface @+0x04 (RequestInterface == BaseSerialiser*[11],
        // 44 bytes), mGuiEventQueue @+0x30. NOT static_asserted: RequestInterface is a pointer array,
        // so on the x64 gate it is 8-byte aligned/sized and these offsets necessarily diverge. Parity
        // is by named member (the accessors return &mRequestInterface / &mGuiEventQueue), per the
        // committed BrnReplayRequestInterface / BrnReplayStatusInterface precedent (no offset asserts
        // on pointer-bearing replay-IO members).
    }

    // X360 0x823BB278: write-lock; returns &mRequestInterface (this+4).
    RequestInterface* InputBuffer_PostSim::GetRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mRequestInterface;
    }

    // X360 0x823BB320: write-lock; merge the source request interface's serialiser slots into
    // mRequestInterface (this+4). The X360 body tail-calls RequestInterface::Append and returns
    // its result; Append is void here, so the merge is performed and 0 returned (parity is the
    // merge behaviour, not the residual return register).
    int InputBuffer_PostSim::AppendRequestInterface(const RequestInterface* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mRequestInterface.Append(lpSource);
        return 0;
    }

    // X360 0x8264E3C8: read-lock; returns &mGuiEventQueue (this+0x30).
    const InputBuffer_PostSim::GuiEventQueue* InputBuffer_PostSim::GetGuiEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiEventQueue;
    }

    // X360 0x823BB3D0: write-lock; returns &mGuiEventQueue (this+0x30).
    InputBuffer_PostSim::GuiEventQueue* InputBuffer_PostSim::GetGuiEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGuiEventQueue;
    }
}
}
