// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO_Accessors.cpp
//
// Out-of-line bodies for the BrnWorld::TriggerEntityModuleIO IO-buffer interface accessors the
// X360 ARTIST build emitted out-of-line. Each guarded accessor asserts the buffer's lock bit
// then returns &member:
//   read-lock  (status>>4 &1) => IsBufferLockedForReading()  (const getter)
//   write-lock (status>>3 &1) => IsBufferLockedForWriting()  (mutators)
// Offsets are layout-derived (&member), not hardcoded. CGS_ASSERT stamps __FILE__/__LINE__, so
// the X360-baked d:\p4 path/lines (90/91/119) are not reproduced.
//
// The three out-of-line functions in this TU:
//   OutputBuffer_PreScene::GetSceneInputInterface() const  @ 0x827A31C8 (R, :90)  -> &mSceneInputInterface(+16)
//   OutputBuffer_PreScene::GetSceneInputInterface()        @ 0x822BCF38 (W, :91)  -> &mSceneInputInterface(+16)
//   InputBuffer_PreScene::GetInputInterface()              @ 0x827A3270 (W, :119) -> &mInputInterface(+4)
// (The read-lock @0x827A31C8 is the const overload, the write-lock @0x822BCF38 the mutable one;
// both return the same +16 member. The write-lock @0x827A3270 is the separate input buffer's
// +4 accessor.)
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    void InputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PreScene, mInputInterface) == 4,
                      "mInputInterface @4");
    }

    // X360 0x827A31C8 (R, :90) -- const scene-input-interface accessor; returns
    // &mSceneInputInterface (this + 16).
    const OutputBuffer_PreScene::SceneInputInterface*
    OutputBuffer_PreScene::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }

    // X360 0x822BCF38 (W, :91) -- mutable scene-input-interface accessor; returns
    // &mSceneInputInterface (this + 16).
    OutputBuffer_PreScene::SceneInputInterface*
    OutputBuffer_PreScene::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // X360 0x827A3270 (W, :119) -- mutable input-interface accessor; returns &mInputInterface
    // (this + 4).
    InputBuffer_PreScene::InputInterface*
    InputBuffer_PreScene::GetInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInputInterface;
    }
}
}
