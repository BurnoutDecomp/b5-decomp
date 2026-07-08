#pragma once

// ===========================================================================
// XCAM::CEncoder -- the video encoder in the XCAM (X360 camera/capture) SDK of
// BURNOUT_X360_ARTIST.XEX. It wraps the Xbox "EMB" media encoder session (a VC-1
// / 'WVC1' stream): it owns the encoder session handle, a YUY2 pre-processing
// video output (CYUY2VideoOutput) that ingests raw camera frames, a
// BITMAPINFOHEADER describing the encoded frame, and a double-buffered pair of
// overlapped-completion helpers (COverlappedIO) guarded by a critical section so
// a producer (ProcessCameraFrameData) and the encoder worker thread can hand
// frames back and forth. Rate control (target frame rate / bit rate) is applied
// lazily on the next EncodeFrame when it drifts from the last applied value.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamEncoder.cpp:
//
//     XCAM::CEncoder::Initialize               @ 0x82983E78
//     XCAM::CEncoder::RecoveryFrameRequired    @ 0x82983DF0
//     XCAM::CEncoder::SetBitrate               @ 0x82983E68
//     XCAM::CEncoder::SetFramerate             @ 0x82983E70
//     XCAM::CEncoder::Shutdown                 @ 0x82984040
//     XCAM::CEncoder::SubmitFrameToEncodeQueue @ 0x829840C0
//     XCAM::CEncoder::EncodeFrame              @ 0x82984188
//     XCAM::CEncoder::SkipFrame                @ 0x829843D0
//
// There is no reference source and no DWARF for this TU; the layout below is
// reconstructed store-for-store from the X360 asm (offsets pinned by the
// _AssertLayout() static_asserts). `XCAM`/`EMB` are X360 SDK boundaries, so
// their identifiers are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamYUY2VideoOutput.h"
#include "SDKs/XCam/XCamOverlappedIO.h"

namespace XCAM
{

// --- Xbox-360 kernel critical section (platform boundary) ------------------
// The RTL_CRITICAL_SECTION guarding the double-buffered request queue. 28 bytes
// is attested by the encoder members straddling it (muCurrentIndex @0x140,
// mCriticalSection @0x144, miFramerate @0x160 -> 0x160-0x144 == 0x1C). Only the
// Rtl* entry points below touch it; the fields are opaque. Supplied by the
// platform layer; NOT reconstructed here.
struct RTL_CRITICAL_SECTION
{
    u8 mReserved[28];
};
u32  RtlInitializeCriticalSectionAndSpinCount(RTL_CRITICAL_SECTION* pCriticalSection, u32 uSpinCount);
void RtlEnterCriticalSection(RTL_CRITICAL_SECTION* pCriticalSection);
void RtlLeaveCriticalSection(RTL_CRITICAL_SECTION* pCriticalSection);

// --- Win32/Xbox timing (platform boundary) ---------------------------------
u32 GetTickCount();

// --- BITMAPINFOHEADER (Win32 video format descriptor) ----------------------
// The 40-byte format header the encoder builds for the YUY2 source frames and
// hands to EMBEncEncodeAndLock. Field placement is store-for-store with the
// +0/+4/+8/+0xC/+0xE/+0x10/+0x14 accesses in CEncoder::Initialize.
struct SBitmapInfoHeader
{
    u32 muSize;          // +0x00 biSize (== 40)
    s32 miWidth;         // +0x04 biWidth
    s32 miHeight;        // +0x08 biHeight
    u16 muPlanes;        // +0x0C biPlanes
    u16 muBitCount;      // +0x0E biBitCount (== 16 for YUY2)
    u32 muCompression;   // +0x10 biCompression (== 'YUY2')
    u32 muSizeImage;     // +0x14 biSizeImage (width * height * 2)
    s32 miXPelsPerMeter; // +0x18
    s32 miYPelsPerMeter; // +0x1C
    u32 muClrUsed;       // +0x20
    u32 muClrImportant;  // +0x24
};
// All fixed-width fields, so this 40-byte BITMAPINFOHEADER layout is identical on
// the X360 and the PC x64 target.
static_assert(sizeof(SBitmapInfoHeader) == 40, "SBitmapInfoHeader must be 40 bytes");

// --- Xbox "EMB" media encoder SDK (platform boundary) ----------------------
// Opaque encoder session handle and the EMB* entry points the encoder drives.
// The exact EMB SDK prototypes are not available; each is declared to match its
// X360 call site so the reconstructed bodies compile store-for-store. Supplied
// by the platform layer; NOT reconstructed here.
struct EMBEncSession;
EMBEncSession* EMBEncSessionCreate(u32 uFourCC);
void           EMBEncSessionDelete(EMBEncSession* pSession);
void           EMBEncSessionSetRTC(EMBEncSession* pSession);
int            EMBEncInit(EMBEncSession* pSession, int iProfile, s32 iWidth, s32 iHeight,
                          int iMaxBitrate, int iLatencyMs, int iGopLength, int iVbvSize,
                          int* pInitParam, double dFramerate, double dBitrate);
int            EMBEncGetExtendedFormat(EMBEncSession* pSession, void* pBuffer, u32* puSize);
int            EMBEncEncodeAndLock(EMBEncSession* pSession, const SBitmapInfoHeader* pFormat,
                                   u32 uFrameData, int iParam, u32* pOutStatus,
                                   s32 iWidth, s32 iHeight, u8 bForceKeyFrame,
                                   u8* pbFrameDropped);
void           EMBEncGetPredType(EMBEncSession* pSession, s64* pPredType);
int            EMBQueryErrorRecoveryFrameType(EMBEncSession* pSession, void* pOutFrameType);
int            EMBEncRTCSetNetworkMetrics(EMBEncSession* pSession, int iFlags, double dMetric);
void           EMBEncRTCChangeBitRate(EMBEncSession* pSession, double dBitRate);
void           EMBEncRTCChangeFrameRate(EMBEncSession* pSession, double dFrameRate);

// The FourCC handed to EMBEncSessionCreate (`lis 0x5756 ; ori 0x4331`): 'WVC1',
// the Windows Media Video / VC-1 codec.
static const u32 KU_XCAM_ENC_CODEC_WVC1 = 0x57564331u;
// The YUY2 packed-4:2:2 compression FourCC baked into the format header
// (`lis 0x3259 ; ori 0x5559` -> 0x32595559).
static const u32 KU_XCAM_FOURCC_YUY2 = 0x32595559u;
// biSize / biBitCount constants stored into the format header.
static const u32 KU_XCAM_BMIH_SIZE     = 40u;
static const u16 KU_XCAM_YUY2_BITCOUNT = 16u;

// Failure code returned when the encoder session cannot be created / initialised
// (`li r3, 8`).
static const int KI_XCAM_ENCODER_INIT_FAILED = 8;
// Result returned when EncodeFrame is called with no request queued (`li r3,0xE8`).
static const int KI_XCAM_ENCODER_NO_REQUEST = 232;
// Result returned when the frame was dropped / the encode+lock failed
// (`li r3, 0x65B`).
static const int KI_XCAM_ENCODER_FRAME_DROPPED = 1627;
// (status, extended-error) SignalComplete pushes on teardown (`0x65B`, `0x45B`).
static const u32 KU_XCAM_SIGNAL_STATUS_TEARDOWN = 1627u; // 0x65B
static const u32 KU_XCAM_SIGNAL_ERROR_TEARDOWN  = 1115u; // 0x45B
// EMBEncInit's fixed RTC configuration constants baked into the X360 image.
static const int KI_XCAM_ENC_MAX_BITRATE = 0x7FFFFFFF;
static const int KI_XCAM_ENC_LATENCY_MS  = 500;   // 0x1F4
static const int KI_XCAM_ENC_GOP_LENGTH  = 90;    // 0x5A
static const int KI_XCAM_ENC_VBV_SIZE    = 10000; // 0x2710
static const int KI_XCAM_ENC_PROFILE     = 5;
// The extended-format probe buffer size seeded before EMBEncGetExtendedFormat.
static const u32 KU_XCAM_EXT_FORMAT_SIZE = 64;
// Critical-section spin count (`li r4, 0x400`).
static const u32 KU_XCAM_CS_SPIN_COUNT = 1024;

class CEncoder
{
public:
    // @ 0x82983E78 -- create + initialise the EMB encoder session for a YUY2
    // source at the given dimensions and rate-control targets, build the
    // BITMAPINFOHEADER, arm the critical section and (when enabled) the YUY2
    // video output. Returns 0 on success, or KI_XCAM_ENCODER_INIT_FAILED (8).
    int Initialize(u32 uDimensions, u32 uFramerate, u32 uBitrate, int iInitParam,
                   int bEnableVideoOutput, D3DDevice* pDevice);

    // @ 0x82984040 -- delete the encoder session, shut the YUY2 output down and
    // complete any still-pending overlapped request in either queue slot.
    void Shutdown();

    // @ 0x829840C0 -- ingest a raw camera frame (copied through the YUY2 output
    // when enabled), stash the frame pointer in the overlapped and enqueue it in
    // the free double-buffer slot under the critical section. Returns the
    // COverlappedIO::Setup result.
    u32 SubmitFrameToEncodeQueue(u32 uFrameData, XOVERLAPPED* pOverlapped);

    // @ 0x82984188 -- flip the double-buffer, then (if a request is queued) apply
    // any pending rate-control changes and encode+lock the queued frame,
    // completing the overlapped. Returns 0 on success, KI_XCAM_ENCODER_NO_REQUEST
    // (232) when nothing is queued, or KI_XCAM_ENCODER_FRAME_DROPPED (1627) when
    // the frame is dropped.
    int EncodeFrame(int iFrameKind, int iParam, u32* pOutStatus,
                    u32* pbIsIntraFrame, void* pRecoveryFrameType);

    // @ 0x829843D0 -- flip the double-buffer and, if the now-current slot still
    // holds a request, complete it (no encode). Returns the affected queue slot.
    COverlappedIO* SkipFrame();

    // @ 0x82983E68 -- set the target bit rate (applied lazily by EncodeFrame).
    void SetBitrate(s32 iBitrate) { miBitrate = iBitrate; }

    // @ 0x82983E70 -- set the target frame rate (applied lazily by EncodeFrame).
    void SetFramerate(s32 iFramerate) { miFramerate = iFramerate; }

    // @ 0x82983DF0 -- request that the next encoded frame be an error-recovery
    // (network-reset) frame.
    void RecoveryFrameRequired() { mbRecoveryFrameRequired = 1; }

private:
    s32                  muFrameCounter;          // +0x000 encoded-frame counter
    EMBEncSession*       mpEncSession;            // +0x004 EMB encoder session
    CYUY2VideoOutput     mVideoOutput;            // +0x008 YUY2 pre-processor (0xB0)
    s32                  mbHasVideoOutput;        // +0x0B8 video output enabled
    SBitmapInfoHeader    mFormat;                 // +0x0BC encoded-frame format
    u8                   mExtendedFormat[64];     // +0x0E4 EMB extended format blob
    u32                  muExtendedFormatSize;    // +0x124 extended-format size (64)
    COverlappedIO        mOverlappedIO[2];        // +0x128 double-buffered requests
    u8                   muCurrentIndex;          // +0x140 active buffer index (0/1)
    u8                   mPad_141[3];             // +0x141 (align mCriticalSection)
    RTL_CRITICAL_SECTION mCriticalSection;        // +0x144 request-queue lock
    s32                  miFramerate;             // +0x160 target frame rate
    s32                  miBitrate;               // +0x164 target bit rate
    s32                  miAppliedFramerate;      // +0x168 last applied frame rate
    s32                  miAppliedBitrate;        // +0x16C last applied bit rate
    s32                  mbRecoveryFrameRequired; // +0x170 force recovery frame
    u32                  muLastEncodeTick;        // +0x174 last EncodeFrame tick
};

} // namespace XCAM
