#pragma once

// ===========================================================================
// XCAM::CDecoderOutput -- the decoder-output stage of the XCAM (X360 camera /
// remote-console capture) SDK in BURNOUT_X360_ARTIST.XEX. It derives from
// XCAM::CI420VideoOutput (see XCamI420VideoOutput.h) and layers a
// "stream-liveness" watchdog on top of the I420 YUV pipeline: it timestamps the
// last few decoded frames and, when the remote feed stalls (no frame for ~2 s),
// falls back to blitting a caller-supplied default RGB texture instead of the
// (now stale) YUV planes. The fallback is drawn with its own single-sampler RGB
// pixel shader.
//
// DERIVED LAYOUT (store-for-store with the X360 asm; base CI420VideoOutput ends
// at sizeof == 0xF8):
//   +0xF8 mbStreamStale       nonzero once the feed is judged stale (set by
//                             Render on a >2 s gap, cleared by Reset/Initialize,
//                             re-cleared by BufferWriteSucceeded once frames flow
//                             again). Stored/compared as a 32-bit word.
//   +0xFC mauFrameTicks[5]    5-slot FIFO of GetTickCount() stamps; [0] newest
//                             (+0xFC), [4] oldest (+0x10C).
//   +0x110 mpDefaultTexture   fallback RGB texture shown while the feed is stale
//   +0x114 mpRGBPixelShader   RGB pixel shader used to draw the fallback quad
//   sizeof(CDecoderOutput) == 0x118
//
// Attestation: Initialize/BufferWriteSucceeded walk the tick FIFO as words at
// this+0xFC..0x10C; the stale flag is the word at this+0xF8; the default texture
// is the pointer at this+0x110 (0x110(r31)); the RGB pixel shader is the pointer
// at this+0x114 (0x114(r31)). There is no reference source and no DWARF for this
// TU; the layout is reconstructed from the asm. `XCAM` is an X360 SDK boundary,
// so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamI420VideoOutput.h"

namespace XCAM
{

// --- Xbox-360 XDK entry points used only by the decoder output (platform boundary)
// Declared with the argument/return shapes the X360 call sites use so the
// reconstructed bodies compile store-for-store. Supplied by the platform layer;
// NOT reconstructed here. (GetTickCount / the D3D device-state setters and
// D3DResource_Release / D3DDevice_CreatePixelShader come in via the base and
// I420 headers.)
extern "C" u32 GetTickCount();
u32 D3DResource_AddRef(D3DResource* pResource);

// RGB fallback pixel-shader microcode blob (dword_8210C210) handed to
// D3DDevice_CreatePixelShader. Address-only load in the X360 image; the bytes are
// unrecoverable so the symbol is declared and NOT defined.
extern const u8 gXCamRGBPixelShaderCode[];

// The watchdog stall threshold in milliseconds (`cmplwi ...,0x7D0` == 2000).
static const u32 KU_XCAM_STREAM_STALE_MS = 0x7D0u;

// Number of frame timestamps kept in the liveness FIFO (loop count in
// Initialize/BufferWriteSucceeded covers this+0xFC..0x10C inclusive).
static const int KI_XCAM_FRAME_TICK_HISTORY = 5;

class CDecoderOutput : public CI420VideoOutput
{
public:
    // @ 0x82986018 -- forward (device,width,height) to CI420VideoOutput; on
    // success seed the tick FIFO with GetTickCount(), clear the stale flag and the
    // default texture, and (when a device is present) create the RGB fallback
    // pixel shader. Returns 0, or 8 (KI_XCAM_VIDEO_OUT_OF_RESOURCES) if that shader
    // fails to create.
    int  Initialize(D3DDevice* pDevice, s32 iWidth, s32 iHeight);

    // @ 0x829857D0 -- release the default texture and the RGB pixel shader (only
    // when a device was bound), then chain to CI420VideoOutput::Shutdown().
    void Shutdown();

    // @ 0x82985840 -- clear the stale flag, release + drop the default texture,
    // then chain to CI420VideoOutput::Reset().
    void Reset();

    // @ 0x829858F0 -- mark the feed stale if the newest frame is older than ~2 s;
    // when stale and a default texture is set, draw the fallback RGB quad, else
    // defer to CI420VideoOutput::Render() (the live YUV path).
    void Render();

    // @ 0x82984838 -- publish the just-written buffer via the base helper, push a
    // fresh GetTickCount() stamp onto the FIFO, and (once already stale) re-clear
    // the stale flag when the last few frames arrived within ~2 s.
    void BufferWriteSucceeded();

    // @ 0x82985898 -- swap the fallback RGB texture: release the old one, store and
    // AddRef the new one (COM-style set).
    void SetDefaultRGBTexture(D3DTexture* pTexture);

private:
    u32         mbStreamStale;                        // +0xF8
    u32         mauFrameTicks[KI_XCAM_FRAME_TICK_HISTORY]; // +0xFC .. +0x10C
    D3DTexture* mpDefaultTexture;                     // +0x110
    D3DPixelShader* mpRGBPixelShader;                 // +0x114
};

} // namespace XCAM
