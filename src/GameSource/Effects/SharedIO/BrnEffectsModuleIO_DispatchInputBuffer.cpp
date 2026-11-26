#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\effects\\EffectsModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"   // CGS_ASSERT

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

// ============================== construction ==============================

// X360 0x82288120: default-construct the dispatch input buffer. Asm store order:
//   1. stb 1,0(this)                     -> IOBuffer::Construct() (status = eStatusConstructed)
//   2. stvx128 zero @+0x20/+0x30/+0x40   -> the three light Vector3s cleared (SetZero x3)
//   3. stw 0,0x10(this)                  -> mpDispatchFrame = nullptr
//   4. bl Camera::Clear(this+0x50)       -> mCameraInput.Clear()
//   5. stw 0,4(this)                     -> mpBaseEffectsFrame = nullptr
//   6. loop slot=0,1: stw 0,(slot+2)*4   -> mapFXEventsEffectsFrames[slot] = nullptr
//   7. stw 0,0x1B0(this)                 -> mpEnvironmentMap = nullptr
// mfWhiteLevel (+0x1B4) is deliberately left uninitialised (no store in the asm).
void DispatchInputBuffer::Construct()
{
    IOBuffer::Construct();                 // stb 1, 0(this) -- eStatusConstructed

    mKeyLightDirection.SetZero();          // stvx128 v0, this+0x20
    mKeyLightColour.SetZero();             // stvx128 v0, this+0x30
    mAverageIrradianceColour.SetZero();    // stvx128 v0, this+0x40

    mpDispatchFrame = 0;             // stw 0, 0x10(this)
    mCameraInput.Clear();                  // bl Camera::Clear(this+0x50)
    mpBaseEffectsFrame = 0;          // stw 0, 4(this)

    for (u8 luSlot = 0; luSlot < 2; ++luSlot)   // loop 0x82288174..0x82288190
        mapFXEventsEffectsFrames[luSlot] = 0;   // stwx 0, (slot+2)*4(this)

    mpEnvironmentMap = 0;            // stw 0, 0x1B0(this)
}

// ============================== getters (read-lock, status bit 4) ==============================

// X360 0x8227E328: read-lock handle to the base effects frame.
BrnEffectsFrame* DispatchInputBuffer::GetBaseEffectsFrame() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 373));
    return mpBaseEffectsFrame;
}

// X360 0x8227DE80: read-lock handle to the embedded director camera (this+0x50).
const BrnDirector::Camera::Camera* DispatchInputBuffer::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 244));
    return &mCameraInput;
}

// X360 0x8227DF28: read-lock handle to the environment-map texture (this+0x1B0).
const renderengine::Texture* DispatchInputBuffer::GetEnvironmentMap() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 247));
    return mpEnvironmentMap;
}

// X360 0x8227DFD0: read-lock read of the white-level scalar (this+0x1B4). The asm
// tail `lfs f1,0x1B4(r28)` returns the float in f1 (Hex-Rays mistyped the fall-through).
f32 DispatchInputBuffer::GetWhiteLevel() const
{
    CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for reading\n", CGS_ARTIST_FILE, 250));
    return mfWhiteLevel;
}

// ============================== setters (write-lock, status bit 3) =============================

// X360 0x823BADE8: write-lock store of the base effects frame.
void DispatchInputBuffer::SetBaseEffectsFrame(BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 366));
    mpBaseEffectsFrame = lpFrame;
}

// X360 0x823BAE90: write-lock store of an FX-events effects frame. The X360 range-asserts
// the slot (<2) ahead of the lock check, then writes mapFXEventsEffectsFrames[luSlot]
// (X360 store at this + (slot+2)*4 == +0x8 + slot*4). The "Invalid slot for Fx Layer" rodata
// carries NO trailing \n.
void DispatchInputBuffer::SetFXEventsEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame)
{
    CGS_ASSERT_AT(luSlot < 2, "Invalid slot for Fx Layer", CGS_ARTIST_FILE, 381);
    CGS_ASSERT_AT(IsBufferLockedForWriting(), "Not locked for writing\n", CGS_ARTIST_FILE, 382);
    mapFXEventsEffectsFrames[luSlot] = lpFrame;
}

// X360 0x823C9988: write-lock copy into the embedded director camera (this+0x50,
// Camera::operator=).
void DispatchInputBuffer::SetCameraInput(const BrnDirector::Camera::Camera* lpCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 242));
    mCameraInput = *lpCamera;
}

// X360 0x823BAA98: write-lock store of the environment-map texture (this+0x1B0).
void DispatchInputBuffer::SetEnvironmentMap(const renderengine::Texture* lpTexture)
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 246));
    mpEnvironmentMap = lpTexture;
}

// X360 0x823BAB40: write-lock store of the white-level scalar (this+0x1B4). The X360
// spills the incoming f1 to f31 across the assert call, then stfs it.
void DispatchInputBuffer::SetWhiteLevel(f32 lfWhiteLevel)
{
    CGS_ASSERT(IsBufferLockedForWriting(), CGS_ASSERT_STREAM_MESSAGE_AT("Not locked for writing\n", CGS_ARTIST_FILE, 249));
    mfWhiteLevel = lfWhiteLevel;
}

}   // namespace EffectsIO
}   // namespace BrnEffects

// DecFIGS EffectsModuleIO.cpp:118: the whole body is the base teardown. ARTIST ICF-folds it into
// BrnWorld::PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0 (`b CgsModule::IOBuffer::Destruct`),
// the target DestroyIOBuffer<DispatchInputBuffer> @0x823AED40 calls.
void BrnEffects::EffectsIO::DispatchInputBuffer::Destruct()
{
    CgsModule::IOBuffer::Destruct();
}

#undef CGS_ARTIST_FILE
