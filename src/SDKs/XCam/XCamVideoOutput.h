#pragma once

// ===========================================================================
// XCAM::CVideoOutputBase -- the shared base of the XCAM (X360 camera/capture)
// SDK video-output classes (CYUY2VideoOutput / CI420VideoOutput) in
// BURNOUT_X360_ARTIST.XEX. It owns a triple-buffered producer/consumer ring of
// frame buffers (guarded by a kernel spin-lock) and a screen-aligned textured
// quad (a D3D9 vertex declaration + vertex shader + vertex buffer) used to blit
// the decoded video into a render rect. This header is the canonical OWNING
// home for the reconstructed bodies in XCamVideoOutput.cpp:
//
//     XCAM::CVideoOutputBase::GetBufferForWriting    @ 0x82984450
//     XCAM::CVideoOutputBase::BufferWriteSucceeded   @ 0x82984478
//     XCAM::CVideoOutputBase::GetNextBufferHelper    @ 0x829844E8
//     XCAM::CVideoOutputBase::CopyBufferHelper       @ 0x82984538  (static)
//     XCAM::CVideoOutputBase::Shutdown               @ 0x829848B8
//     XCAM::CVideoOutputBase::UpdateVertexBuffer     @ 0x82984998
//     XCAM::CVideoOutputBase::Initialize             @ 0x82985AA0
//     XCAM::CVideoOutputBase::SetRenderRect          @ 0x82985B80
//
// There is no reference source and no DWARF for this TU; the layout below is
// reconstructed store-for-store from the X360 asm. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// LAYOUT (store-for-store with the asm; byte offsets pinned in the .cpp):
//   +0x00 mpDevice             D3D device (AddRef'd in Initialize)
//   +0x04 mpVertexDeclaration  D3D vertex declaration for the quad
//   +0x08 mpVertexShader       D3D vertex shader for the quad
//   +0x0C mpResource0C         extra D3D resource (allocated by the derived
//                              class; released here)
//   +0x10 mpVertexBuffer       80-byte D3D vertex buffer (the quad)
//   +0x14 mRenderRect          integer destination rect {l,t,r,b}
//   +0x24 mVertices[4]         the quad: {x,y,z,u,v} x4 = 80 bytes
//   +0x74 miWidth              frame width  (Initialize arg 3)
//   +0x78 miHeight             frame height (Initialize arg 4)
//   +0x7C mpBufferResource[3]  the 3 ring backing allocations -- D3D resources
//                              when a device is present, else raw XMem blocks
//   +0x88 mBuffers[3]          the 3 ring descriptors {pitch,data}
//   +0xA0 muWriteIndex         producer cursor (wraps at 3)
//   +0xA4 muReadyIndex         last completed buffer index
//   +0xA8 mbBufferReady        byte flag: a completed buffer is waiting
//   +0xAC mSpinLock            kernel spin-lock guarding the ring cursors
// ===========================================================================

#include "types.hpp"

namespace XCAM
{

// --- Xbox-360 D3D9 platform object types (opaque handles) ------------------
// Modelled with the D3D COM-style resource hierarchy the X360 image assumes
// (every buffer/shader is a D3DResource), so the by-name Release calls upcast
// exactly as the asm does. These are external XDK types; only the pointer
// relationship the call sites rely on is expressed here.
struct D3DResource {};
struct D3DDevice {};
struct D3DVertexBuffer      : D3DResource {};
struct D3DVertexDeclaration : D3DResource {};
struct D3DVertexShader      : D3DResource {};

// --- Xbox-360 kernel spin-lock primitives (platform boundary) --------------
typedef u32 KSPIN_LOCK;
typedef u8  KIRQL;
KIRQL KfAcquireSpinLock(KSPIN_LOCK* pSpinLock);
void  KfReleaseSpinLock(KSPIN_LOCK* pSpinLock, KIRQL uNewIrql);

// --- Xbox-360 XDK D3D device / resource entry points (platform boundary) ---
// Declared with the argument/return shapes the X360 call sites use so the
// reconstructed bodies compile store-for-store. Supplied by the platform layer;
// NOT reconstructed here.
u32                   D3DDevice_AddRef(D3DDevice* pDevice);
u32                   D3DDevice_Release(D3DDevice* pDevice);
u32                   D3DResource_Release(D3DResource* pResource);
D3DVertexDeclaration* D3DDevice_CreateVertexDeclaration(const void* pVertexElements);
D3DVertexShader*      D3DDevice_CreateVertexShader(const void* pFunction);
D3DVertexBuffer*      D3DDevice_CreateVertexBuffer(u32 uLength, u32 uUsage, u32 uUnusedPool);
D3DVertexBuffer*      D3DDevice_GetStreamSource(D3DDevice* pDevice, u32 uStreamNumber,
                                                u32* pOffsetInBytes, u32* pStride);
u32                   D3DDevice_SetStreamSource(D3DDevice* pDevice, u32 uStreamNumber,
                                                D3DVertexBuffer* pStreamData, u32 uOffsetInBytes,
                                                u32 uStride, u32 uUnused);
void*                 D3DVertexBuffer_Lock(D3DVertexBuffer* pThis, u32 uOffsetToLock,
                                           u32 uSizeToLock, u32 uFlags);
void                  D3DVertexBuffer_Unlock(D3DVertexBuffer* pThis);

// --- Xbox-360 XDK heap free (platform boundary) ----------------------------
void XMemFree(void* pAddress, u32 uAttributes);

// The attribute word baked into the no-device Shutdown path (`lis r4,0x6090 ;
// ori r4,r4,0x2068` -> 0x60902068) when freeing a ring block through XMemFree.
static const u32 KU_XCAM_XMEM_FREE_ATTRIBUTES = 0x60902068u;

// Failure code returned by Initialize when a D3D resource cannot be created.
static const int KI_XCAM_VIDEO_OUT_OF_RESOURCES = 8;

// --- opaque rodata blobs (contents UNATTESTED; extern-only, never fabricated)
// The X360 image loads the ADDRESS of each blob and hands it to the D3D create
// calls. Declared as extern byte arrays so the symbol names the blob; the bytes
// themselves are not recoverable, so they are declared and NOT defined.
extern const u8 gXCamVideoVertexElements[];   // unk_82F3E374  (D3DVERTEXELEMENT9[])
extern const u8 gXCamVideoVertexShaderCode[]; // dword_8210BDE0 (vertex-shader microcode)

// One (pitch, data) frame-buffer descriptor stored in the triple ring. 8-byte
// stride is attested by the `8 * (index + 17)` walk in GetBufferForWriting and
// the +0/+4 loads in CopyBufferHelper.
struct SVideoBuffer
{
    u32   muPitch; // +0  row pitch in bytes
    void* mpData;  // +4  pixel data
};

// Integer destination rectangle programmed by SetRenderRect.
struct SVideoRect
{
    s32 iLeft;   // +0
    s32 iTop;    // +4
    s32 iRight;  // +8
    s32 iBottom; // +C
};

// One vertex of the output quad: position + UV. 20-byte stride is attested by
// SetRenderRect writing x at 0x24/0x38/0x4C/0x60 (i.e. +20 apart).
struct SVideoVertex
{
    f32 fX; // +0
    f32 fY; // +4
    f32 fZ; // +8  (never written; carried from construction)
    f32 fU; // +C
    f32 fV; // +10
};

class CVideoOutputBase
{
public:
    // @ 0x82985AA0 -- bind the device, create the quad's D3D resources, seed the
    // static UVs and upload, then clear the ring spin-lock. Returns 0 on success
    // or KI_XCAM_VIDEO_OUT_OF_RESOURCES if any D3D resource fails to create.
    int  Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight);

    // @ 0x829848B8 -- release every D3D resource (or, when no device was bound,
    // XMemFree the raw ring blocks).
    void Shutdown();

    // @ 0x82985B80 -- store the integer render rect, project it onto the four
    // quad corners and re-upload the vertex buffer.
    void SetRenderRect(const SVideoRect* pRect);

    // @ 0x82984998 -- copy the local vertices into the (temporarily unbound)
    // D3D vertex buffer, then restore the previous stream source.
    void UpdateVertexBuffer();

    // @ 0x82984450 -- copy the descriptor of the buffer at the current write
    // cursor into *pOut.
    void GetBufferForWriting(SVideoBuffer* pOut);

    // @ 0x82984478 -- publish the just-written buffer: mark it ready, record its
    // index and advance the write cursor (wrapping at 3), under the spin-lock.
    void BufferWriteSucceeded();

    // @ 0x829844E8 -- consume the ready buffer under the spin-lock, returning its
    // index; returns 3 (no buffer) when nothing is ready.
    int  GetNextBufferHelper();

    // @ 0x82984538 -- copy uRowCount rows of uRowSize bytes from pSrc to pDst,
    // honouring each descriptor's pitch (single memcpy when the pitches match).
    static void* CopyBufferHelper(SVideoBuffer* pSrc, SVideoBuffer* pDst,
                                  u32 uRowSize, u32 uRowCount);

    // `protected`, not `private`: the concrete derived outputs (CI420VideoOutput,
    // CYUY2VideoOutput) share this prefix and reach these members by name -- their
    // asm loads/stores land inside this base subobject (mpDevice @0x00, the quad
    // resources @0x04..0x10, miWidth/miHeight @0x74/0x78, the plane-0 ring at
    // mpBufferResource @0x7C / mBuffers @0x88, and the shared cursors @0xA0..0xA8).
protected:
    D3DDevice*            mpDevice;            // +0x00
    D3DVertexDeclaration* mpVertexDeclaration; // +0x04
    D3DVertexShader*      mpVertexShader;      // +0x08
    D3DResource*          mpResource0C;        // +0x0C
    D3DVertexBuffer*      mpVertexBuffer;      // +0x10
    SVideoRect            mRenderRect;         // +0x14
    SVideoVertex          mVertices[4];        // +0x24
    s32                   miWidth;             // +0x74
    s32                   miHeight;            // +0x78
    D3DResource*          mpBufferResource[3]; // +0x7C
    SVideoBuffer          mBuffers[3];         // +0x88
    u32                   muWriteIndex;        // +0xA0
    u32                   muReadyIndex;        // +0xA4
    u8                    mbBufferReady;       // +0xA8
    u8                    mPad_A9[3];          // +0xA9  (align mSpinLock)
    KSPIN_LOCK            mSpinLock;           // +0xAC
};

} // namespace XCAM
