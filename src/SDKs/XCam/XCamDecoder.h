#pragma once

// ===========================================================================
// XCAM::CDecoder -- the video DECODER in the XCAM (X360 camera / remote-console
// capture) SDK of BURNOUT_X360_ARTIST.XEX. It wraps a Windows-Media-Video
// ('WMVR') decoder session and drives the decoded planar-YUV frames into an
// embedded CDecoderOutput (the I420 output stage with the stream-liveness
// watchdog, see XCamDecoderOutput.h). Encoded data is pulled from a caller-
// supplied COM-style "data source" object through the WMVDecCBGetData_HANDLER
// callback (homed in XCamConfig.h); after every DecodeData the decoder pings
// that same object through its second vtable slot and atomically bumps a
// decoded-frame counter.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamDecoder.cpp:
//
//     XCAM::CDecoder::InitializeCodec        @ 0x82988078
//     XCAM::CDecoder::DecodeFrame            @ 0x82988118
//     XCAM::CDecoder::Initialize             @ 0x82988248
//     XCAM::CDecoder::ProcessSequenceHeader  @ 0x82988038
//     XCAM::CDecoder::Reset                  @ 0x82987FE0
//
// There is no reference source and no DWARF for this TU; the member ORDER below
// is reconstructed store-for-store from the X360 asm. The byte offsets quoted are
// the X360 (32-bit-pointer) displacements from the disassembly; on the x64 PC
// target the pointer members widen, so the offsets shift but the order holds.
// `XCAM`/`WMV` are X360 SDK boundaries, so their identifiers are preserved
// verbatim per the naming convention.
//
// LAYOUT (member order store-for-store with the asm; X360 byte offsets quoted):
//   +0x000 muDecodeCount           atomically-incremented decoded-frame counter
//                                   (lwarx/stwcx on this+0 in DecodeFrame; =0 in
//                                   Initialize)
//   +0x004 mpDecoder               WMV decoder session handle (WMVideoDecInit
//                                   writes it via &mpDecoder; the other WMV* calls
//                                   pass the value)
//   +0x008 mDataCallback           {context, get-data handler} descriptor handed
//                                   to WMVideoDecInit (this+8 = context = the data
//                                   source, this+0xC = &WMVDecCBGetData_HANDLER)
//   +0x010 mpDataSource            the COM-style data-source/notifier object (same
//                                   pointer as the callback context); DecodeFrame
//                                   calls its vtable slot 1 after each decode
//   +0x014 mDecoderOutput          embedded CDecoderOutput (0x118 bytes)
//   +0x12C muDimensions            packed frame dimensions: width = high 16 bits,
//                                   height = low 16 bits
//   +0x130 miFramerate             rate-control param A (passed to WMVideoDecInit
//                                   as a float; framerate by analogy to CEncoder)
//   +0x134 miBitrate               rate-control param B (passed as a float)
//   +0x138 mbSequenceHeaderDecoded set true once a sequence header decodes; cleared
//                                   by InitializeCodec / Reset / the re-init path
//   sizeof(CDecoder) == 0x13C
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamConfig.h"          // WMVDecCBGetData_HANDLER
#include "SDKs/XCam/XCamDecoderOutput.h"   // CDecoderOutput (+ D3DDevice, SVideoBuffer)

namespace XCAM
{

// --- Windows Media Video decoder SDK (platform boundary) -------------------
// Opaque decoder-session handle and the WMVideoDec* entry points the decoder
// drives. The exact WMV SDK prototypes are not available; each is declared to
// match its X360 call site so the reconstructed bodies compile store-for-store.
// Supplied by the platform layer; NOT reconstructed here.
struct WMVDEC;

// The (get-data) callback descriptor handed to WMVideoDecInit. Field order pinned
// by Initialize storing the context at this+0x08 and the handler at this+0x0C.
typedef int (*WMVDecGetDataFn)(void* pContext, int a2, int a3, int a4, int a5, int a6);
struct SWMVDecDataCallback
{
    void*           mpContext;   // +0x00  data-source "this" (COM-style interface)
    WMVDecGetDataFn mpfnGetData; // +0x04  &WMVDecCBGetData_HANDLER
};

int WMVideoDecInit(WMVDEC** ppDecoder, SWMVDecDataCallback* pCallback, u32 uFourCC,
                   double dFramerate, double dBitrate, s32 iWidth, s32 iHeight, s32 iMode);
int WMVideoDecDecodeSequenceHeader(WMVDEC** ppDecoder);
int WMVideoDecReset(WMVDEC* pDecoder);
int WMVideoDecClose(WMVDEC* pDecoder);
int WMVideoDecDecodeData(WMVDEC* pDecoder, void* pData, int iMode);
int WMVideoDecSetErrorRecoveryFrameType(WMVDEC* pDecoder, int iFrameType);
int WMVideoDecPlanarYUVGetOutput(WMVDEC* pDecoder, int iFlags,
                                 void* pYData, void* pUData, void* pVData,
                                 u32 uYPitch, u32 uUPitch, u32 uVPitch);

// The codec FourCC handed to WMVideoDecInit (`lis 0x574D ; ori 0x5652`): 'WMVR'.
static const u32 KU_XCAM_DEC_CODEC_WMVR = 0x574D5652u;
// The DecodeData / PlanarYUVGetOutput mode arguments (`li r30,2` / `li r4,1`).
static const int KI_XCAM_DEC_DECODE_MODE = 2;
static const int KI_XCAM_DEC_OUTPUT_MODE = 1;
// InitializeCodec's failure return (`li r3,8`), matching the video-out family.
static const int KI_XCAM_DECODER_INIT_FAILED = 8;

// WMVideoDecDecodeData result codes that steer the post-decode dispatch. The
// numeric values are attested by the DecodeFrame compares; the semantic labels
// are inferred (a full frame is ready at 0; a sequence change wants a reset at 3;
// -100/1/4/11 need no action; any other value is a hard error -> full re-init).
static const int KI_XCAM_DEC_FRAME_READY    = 0;
static const int KI_XCAM_DEC_NEED_MORE_DATA = 1;
static const int KI_XCAM_DEC_RESET_REQUIRED = 3;
static const int KI_XCAM_DEC_ASYNC_PENDING  = -100;

class CDecoder
{
public:
    // @ 0x82988248 -- stash the dimensions + rate-control params + data source,
    // bring up the CDecoderOutput stage and, on success, wire the WMV get-data
    // callback and initialise the codec. Returns 0, the CDecoderOutput::Initialize
    // failure code, or KI_XCAM_DECODER_INIT_FAILED (8).
    int Initialize(u32 uDimensions, s32 iFramerate, s32 iBitrate,
                   void* pDataSource, D3DDevice* pDevice);

    // @ 0x82988078 -- (re)create the WMV decoder session from the stored
    // dimensions/rate params. Returns 0 on success or KI_XCAM_DECODER_INIT_FAILED.
    int InitializeCodec();

    // @ 0x82988118 -- feed the next encoded frame through the codec: set the
    // error-recovery frame type, DecodeData, ping the data source (vtable slot 1),
    // bump the decoded-frame counter, then dispatch on the result -- publishing a
    // ready planar-YUV frame to the output (unless bSkipOutput), resetting, or
    // fully re-initialising on error. Returns the raw DecodeData result.
    int DecodeFrame(int bSkipOutput, int iErrorRecoveryFrameType);

    // @ 0x82988038 -- decode the stream's sequence header; record whether it
    // succeeded in mbSequenceHeaderDecoded and return the raw result.
    int ProcessSequenceHeader();

    // @ 0x82987FE0 -- reset the codec and clear mbSequenceHeaderDecoded; unless
    // bPreserveOutput is set, also reset the CDecoderOutput stage.
    int Reset(int bPreserveOutput);

    // @ external (XCAM__CDecoder__Shutdown) -- tear the decoder session down.
    // Called by CRemoteConsoleList::Shutdown on each embedded decoder; its body
    // is its own TU, declared here (its canonical home) so consumers compile.
    int Shutdown();

private:
    u32                 muDecodeCount;           // +0x000
    WMVDEC*             mpDecoder;               // +0x004
    SWMVDecDataCallback mDataCallback;           // +0x008
    void*               mpDataSource;            // +0x010
    CDecoderOutput      mDecoderOutput;          // +0x014 (0x118 bytes)
    u32                 muDimensions;            // +0x12C
    s32                 miFramerate;             // +0x130
    s32                 miBitrate;               // +0x134
    bool                mbSequenceHeaderDecoded; // +0x138
};

} // namespace XCAM
