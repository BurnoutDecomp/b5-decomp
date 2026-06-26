#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"

#include <cstddef>   // offsetof

// class:BrnWorld::CrashIO group -- the four read-side buffer accessors the X360 emitted
// out-of-line for the crash module's input buffers plus the post-physics output buffer's read
// view. Reconstructed from BURNOUT_X360_ARTIST.XEX. Each tests the read-lock bit
// (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 == IsBufferLockedForReading()) and on
// failure streams "Not locked for reading\n", then returns `this + offset`:
//   InputBuffer_PreScene::GetReadInterface             @ 0x827BB3D8  this + 0x3CD0  (line 81)
//   InputBuffer_HandleGameActions::GetReadInterface    @ 0x827BB528  this + 0x7A70  (line 87)
//   InputBuffer_PostPhysics::GetReadInterface          @ 0x827BB918  this + 0x79B0  (line 174)
//   OutputBuffer_PostPhysics_ReadView::GetGameEventQueue @ 0x827A2680  this + 0x7A0 (line 210)
//
// The streamed-message asserts map to the house CGS_ASSERT (the trailing "\n" is dropped from
// the stringized condition). The getter method names and the returned members' names are not
// recoverable from the truncated symbols, so each returned member is correctly-positioned
// opaque storage so the single X360-pinned return offset is exact.

namespace BrnWorld
{
namespace CrashIO
{
    void InputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PreScene, mReadInterface) == 0x3CD0,
                      "InputBuffer_PreScene::mReadInterface @0x3CD0");
    }
    const InputBuffer_PreScene::ReadInterfaceStorage*
    InputBuffer_PreScene::GetReadInterface() const   // 0x827BB3D8
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mReadInterface;
    }

    void InputBuffer_HandleGameActions::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_HandleGameActions, mReadInterface) == 0x7A70,
                      "InputBuffer_HandleGameActions::mReadInterface @0x7A70");
    }
    const InputBuffer_HandleGameActions::ReadInterfaceStorage*
    InputBuffer_HandleGameActions::GetReadInterface() const   // 0x827BB528
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mReadInterface;
    }

    void InputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PostPhysics, mReadInterface) == 0x79B0,
                      "InputBuffer_PostPhysics::mReadInterface @0x79B0");
    }
    const InputBuffer_PostPhysics::ReadInterfaceStorage*
    InputBuffer_PostPhysics::GetReadInterface() const   // 0x827BB918
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mReadInterface;
    }

    void OutputBuffer_PostPhysics_ReadView::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics_ReadView, mGameEventQueue) == 0x7A0,
                      "OutputBuffer_PostPhysics_ReadView::mGameEventQueue @0x7A0");
    }
    // PS3 DecFIGS: OutputBuffer_PostPhysics::GetGameEventQueue (BrnCrashModuleIO.h:210).
    const OutputBuffer_PostPhysics_ReadView::GameEventQueueStorage*
    OutputBuffer_PostPhysics_ReadView::GetGameEventQueue() const   // 0x827A2680
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameEventQueue;
    }
}
}
