// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO_InputBuffer_Dispatch.cpp
//
// Out-of-line bodies for the BrnWorld::PropEntityIO::InputBuffer_Dispatch scalar accessors the
// X360 ARTIST build emitted out-of-line. Each guarded accessor asserts the buffer's lock bit
// then reads/writes a 32-bit word member:
//   read-lock  (status>>4 &1) => IsBufferLockedForReading()  (getters)
//   write-lock (status>>3 &1) => IsBufferLockedForWriting()  (setters)
// Field offsets are layout-derived (member-by-name), not hardcoded. CGS_ASSERT stamps
// __FILE__/__LINE__, so the X360-baked GameSource/.../BrnPropEntityModuleIO.h path/lines
// (295/296/301/302/304/305) are intentionally not reproduced.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

namespace BrnWorld
{
namespace PropEntityIO
{
    void InputBuffer_Dispatch::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_Dispatch, muDispatchFrame) == 4,
                      "muDispatchFrame @4");
        static_assert(offsetof(InputBuffer_Dispatch, muShadowMap) == 8,
                      "muShadowMap @8");
        static_assert(offsetof(InputBuffer_Dispatch, mSceneResultQueue) == 0xC,
                      "mSceneResultQueue @0xC");
        static_assert(offsetof(InputBuffer_Dispatch, muCoronaSubmissionInterface) == 0x801C,
                      "muCoronaSubmissionInterface @0x801C");
    }

    // X360 0x822B8EA8 (R, :295) -- read-lock; return the dispatch-frame index (this+4).
    u32 InputBuffer_Dispatch::GetDispatchFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return muDispatchFrame;
    }

    // X360 0x827A1180 (W, :296) -- write-lock; set the dispatch-frame index (this+4).
    void InputBuffer_Dispatch::SetDispatchFrame(u32 luDispatchFrame)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        muDispatchFrame = luDispatchFrame;
    }

    // X360 0x822B8F50 (R, :301) -- read-lock; return the shadow-map handle (this+8).
    u32 InputBuffer_Dispatch::GetShadowMap() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return muShadowMap;
    }

    // X360 0x827A1228 (W, :302) -- write-lock; set the shadow-map handle (this+8).
    void InputBuffer_Dispatch::SetShadowMap(u32 luShadowMap)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        muShadowMap = luShadowMap;
    }

    // X360 0x827BB1E0 (W, :296) -- write-lock; return the scene-query-results queue (this+0xC).
    // Called by BrnWorld::PropEntityModule::GenerateDispatchLists /
    // GenerateShadowMapDispatchLists (WorldModule::GenerateDispatchLists path).
    InputBuffer_Dispatch::SceneResultQueueStorage* InputBuffer_Dispatch::GetSceneResultQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneResultQueue;
    }

    // X360 0x822B8FF8 (R, :304) -- read-lock; return the corona-submission interface handle
    // (this+0x801C).
    u32 InputBuffer_Dispatch::GetCoronaSubmissionInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return muCoronaSubmissionInterface;
    }

    // X360 0x827A12D0 (W, :305) -- write-lock; set the corona-submission interface handle
    // (this+0x801C).
    void InputBuffer_Dispatch::SetCoronaSubmissionInterface(u32 luCoronaSubmissionInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        muCoronaSubmissionInterface = luCoronaSubmissionInterface;
    }
}
}
