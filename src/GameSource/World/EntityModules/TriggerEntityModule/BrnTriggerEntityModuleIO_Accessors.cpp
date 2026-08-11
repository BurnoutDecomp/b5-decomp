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
    // ⛔ CONSOLE-OFFSET PIN CORRECTED 2026-08-11 (driving-input wave).
    // This body used to assert `offsetof(InputBuffer_PreScene, mInputInterface) == 4` --
    // the X360 getter's `addi r3, this, 4`. That is a 32-BIT-POINTER offset and it CANNOT
    // hold on the x64 host: mInputInterface is the 131 KB TriggerManagementInputInterface,
    // whose leading VariableEventQueue<131072,16> begins with an event POINTER, so
    // alignof == 8 here (4 on the console) and MSVC lands the member at +8 behind the
    // 1-byte IOBuffer status. MEASURED on this host: alignof(InputInterface) == 8,
    // sizeof(InputInterface) == 132128, sizeof(InputBuffer_PreScene) == 132136.
    // The assert therefore FAILED TO COMPILE, which is why this whole TU had never been on
    // the build list and why InputBuffer_PreScene::GetInputInterface() -- committed here
    // since the trigger wave -- was still an unresolved external for every caller.
    // Parity in this project is by named member, not by byte offset (same rule the sibling
    // UpdateOutputBuffer::_AssertLayout states), so pin what IS invariant: the interface is
    // the buffer's only payload, seated at its own alignment right after the status byte.
    void InputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PreScene, mInputInterface)
                          == alignof(InputBuffer_PreScene::InputInterface),
                      "mInputInterface must sit at its own alignment immediately after the IOBuffer status byte "
                      "(X360: +4 with 4-byte pointers; x64 host: +8)");
        static_assert(sizeof(InputBuffer_PreScene)
                          == offsetof(InputBuffer_PreScene, mInputInterface)
                             + sizeof(InputBuffer_PreScene::InputInterface),
                      "mInputInterface must be the buffer's only payload member");
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
