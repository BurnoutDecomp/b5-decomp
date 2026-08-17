#include "GameSource/Graphics/BrnRendererModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// RendererIO::OutputBuffer / RendererIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This bodies the renderer's per-frame data interface: the 35
// OutputBuffer lock-checked accessors (handle getters/setters + the embedded camera/switches/
// perfmon aggregate handles) plus InputBuffer::SetBrnCamera. Every accessor shares the recurring
// CgsModule::IOBuffer lock-guard prologue: it reads the 1-byte status flag and tests a single lock
// bit -- getters test bit 4 (eStatusLockedForRead, "Not locked for reading"), setters test bit 3
// (eStatusLockedForWrite, "Not locked for writing") -- then touches one named member.
//
//   --- OutputBuffer getters (read-lock, status bit 4) ---
//   GetBaseEffectsFrame()                 @ 0x823B3B90 -> mpBaseEffectsFrame             (X360 *(this+9))
//   GetBlobbyShadowBuffer()               @ 0x823B3E90 -> mpBlobbyShadowBuffer           (X360 *(this+17))
//   GetBrnCamera()                        @ 0x823B3650 -> &mBrnCamera                    (X360 this+80)
//   GetCoronaSubmissionInterface()        @ 0x823B3F38 -> mpCoronaSubmissionInteface     (X360 *(this+18))
//   GetDispatchFrame()                    @ 0x823B35A8 -> mpDispatchFrame                (X360 *(this+1))
//   GetExternallyVisiblePerformanceMonitors() @ 0x823F6B38 -> &mExternallyVisiblePerfmons (X360 this+440)
//   GetFXEventsEffectsFrame(slot)         @ 0x823B3D10 -> mapFXEventsEffectsFrames[slot] (X360 *(this+14+slot))
//   GetIm2dDebugRenderBuffer()            @ 0x823B3AE8 -> mpIm2dDebugRenderBuffer        (X360 *(this+6))
//   GetIm2dRenderBuffer()                 @ 0x823B36F8 -> mpIm2dRenderBuffer             (X360 *(this+2))
//   GetIm3dDebugRenderBuffer()            @ 0x823B3A40 -> mpIm3dDebugRenderBuffer        (X360 *(this+5))
//   GetIm3dRenderBuffer()                 @ 0x823B37A0 -> mpIm3dRenderBuffer             (X360 *(this+3))
//   GetIm3dRenderBufferMenusAndHud()      @ 0x823B3998 -> mpIm3dRenderBufferMenusAndHud  (X360 *(this+8))
//   GetIm3dRenderBufferRacePosition()     @ 0x823B38F0 -> mpIm3dRenderBufferRacePosition (X360 *(this+7))
//   GetIm3dRenderBufferUntex()            @ 0x823B3848 -> mpIm3dRenderBufferUntex        (X360 *(this+4))
//   GetRenderSwitches()                   @ 0x823B3FE0 -> &mRenderSwitches               (X360 this+432)
//   GetReusableLoadingScreenAllocator()   @ 0x823B4088 -> mpReusableLoadingScreenAllocator (X360 *(this+121))
//   GetShaderConstantsFrame()             @ 0x823B3DE8 -> mpShaderConstantsFrame         (X360 *(this+16))
//   GetWorldEffectsFrame(slot)            @ 0x823B3C38 -> mapWorldEffectsFrames[slot]    (X360 *(this+10+slot))
//
//   --- OutputBuffer setters (write-lock, status bit 3) ---
//   SetBaseEffectsFrame()                 @ 0x823FB3A0 -> mpBaseEffectsFrame             (X360 this[9])
//   SetBlobbyShadowBuffer()               @ 0x823FB6B0 -> mpBlobbyShadowBuffer           (X360 this[17])
//   SetBrnCamera()                        @ 0x82405980 -> mBrnCamera = lrCamera          (X360 this+80, op=)
//   SetCoronaSubmissionInterface()        @ 0x823FB758 -> mpCoronaSubmissionInteface     (X360 this[18])
//   SetDispatchFrame()                    @ 0x823FAE60 -> mpDispatchFrame                (X360 this[1])
//   SetFXEventsEffectsFrame(slot, frame)  @ 0x823FB528 -> mapFXEventsEffectsFrames[slot] (X360 this[14+slot])
//   SetIm2dDebugRenderBuffer()            @ 0x823FB2F8 -> mpIm2dDebugRenderBuffer        (X360 this[6])
//   SetIm2dRenderBuffer()                 @ 0x823FAF08 -> mpIm2dRenderBuffer             (X360 this[2])
//   SetIm3dDebugRenderBuffer()            @ 0x823FB250 -> mpIm3dDebugRenderBuffer        (X360 this[5])
//   SetIm3dRenderBuffer()                 @ 0x823FAFB0 -> mpIm3dRenderBuffer             (X360 this[3])
//   SetIm3dRenderBufferMenusAndHud()      @ 0x823FB1A8 -> mpIm3dRenderBufferMenusAndHud  (X360 this[8])
//   SetIm3dRenderBufferRacePosition()     @ 0x823FB100 -> mpIm3dRenderBufferRacePosition (X360 this[7])
//   SetIm3dRenderBufferUntex()            @ 0x823FB058 -> mpIm3dRenderBufferUntex        (X360 this[4])
//   SetRenderSwitches()                   @ 0x823FB800 -> mRenderSwitches = *lpSwitches  (X360 6-byte copy to this+432)
//   SetReusableLoadingScreenAllocator()   @ 0x823FB8C8 -> mpReusableLoadingScreenAllocator (X360 this[121])
//   SetShaderConstantsFrame()             @ 0x823FB608 -> mpShaderConstantsFrame         (X360 this[16])
//   SetWorldEffectsFrame(slot, frame)     @ 0x823FB448 -> mapWorldEffectsFrames[slot]    (X360 this[10+slot])
//
//   --- InputBuffer ---
//   InputBuffer::SetBrnCamera()           @ 0x823C8610 -> mBrnCamera = lrCamera          (X360 this+16, op=)
//
// X360 notes:
//   - The X360 lock guard reads the 1-byte IOBuffer status (lbz 0(this)) and tests one bit:
//       (*this >> 4) & 1  == eStatusLockedForRead  (0x10) -> IsBufferLockedForReading()
//       (*this >> 3) & 1  == eStatusLockedForWrite (0x08) -> IsBufferLockedForWriting()
//   - The X360 *(this+N)/this[N] word indices map to the named members by the 32-bit on-disk
//     layout (status word 0; pointer members words 1..18; mBrnCamera at byte 80; mRenderSwitches at
//     byte 432; mExternallyVisiblePerfmons at byte 440; mpReusableLoadingScreenAllocator word 121).
//     On x64 every member is touched BY NAME, never by raw offset.
//   - The slot-validated array accessors (World x4 @<4, FXEvents x2 @<2) keep the X360's "Invalid
//     slot for Fx Layer" range assert ahead of the lock check.
//   - SetRenderSwitches' X360 6-byte byte-copy loop == copying the whole RenderSwitches aggregate;
//     reconstructed as a struct assignment.
//   - The X360-baked d:\p4 BrnRendererModuleIO.h file/line cites are discarded per project policy;
//     CGS_ASSERT carries the stringized condition + __FILE__/__LINE__.

namespace RendererIO
{

// ============================== OutputBuffer getters (read-lock) ==============================

// X360 0x823B35A8: read-lock handle to the dispatch frame.
CgsGraphics::DispatchFrame* OutputBuffer::GetDispatchFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpDispatchFrame;
}

// X360 0x823B36F8: read-lock handle to the immediate-mode 2D render buffer.
CgsGraphics::Im2dRenderBuffer* OutputBuffer::GetIm2dRenderBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm2dRenderBuffer;
}

// X360 0x823B37A0: read-lock handle to the immediate-mode 3D render buffer.
CgsGraphics::Im3dRenderBuffer* OutputBuffer::GetIm3dRenderBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm3dRenderBuffer;
}

// X360 0x823B3848: read-lock handle to the untextured immediate-mode 3D render buffer.
CgsGraphics::Im3dRenderBufferUntex* OutputBuffer::GetIm3dRenderBufferUntex() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm3dRenderBufferUntex;
}

// X360 0x823B3A40: read-lock handle to the debug immediate-mode 3D render buffer.
CgsGraphics::Im3dRenderBuffer* OutputBuffer::GetIm3dDebugRenderBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm3dDebugRenderBuffer;
}

// X360 0x823B3AE8: read-lock handle to the debug immediate-mode 2D render buffer.
CgsGraphics::Im2dRenderBuffer* OutputBuffer::GetIm2dDebugRenderBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm2dDebugRenderBuffer;
}

// X360 0x823B38F0: read-lock handle to the race-position immediate-mode 3D render buffer.
CgsGraphics::Im3dRenderBuffer* OutputBuffer::GetIm3dRenderBufferRacePosition() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm3dRenderBufferRacePosition;
}

// X360 0x823B3998: read-lock handle to the menus-and-HUD immediate-mode 3D render buffer.
CgsGraphics::Im3dRenderBuffer* OutputBuffer::GetIm3dRenderBufferMenusAndHud() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpIm3dRenderBufferMenusAndHud;
}

// X360 0x823B3B90: read-lock handle to the base effects frame.
BrnEffectsFrame* OutputBuffer::GetBaseEffectsFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpBaseEffectsFrame;
}

// X360 0x823B3C38: read-lock handle to a world effects frame. X360 range-asserts the slot (<4)
// ahead of the lock check, then returns mapWorldEffectsFrames[luSlot] (word 10+slot).
BrnEffectsFrame* OutputBuffer::GetWorldEffectsFrame(u8 luSlot) const
{
    CGS_ASSERT(luSlot < 4, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mapWorldEffectsFrames[luSlot];
}

// X360 0x823B3D10: read-lock handle to an FX-events effects frame. X360 range-asserts the slot (<2)
// ahead of the lock check, then returns mapFXEventsEffectsFrames[luSlot] (word 14+slot).
BrnEffectsFrame* OutputBuffer::GetFXEventsEffectsFrame(u8 luSlot) const
{
    CGS_ASSERT(luSlot < 2, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mapFXEventsEffectsFrames[luSlot];
}

// X360 0x823B3DE8: read-lock handle to the shader-constants frame.
BrnShaderConstantsFrame* OutputBuffer::GetShaderConstantsFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpShaderConstantsFrame;
}

// X360 0x823B3E90: read-lock handle to the blobby-shadow buffer.
BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* OutputBuffer::GetBlobbyShadowBuffer() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpBlobbyShadowBuffer;
}

// X360 0x823B3F38: read-lock handle to the corona submission interface.
BrnCoronaManager::BrnSubmissionInterface* OutputBuffer::GetCoronaSubmissionInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpCoronaSubmissionInteface;
}

// X360 0x823B3650: read-lock handle to the embedded director camera (mutable address, this+80).
BrnDirector::Camera::Camera* OutputBuffer::GetBrnCamera()
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mBrnCamera;
}

// X360 0x823B3FE0: read-lock handle to the embedded render switches (mutable address, this+432).
RenderSwitches* OutputBuffer::GetRenderSwitches()
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRenderSwitches;
}

// X360 0x823F6B38: handle to the embedded externally-visible performance monitors (this+440).
// The X360 thunk is a bare `addi r3, this, 440` with no lock guard.
ExternallyVisiblePerformanceMonitors* OutputBuffer::GetExternallyVisiblePerformanceMonitors()
{
    return &mExternallyVisiblePerfmons;
}

// X360 0x823B4088: read-lock handle to the reusable loading-screen allocator.
CgsMemory::LinearMalloc* OutputBuffer::GetReusableLoadingScreenAllocator() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mpReusableLoadingScreenAllocator;
}

// ============================== OutputBuffer setters (write-lock) ==============================

// X360 0x823FAE60: write-lock store of the dispatch frame.
void OutputBuffer::SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpDispatchFrame = lpDispatchFrame;
}

// X360 0x823FAF08: write-lock store of the immediate-mode 2D render buffer.
void OutputBuffer::SetIm2dRenderBuffer(CgsGraphics::Im2dRenderBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm2dRenderBuffer = lpBuffer;
}

// X360 0x823FAFB0: write-lock store of the immediate-mode 3D render buffer.
void OutputBuffer::SetIm3dRenderBuffer(CgsGraphics::Im3dRenderBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm3dRenderBuffer = lpBuffer;
}

// X360 0x823FB058: write-lock store of the untextured immediate-mode 3D render buffer.
void OutputBuffer::SetIm3dRenderBufferUntex(CgsGraphics::Im3dRenderBufferUntex* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm3dRenderBufferUntex = lpBuffer;
}

// X360 0x823FB250: write-lock store of the debug immediate-mode 3D render buffer.
void OutputBuffer::SetIm3dDebugRenderBuffer(CgsGraphics::Im3dRenderBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm3dDebugRenderBuffer = lpBuffer;
}

// X360 0x823FB2F8: write-lock store of the debug immediate-mode 2D render buffer.
void OutputBuffer::SetIm2dDebugRenderBuffer(CgsGraphics::Im2dRenderBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm2dDebugRenderBuffer = lpBuffer;
}

// X360 0x823FB100: write-lock store of the race-position immediate-mode 3D render buffer.
void OutputBuffer::SetIm3dRenderBufferRacePosition(CgsGraphics::Im3dRenderBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm3dRenderBufferRacePosition = lpBuffer;
}

// X360 0x823FB1A8: write-lock store of the menus-and-HUD immediate-mode 3D render buffer.
void OutputBuffer::SetIm3dRenderBufferMenusAndHud(CgsGraphics::Im3dRenderBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpIm3dRenderBufferMenusAndHud = lpBuffer;
}

// X360 0x823FB3A0: write-lock store of the base effects frame.
void OutputBuffer::SetBaseEffectsFrame(BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpBaseEffectsFrame = lpFrame;
}

// X360 0x823FB448: write-lock store of a world effects frame. X360 range-asserts the slot (<4)
// ahead of the lock check, then writes mapWorldEffectsFrames[luSlot] (word 10+slot).
void OutputBuffer::SetWorldEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(luSlot < 4, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mapWorldEffectsFrames[luSlot] = lpFrame;
}

// X360 0x823FB528: write-lock store of an FX-events effects frame. X360 range-asserts the slot (<2)
// ahead of the lock check, then writes mapFXEventsEffectsFrames[luSlot] (word 14+slot).
void OutputBuffer::SetFXEventsEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(luSlot < 2, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mapFXEventsEffectsFrames[luSlot] = lpFrame;
}

// X360 0x823FB608: write-lock store of the shader-constants frame.
void OutputBuffer::SetShaderConstantsFrame(BrnShaderConstantsFrame* lpFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpShaderConstantsFrame = lpFrame;
}

// X360 0x823FB6B0: write-lock store of the blobby-shadow buffer.
void OutputBuffer::SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBuffer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpBlobbyShadowBuffer = lpBuffer;
}

// X360 0x823FB758: write-lock store of the corona submission interface.
void OutputBuffer::SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpCoronaSubmissionInteface = lpInterface;
}

// X360 0x82405980: write-lock copy into the embedded director camera (this+80, Camera::operator=).
void OutputBuffer::SetBrnCamera(const BrnDirector::Camera::Camera& lrCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mBrnCamera = lrCamera;
}

// X360 0x823FB800: write-lock copy into the embedded render switches. The X360 6-byte byte-copy
// loop to this+432 == assigning the whole RenderSwitches aggregate.
void OutputBuffer::SetRenderSwitches(const RenderSwitches& lrSwitches)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mRenderSwitches = lrSwitches;
}

// X360 0x823FB8C8: write-lock store of the reusable loading-screen allocator.
void OutputBuffer::SetReusableLoadingScreenAllocator(CgsMemory::LinearMalloc* lpAllocator)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mpReusableLoadingScreenAllocator = lpAllocator;
}

// ============================== InputBuffer ==============================

// ⭐ @0x82400190 / @0x824001A8 -- the two Construct bodies, added 2026-08-17 with
// BrnRendererModule::Update (boot audit F-P2-4). CreateIOBuffer<T> calls T::Construct, so
// without these the buffer pair could not be carved at all. Both are exactly what this
// header's layout banner already describes them as: set the IOBuffer status byte, then
// InputBuffer constructs its embedded camera and OutputBuffer nulls every owned pointer
// member plus the two effects-frame pointer arrays. Nothing here is inferred beyond that
// description and the member list beside it.
void InputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();
    mBrnCamera.Construct();
}

void OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();
    mpDispatchFrame                = 0;
    mpIm2dRenderBuffer             = 0;
    mpIm3dRenderBuffer             = 0;
    mpIm3dRenderBufferUntex        = 0;
    mpIm3dDebugRenderBuffer        = 0;
    mpIm2dDebugRenderBuffer        = 0;
    mpIm3dRenderBufferRacePosition = 0;
    mpIm3dRenderBufferMenusAndHud  = 0;
    mpBaseEffectsFrame             = 0;
    for (s32 li = 0; li < 4; ++li) mapWorldEffectsFrames[li]    = 0;
    for (s32 li = 0; li < 2; ++li) mapFXEventsEffectsFrames[li] = 0;
    mpShaderConstantsFrame           = 0;
    mpBlobbyShadowBuffer             = 0;
    mpCoronaSubmissionInteface       = 0;
    mpReusableLoadingScreenAllocator = 0;
    mpSnapshotBuffer                 = 0;
}

// X360 0x823C8610: write-lock copy into the embedded director camera (this+16, Camera::operator=).
void InputBuffer::SetBrnCamera(const BrnDirector::Camera::Camera& lrCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mBrnCamera = lrCamera;
}

// The read side, added 2026-08-17 with BrnRendererModule::Update (boot audit F-P2-4). Mirrors
// the OutputBuffer's GetBrnCamera @0x823B3650: read-lock assert, then the embedded camera.
const BrnDirector::Camera::Camera* InputBuffer::GetBrnCamera() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mBrnCamera;
}

}   // namespace RendererIO
