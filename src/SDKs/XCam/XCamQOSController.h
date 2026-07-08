#pragma once

// ===========================================================================
// XCAM::CQOSController -- the quality-of-service / rate-control governor of the
// X360 XCAM (Xbox Live Vision camera) streaming SDK in BURNOUT_X360_ARTIST.XEX.
// It sits between the receiver-side QOS feedback (loss / recovery / throttle
// flags carried in each packet header) and the encoder-side CEncoder /
// CPacketizer: it measures the real outbound bandwidth, and on a set of timers
// nudges the target/actual bit rate up or down, forces key / recovery frames,
// and re-derives the encode frame rate from the current bit rate.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamQOSController.cpp. There is no reference source and no DWARF for this TU;
// the SHAPE below is reconstructed store-for-store from the X360 asm (member
// offsets are the load/store displacements off `this`). `XCAM` is an X360 SDK
// boundary, so its identifiers (CQOSController, ...) are preserved verbatim per
// the naming convention.
//
// GROUNDED here (7): CalculateRealBandwidthUsage, DoWork, ForceKeyFrame,
//   ProcessQOSFeedback, SetTargetBitrate, SetTargetFramerate, SumSendSize.
//
// BLOCKED (7, declared only -- bodies reach through the still-un-homed
//   XCAM::CStreamEngine to its embedded CEncoder (+0x44) / CPacketizer (+0x1BC)
//   / CRemoteConsoleList (+0x268) and engine fields, and/or need un-homed
//   collaborators (CRemoteConsoleList::FindRemoteConsole,
//   CStreamEngine::SetNewEncodeFrequency) or un-recovered rodata frame-rate
//   ladder tables): DecreaseBitrate, ForceRecoveryFrame, GetSlowestConnection,
//   IncreaseBitrate, ResetActualFramerate, SetActualBitrate, ThrottleBitrateDown.
//   These are declared so the grounded callers (DoWork, the setters) compile;
//   their definitions land when CStreamEngine is homed.
// ===========================================================================

#include "types.hpp"

namespace XCAM
{

// The owning stream engine. CQOSController only holds it by pointer (the blocked
// bodies reach through it to the embedded encoder / packetizer / remote-console
// list). Forward-declared per the pointer-only un-homed-collaborator exception;
// homed when XCAM::CStreamEngine is reconstructed.
class CStreamEngine;

// --- Win32 / Xbox platform boundary ----------------------------------------
// Millisecond tick used to drive the QOS timers (matches the sibling XCAM TUs).
extern "C" u32 GetTickCount();

// The X360 asm folds the byte accumulator into an interrupt-disabled
// lwarx/stwcx atomic (mfmsr / mtmsree wrap); that is the XDK
// InterlockedExchange / InterlockedExchangeAdd on a signed 32-bit counter.
// Declared as a platform boundary; the console/PC bodies are supplied by the
// runtime.
extern "C" s32 InterlockedExchange(volatile s32* pTarget, s32 iValue);
extern "C" s32 InterlockedExchangeAdd(volatile s32* pAddend, s32 iValue);

// QOS bandwidth window (`flt_82019638`): only re-measure once >5 s has elapsed.
static const f32 KF_QOS_BANDWIDTH_WINDOW_MS = 5000.0f;
// bytes-per-ms -> kilobits-per-second scale (`flt_82004C88`).
static const f32 KF_QOS_BITS_PER_BYTE = 8.0f;
// DoWork timer thresholds (ms).
static const u32 KU_QOS_KEYFRAME_INTERVAL_MS = 1000;  // 0x3E8
static const u32 KU_QOS_RECOVERY_INTERVAL_MS = 1000;  // 0x3E8
static const u32 KU_QOS_DECREASE_INTERVAL_MS = 200;   // 0xC8
static const u32 KU_QOS_INCREASE_INTERVAL_MS = 4000;  // 0xFA0
// Result returned by GetSlowestConnection when no live console is found (`0xE8`).
static const int KI_QOS_NO_CONNECTION = 232;
// The QOS feedback flag bits read from packet-header byte +1 (a2[1]).
static const u8 KU_QOS_FLAG_KEYFRAME_A = 0x08;
static const u8 KU_QOS_FLAG_KEYFRAME_B = 0x10;
static const u8 KU_QOS_FLAG_THROTTLE_A = 0x40;
static const u8 KU_QOS_FLAG_THROTTLE_B = 0x20;
// Byte count of the QOS report snapshot copied around (memcpy size `0x24`).
static const u32 KU_QOS_REPORT_SIZE = 36;

class CQOSController
{
public:
    // @ 0x829836A0 -- atomically drain the outbound byte counter roughly every
    // 5 s into mfBandwidth (kbps) and re-derive mfBandwidthRatio against the
    // current bit rate. Returns the GetTickCount() value it sampled.
    u32 CalculateRealBandwidthUsage();

    // @ 0x82983C78 -- the periodic rate-control tick: measure bandwidth, then on
    // the per-action timers force a key / recovery frame and step the bit rate
    // down (DecreaseBitrate) or up (IncreaseBitrate), committing any change via
    // SetActualBitrate. Called by CStreamEngine::EncoderWorkerThreadProc.
    u32 DoWork();

    // @ 0x82983628 -- request that a key frame be emitted and reset the
    // key-frame timer.
    u32 ForceKeyFrame();

    // @ 0x829835A8 -- fold the QOS feedback flags from a received packet header
    // into the pending-action bits; when a throttle bit is set, snapshot the
    // attached QOS report. pHeader is the packet header (flag byte at +1),
    // pReport the 36-byte report payload.
    void ProcessQOSFeedback(const u8* pHeader, const void* pReport);

    // @ 0x82983DC8 -- set the target (ceiling) bit rate; if the current bit rate
    // now exceeds it, drop the actual bit rate down to the new ceiling.
    void SetTargetBitrate(u32 uBitrate);

    // @ 0x82983BF8 -- set the target frame rate and re-derive the actual encode
    // frame rate from the current bit rate.
    void SetTargetFramerate(int iFramerate);

    // @ 0x82983668 -- track the peak send size and, when a new peak is seen,
    // atomically add the given byte count into the bandwidth accumulator.
    void SumSendSize(u32 uSendSize, s32 iBytes);

    // ---- BLOCKED bodies (declared only; see file banner) ----
    // @ 0x829839B8 -- step the send bit rate down by ~3% (floored by the engine
    // minimum) and push it to the packetizer.
    u32 DecreaseBitrate();
    // @ 0x82983810 -- ask the encoder for an error-recovery frame; reset the
    // recovery timer.
    u32 ForceRecoveryFrame();
    // @ 0x82983940 -- look up the slowest live remote console and copy its 36-byte
    // stats out; returns 0 or KI_QOS_NO_CONNECTION.
    int GetSlowestConnection(void* pOutStats);
    // @ 0x82983AA8 -- step the send bit rate up (bandwidth-headroom dependent),
    // clamped to the target ceiling / engine minimum, and push it to the
    // packetizer.
    u32 IncreaseBitrate();
    // @ 0x82983858 -- pick the actual encode frame rate from the bit-rate ladder
    // for the current resolution and re-arm the encode frequency.
    int ResetActualFramerate();
    // @ 0x82983C10 -- commit a new actual bit rate: push it to the encoder and
    // re-derive the frame rate.
    int SetActualBitrate(u32 uBitrate);
    // @ 0x82983D68 -- clamp the actual bit rate down to a bandwidth-derived cap.
    void ThrottleBitrateDown();

    // --- layout (member ORDER is store-for-store with the X360 asm; the X360
    //     byte offsets are noted, but mpEngine is a pointer that widens 4->8 on
    //     the PC x64 target, so the console displacements past it do not carry
    //     over. Parity is by named member, not byte offset, per the widening
    //     convention.) ---
    CStreamEngine* mpEngine;             // X360 +0x00 owning stream engine
    u32            miTargetBitrate;      // +0x04 target / ceiling bit rate
    u32            miCurrentBitrate;     // +0x08 current (actual) bit rate
    s32            miTargetFramerate;    // +0x0C target frame rate
    s32            miActualFramerate;    // +0x10 actual (derived) frame rate
    u32            miSendBitrate;        // +0x14 packetizer send-rate cap
    u32            miConnectionBitrate;  // +0x18 measured connection bit rate
    u8             mbKeyFrameRequested;  // +0x1C key-frame feedback pending
    u8             mbRecoveryRequested;  // +0x1D recovery-frame pending
    u8             mbDecreaseRequested;  // +0x1E throttle-down pending
    u8             mPad_1F;              // +0x1F (align next word)
    u32            muKeyFrameTick;       // +0x20 last key-frame action tick
    u32            muRecoveryTick;       // +0x24 last recovery action tick
    u32            muBitrateTick;        // +0x28 last bit-rate change tick
    u32            muBandwidthTick;      // +0x2C last bandwidth measurement tick
    s32            mbKeyFramePending;    // +0x30 key-frame signal to the encoder
    s32            miByteCounter;        // +0x34 atomic outbound byte accumulator
    u32            miPeakSendSize;       // +0x38 peak send size seen
    f32            mfBandwidth;          // +0x3C measured bandwidth (kbps)
    f32            mfBandwidthRatio;     // +0x40 bandwidth / current bit rate
    u8             mQOSReport[36];       // +0x44 last QOS report snapshot
};

} // namespace XCAM
