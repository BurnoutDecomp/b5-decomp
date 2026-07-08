#pragma once

// ===========================================================================
// XCAM::CI420VideoOutput -- the I420 (planar YUV 4:2:0) specialisation of the
// XCAM (X360 camera/capture) SDK video-output family in BURNOUT_X360_ARTIST.XEX.
// It derives from XCAM::CVideoOutputBase (see XCamVideoOutput.h) and adds the U
// and V chroma planes: a full frame is three planes (Y full-res, U/V half-res),
// each triple-buffered. On top of the base ring it owns two extra texture arrays
// and two extra descriptor arrays, laid out immediately after the base subobject:
//
//   base ends at sizeof(CVideoOutputBase) == 0xB0. The base plane-0 ring is the
//   luma (Y) plane: mpBufferResource[3] @0x7C (Y textures / raw blocks) and
//   mBuffers[3] @0x88 (Y {pitch,data} descriptors).
//
// DERIVED LAYOUT (store-for-store with the X360 asm):
//   +0xB0 mpUTexture[3]  U-plane D3D textures (or raw XMem blocks with no device)
//   +0xBC mpVTexture[3]  V-plane D3D textures (or raw XMem blocks with no device)
//   +0xC8 mUBuffers[3]   U-plane {pitch,data} descriptors  (SVideoBuffer)
//   +0xE0 mVBuffers[3]   V-plane {pitch,data} descriptors  (SVideoBuffer)
//   sizeof(CI420VideoOutput) == 0xF8
//
// Attestation for the offsets: GetBuffersForWriting/Render index the three planes
// as 8-byte descriptor slots at (writeIndex+17) [Y @0x88], (writeIndex+25)
// [U @0xC8] and (writeIndex+28) [V @0xE0]; Initialize stores the U/V textures at
// 0xB0/0xBC (r30=this+0xB0, r30+0xC) and LockRect fills the descriptors at
// 0x88/0xC8/0xE0. There is no reference source and no DWARF for this TU; the
// layout is reconstructed from the asm. `XCAM` is an X360 SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamVideoOutput.h"

namespace XCAM
{

// --- Xbox-360 D3D9 platform object types (opaque handles) ------------------
// Both derive from D3DResource (defined in the base header) so the by-name
// Release / SetTexture calls upcast exactly as the asm does. External XDK types.
struct D3DTexture     : D3DResource {};
struct D3DPixelShader : D3DResource {};

// --- Xbox-360 XDK entry points used only by the I420 output (platform boundary)
// Declared with the argument/return shapes the X360 call sites use so the
// reconstructed bodies compile store-for-store. Supplied by the platform layer;
// NOT reconstructed here.
void*           XMemAlloc(u32 uSize, u32 uAttributes);
D3DPixelShader* D3DDevice_CreatePixelShader(const void* pFunction);
D3DTexture*     D3DDevice_CreateTexture(u32 uWidth, u32 uHeight, u32 uDepth, u32 uLevels,
                                        u32 uUsage, u32 uFormat, u32 uUnusedPool, u32 uD3DType);
void            D3DTexture_LockRect(D3DResource* pThis, u32 uLevel, SVideoBuffer* pLockedRect,
                                    const void* pRect, u32 uFlags);
void            D3DTexture_UnlockRect(D3DResource* pThis, u32 uLevel);

// Draw-time device state setters (all inlined at the X360 call sites).
void D3DDevice_SetVertexDeclaration(D3DDevice* pDevice, D3DVertexDeclaration* pDecl);
void D3DDevice_SetVertexShader(D3DDevice* pDevice, D3DVertexShader* pShader);
void D3DDevice_SetPixelShader(D3DDevice* pDevice, D3DPixelShader* pShader);
u32  D3DDevice_SetTexture(D3DDevice* pDevice, u32 uSampler, D3DResource* pTexture, u32 uFlags);
void D3DDevice_SetSamplerState_MinFilter(D3DDevice* pDevice, u32 uSampler, u32 uValue);
void D3DDevice_SetSamplerState_MagFilter(D3DDevice* pDevice, u32 uSampler, u32 uValue);
void D3DDevice_SetRenderState_ZEnable(D3DDevice* pDevice, u32 uValue);
void D3DDevice_SetRenderState_CullMode(D3DDevice* pDevice, u32 uValue);
void D3DDevice_SetRenderState_ViewportEnable(D3DDevice* pDevice, u32 uValue);
void D3DDevice_DrawVertices(D3DDevice* pDevice, u32 uPrimitiveType, u32 uStartVertex, u32 uVertexCount);

// Pixel-shader microcode blob (dword_8210BED0) handed to D3DDevice_CreatePixelShader.
// Address-only load in the X360 image; the bytes are unrecoverable so the symbol
// is declared and NOT defined.
extern const u8 gXCamI420PixelShaderCode[];

// Texture format word baked into the CreateTexture calls (`lis r11,0x2800 ;
// ori r29,r11,2` -> 0x28000002): the tiled 8-bit single-channel plane format.
static const u32 KU_XCAM_I420_PLANE_FORMAT = 0x28000002u;

// D3DType argument to CreateTexture (`li r10,3`).
static const u32 KU_XCAM_I420_TEXTURE_TYPE = 3u;

// Attribute word for the raw (no-device) plane allocations, matching the base's
// XMemFree attributes (`lis r4,0x6090 ; ori r4,r4,0x2068` -> 0x60902068).
static const u32 KU_XCAM_I420_XMEM_ALLOC_ATTRIBUTES = KU_XCAM_XMEM_FREE_ATTRIBUTES;

// Status returned by GetNextBuffer when no completed frame is ready (`li r3,0x3E5`).
static const int KI_XCAM_NO_BUFFER_READY = 997;

class CI420VideoOutput : public CVideoOutputBase
{
public:
    // @ 0x82985DB0 -- forward (device,width,height) to the base, then create the
    // three planes x three ring slots (D3D textures when a device is present, else
    // raw XMem blocks), seed the descriptors and Reset. Returns 0, or 8 on any
    // resource-allocation failure (KI_XCAM_VIDEO_OUT_OF_RESOURCES).
    int Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight);

    // @ 0x82985018 -- release the U/V-plane textures (or XMemFree the raw blocks),
    // then chain to the base to release the Y plane and the quad resources.
    void Shutdown();

    // @ 0x829850D0 -- bind the quad + the ready frame's three plane textures and
    // draw the screen-aligned quad, then unbind everything.
    void Render();

    // @ 0x82984698 -- clear all three ring slots (Y -> 0, U/V -> 0x80 neutral
    // chroma) and re-arm the producer cursor (write=1, ready=0, ready-flag=0).
    void Reset();

    // @ 0x82984790 -- consume the ready frame under the base helper and copy its
    // Y/U/V planes into the caller's three buffers. Returns 0, or 997 if no frame
    // is ready.
    int GetNextBuffer(SVideoBuffer* pY, SVideoBuffer* pU, SVideoBuffer* pV);

    // @ 0x82984728 -- copy the current write-slot's three plane descriptors into
    // the caller's three buffers (pointers into the ring, no data copy).
    void GetBuffersForWriting(SVideoBuffer* pY, SVideoBuffer* pU, SVideoBuffer* pV);

private:
    D3DResource* mpUTexture[3]; // +0xB0  U-plane textures / raw blocks
    D3DResource* mpVTexture[3]; // +0xBC  V-plane textures / raw blocks
    SVideoBuffer mUBuffers[3];  // +0xC8  U-plane {pitch,data} descriptors
    SVideoBuffer mVBuffers[3];  // +0xE0  V-plane {pitch,data} descriptors
};

} // namespace XCAM
