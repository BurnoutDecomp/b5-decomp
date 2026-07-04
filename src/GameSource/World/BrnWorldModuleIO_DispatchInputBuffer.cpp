// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.cpp
//
// Out-of-line bodies for the 19 class:BrnWorldIO::DispatchInputBuffer accessors/mutators the X360
// build emitted for the World module's per-frame graphics-dispatch INPUT buffer. Each body tests
// the inherited IOBuffer status flag then acts on the named member:
//   - const getters assert the read-lock  ("Not locked for reading\n")  and return the member;
//   - setters assert the write-lock ("Not locked for writing\n") and perform the store/copy.
// (The X360 rodata carries the trailing \n on both lock strings -- matched verbatim.) The
// slot-range assert on GetEffectsFrame/SetEffectsFrame ("Invalid slot for Fx Layer") carries NO
// trailing newline (X360 rodata) and precedes the lock check, matching the asm order.
// ============================================================================
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorldIO
{

// ---- dispatch frame ---------------------------------------------------------

// X360 0x827A4EB0 (:383 R) -- read-lock handle to the graphics dispatch frame (this+0x04).
CgsGraphics::DispatchFrame* DispatchInputBuffer::GetDispatchFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpDispatchFrame;
}

// X360 0x823B6B30 -- write-lock store of the graphics dispatch frame (this+0x04).
void DispatchInputBuffer::SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpDispatchFrame = lpDispatchFrame;
}

// ---- director camera (embedded by value) ------------------------------------

// X360 0x827A4248 (:385 R) -- read-lock handle to the embedded director camera (this+0x10).
const BrnDirector::Camera::Camera* DispatchInputBuffer::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mCameraInput;
}

// X360 0x823C8F28 -- write-lock copy into the embedded director camera
// (this+0x10, BrnDirector::Camera::Camera::operator=).
void DispatchInputBuffer::SetCameraInput(const BrnDirector::Camera::Camera* lpCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mCameraInput = *lpCamera;
}

// ---- shader-constants frame -------------------------------------------------

// X360 0x827BBEF0 -- read-lock handle to the shader-constants frame (this+0x170).
BrnShaderConstantsFrame* DispatchInputBuffer::GetShaderConstantsFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpShaderConstantsFrame;
}

// X360 0x823B50B0 -- write-lock store of the shader-constants frame (this+0x170).
void DispatchInputBuffer::SetShaderConstantsFrame(BrnShaderConstantsFrame* lpFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpShaderConstantsFrame = lpFrame;
}

// ---- per-Fx-layer effects frames (slot<4) -----------------------------------

// X360 0x827BCCD8 (:393 R) -- read-lock handle to a per-Fx-layer effects frame
// (this + 0x174 + slot*4). The slot range-assert (<4) precedes the buffer read-lock check,
// matching the asm order; "Invalid slot for Fx Layer" carries NO trailing newline.
BrnEffectsFrame* DispatchInputBuffer::GetEffectsFrame(u8 luSlot) const
{
    CGS_ASSERT(luSlot < 4, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mapEffectsFrames[luSlot];
}

// X360 0x823B6BD8 -- write-lock store of an effects frame into a numbered FX layer slot. The X360
// range-asserts the slot (<4, "Invalid slot for Fx Layer", no trailing newline) ahead of the lock
// check, then writes mapEffectsFrames[luSlot] (this + 0x174 + slot*4).
void DispatchInputBuffer::SetEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(luSlot < 4, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mapEffectsFrames[luSlot] = lpFrame;
}

// ---- blobby-shadow submission buffer ----------------------------------------

// X360 0x827A42F0 (:398 R) -- read-lock handle to the blobby-shadow submission buffer (this+0x184).
BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* DispatchInputBuffer::GetBlobbyShadowBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpBlobbyShadowBuffer;
}

// X360 0x823B5158 -- write-lock store of the blobby-shadow buffer (this+0x184).
void DispatchInputBuffer::SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpBlobbyShadowBuffer = lpBuffer;
}

// ---- corona submission interface --------------------------------------------

// X360 0x827A4398 (:401 R) -- read-lock handle to the corona-submission interface (this+0x188).
BrnCoronaManager::BrnSubmissionInterface* DispatchInputBuffer::GetCoronaSubmissionInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpCoronaSubmissionInterface;
}

// X360 0x823B5200 -- write-lock store of the corona submission interface (this+0x188).
void DispatchInputBuffer::SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpCoronaSubmissionInterface = lpInterface;
}

// ---- game/sim time scalars --------------------------------------------------

// X360 0x827BBF98 -- read-lock read of the game-time scalar (this+0x18C).
f32 DispatchInputBuffer::GetGameTime() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mfGameTime;
}

// X360 0x823B52A8 -- write-lock store of the game-time scalar (this+0x18C).
void DispatchInputBuffer::SetGameTime(f32 lfGameTime)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mfGameTime = lfGameTime;
}

// X360 0x827BC040 -- read-lock read of the sim-time scalar (this+0x190).
f32 DispatchInputBuffer::GetSimTime() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mfSimTime;
}

// X360 0x823B5358 -- write-lock store of the simulation-time scalar (this+0x190).
void DispatchInputBuffer::SetSimTime(f32 lfSimTime)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mfSimTime = lfSimTime;
}

// ---- dispatch-thread input buffer handle ------------------------------------

// X360 0x827BC0E8 (:411 R) -- read-lock handle to the dispatch-thread input buffer (this+0x194).
BrnGame::DispatchThreadInputBuffer* DispatchInputBuffer::GetDispatchThreadInputBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpDispatchThreadInputBuffer;
}

// X360 0x823B5408 -- write-lock store of the dispatch-thread input buffer handle (this+0x194).
void DispatchInputBuffer::SetDispatchThreadInputBuffer(BrnGame::DispatchThreadInputBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpDispatchThreadInputBuffer = lpBuffer;
}

// ---- per-frame render switches (embedded by value) --------------------------

// X360 0x823B6CB8 (:438 W) -- write-lock copy of the per-frame render switches (this+0x198). The
// X360 tail is a 6-byte byte-copy loop (ctr=6, lbz/stb) -- a member-wise copy of the 6-bool
// RenderSwitches struct, reproduced here as struct assignment.
void DispatchInputBuffer::SetRenderSwitches(const RendererIO::RenderSwitches& lrSwitches)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mRenderSwitches = lrSwitches;
}

}   // namespace BrnWorldIO
