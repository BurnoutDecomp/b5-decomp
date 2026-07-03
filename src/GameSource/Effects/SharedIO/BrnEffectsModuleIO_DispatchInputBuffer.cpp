#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnEffects::EffectsIO::DispatchInputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The per-frame input payload the effects dispatch pass reads while
// building the graphics DispatchFrame. Named members accessed BY NAME (the committed
// RendererIO::OutputBuffer pattern); the byte offsets in the header banner are X360-pinned.
// Every accessor shares the recurring CgsModule::IOBuffer lock-guard prologue: getters test
// bit 4 (eStatusLockedForRead, "Not locked for reading\n"), setters test bit 3
// (eStatusLockedForWrite, "Not locked for writing\n"). (These Effects lock strings carry the
// trailing \n per the X360 rodata.)

namespace BrnEffects
{
namespace EffectsIO
{

// ============================== getters (read-lock, status bit 4) ==============================

// X360 0x8227E328: read-lock handle to the base effects frame.
BrnEffectsFrame* DispatchInputBuffer::GetBaseEffectsFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpBaseEffectsFrame;
}

// X360 0x8227DE80: read-lock handle to the embedded director camera (this+0x50).
const BrnDirector::Camera::Camera* DispatchInputBuffer::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mCameraInput;
}

// X360 0x8227DF28: read-lock handle to the environment-map texture (this+0x1B0).
const renderengine::Texture* DispatchInputBuffer::GetEnvironmentMap() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpEnvironmentMap;
}

// X360 0x8227DFD0: read-lock read of the white-level scalar (this+0x1B4). The asm
// tail `lfs f1,0x1B4(r28)` returns the float in f1 (Hex-Rays mistyped the fall-through).
f32 DispatchInputBuffer::GetWhiteLevel() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mfWhiteLevel;
}

// ============================== setters (write-lock, status bit 3) =============================

// X360 0x823BADE8: write-lock store of the base effects frame.
void DispatchInputBuffer::SetBaseEffectsFrame(BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpBaseEffectsFrame = lpFrame;
}

// X360 0x823BAE90: write-lock store of an FX-events effects frame. The X360 range-asserts
// the slot (<2) ahead of the lock check, then writes mapFXEventsEffectsFrames[luSlot]
// (X360 store at this + (slot+2)*4 == +0x8 + slot*4). The "Invalid slot for Fx Layer" rodata
// carries NO trailing \n.
void DispatchInputBuffer::SetFXEventsEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(luSlot < 2, "Invalid slot for Fx Layer");
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mapFXEventsEffectsFrames[luSlot] = lpFrame;
}

// X360 0x823C9988: write-lock copy into the embedded director camera (this+0x50,
// Camera::operator=).
void DispatchInputBuffer::SetCameraInput(const BrnDirector::Camera::Camera* lpCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mCameraInput = *lpCamera;
}

// X360 0x823BAA98: write-lock store of the environment-map texture (this+0x1B0).
void DispatchInputBuffer::SetEnvironmentMap(const renderengine::Texture* lpTexture)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpEnvironmentMap = lpTexture;
}

// X360 0x823BAB40: write-lock store of the white-level scalar (this+0x1B4). The X360
// spills the incoming f1 to f31 across the assert call, then stfs it.
void DispatchInputBuffer::SetWhiteLevel(f32 lfWhiteLevel)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mfWhiteLevel = lfWhiteLevel;
}

}   // namespace EffectsIO
}   // namespace BrnEffects
