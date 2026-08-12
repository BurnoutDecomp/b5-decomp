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
        // HOST layout, not the console's: mpDispatchFrame / mpShadowMap are real 8-byte host
        // pointers now (see the header's POINTER WIDTH note), so the queue and the corona word
        // sit past their console offsets. These tripwires state the HOST fact -- transcribing
        // the console's 4 / 8 / 0xC / 0x801C would be exactly the bug the widening fixes.
        static_assert(offsetof(InputBuffer_Dispatch, mpDispatchFrame) == 8,
                      "mpDispatchFrame @8 (host: 1 status byte + 3 pad + 4 align)");
        static_assert(offsetof(InputBuffer_Dispatch, mpShadowMap)
                          == offsetof(InputBuffer_Dispatch, mpDispatchFrame) + sizeof(void*),
                      "mpShadowMap immediately follows mpDispatchFrame");
        static_assert(offsetof(InputBuffer_Dispatch, mSceneResultQueue)
                          == offsetof(InputBuffer_Dispatch, mpShadowMap) + sizeof(void*),
                      "mSceneResultQueue immediately follows mpShadowMap");
        static_assert(offsetof(InputBuffer_Dispatch, muCoronaSubmissionInterface)
                          > offsetof(InputBuffer_Dispatch, mSceneResultQueue),
                      "muCoronaSubmissionInterface follows the scene-result queue");
    }

    // ========================================================================
    // InputBuffer_Dispatch::Construct   @ 0x822DC300
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-12 (prop-BOOT wave, agent B8). Another never-written Construct: both
    // creation sites (WorldModule::GenerateDispatchLists' stack buffer and the static
    // sPropDispatchInput) call `Construct()`, which resolved to the inherited
    // CgsModule::IOBuffer::Construct, so mSceneResultQueue kept mbIsConstructed == false and
    // every GenerateDispatchLists AddEvent would fire "Not Constructed".
    //
    // The asm is four statements:
    //   stb 1, 0(this)                                    -> IOBuffer::Construct()
    //   bl  VariableEventQueue<32768,16>::Construct(+12)   \ mSceneResultQueue
    //   bl  VariableEventQueue<32768,16>::Clear(+12)       /
    //   stw 0, 4(this) ; stw 0, 8(this)                   -> mpDispatchFrame / mpShadowMap = 0
    // (the two console words at +4/+8 are the host pointers -- see the header's POINTER WIDTH
    //  note -- so they are nulled by name, not by offset).
    // ========================================================================
    void InputBuffer_Dispatch::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mSceneResultQueue.Construct();
        mSceneResultQueue.Clear();

        mpDispatchFrame = 0;
        mpShadowMap     = 0;
    }

    // X360 0x822B8EA8 (R, :295) -- read-lock; return the dispatch frame (console word @this+4).
    CgsGraphics::DispatchFrame* InputBuffer_Dispatch::GetDispatchFrame() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mpDispatchFrame;
    }

    // X360 0x827A1180 (W, :296) -- write-lock; set the dispatch frame (console word @this+4).
    void InputBuffer_Dispatch::SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mpDispatchFrame = lpDispatchFrame;
    }

    // X360 0x822B8F50 (R, :301) -- read-lock; return the shadow map (console word @this+8).
    ShadowMap* InputBuffer_Dispatch::GetShadowMap() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mpShadowMap;
    }

    // X360 0x827A1228 (W, :302) -- write-lock; set the shadow map (console word @this+8).
    void InputBuffer_Dispatch::SetShadowMap(ShadowMap* lpShadowMap)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mpShadowMap = lpShadowMap;
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
