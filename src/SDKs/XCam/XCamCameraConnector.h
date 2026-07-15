#pragma once

// ===========================================================================
// XCAM::CCameraConnector -- the per-frame camera-pump worker of the XCAM (X360
// camera / remote-console capture) SDK in BURNOUT_X360_ARTIST.XEX. It drives the
// asynchronous acquisition pipeline for one video source:
//
//   * it (re)connects to a foreign camera capture DEVICE (mpCameraDevice),
//   * keeps a ring of 5 overlapped-IO frame requests in flight against it,
//   * and, as each request completes, hands the finished frame to a foreign
//     frame SINK (mpFrameSink), bumping an interrupt-safe completed-frame count.
//
// The device and sink are foreign COM-style interfaces (same pattern as the
// data-source object in XCAM::CDecoder): they are NOT homed in this image, so
// -- exactly as CDecoder does for its data source -- only the vtable SLOTS this
// worker calls are expressed, by raw slot dispatch through a void*. No collaborator
// type shape is fabricated. DoWork is called by CStreamEngine::EncoderWorkerThreadProc
// and Shutdown by CStreamEngine::~CStreamEngine (both still un-homed).
//
// There is no reference source and no DWARF for this TU; the layout below is
// reconstructed store-for-store from the X360 asm (per-member X360 byte offsets
// are in the comments). `XCAM` is an X360 SDK boundary, so its identifiers are
// preserved verbatim per the naming convention.
//
// CCameraConnector instance LAYOUT (store-for-store with the asm):
//   +0x000 muCompletedFrameCount  interrupt-safe delivered-frame counter (this+0)
//   +0x004 mpFrameSink            foreign frame-sink interface (DeliverFrame slot 0)
//   +0x008 (unread by this TU)
//   +0x00C mpCameraDevice         foreign capture-device interface (slots 1/2/3)
//   +0x010 maFrameSlots[5]        per-request {field0, frame-data} descriptors
//   +0x038 maOverlapped[5]        the 5 in-flight XOVERLAPPED requests (28B each)
//   +0x0C4 maSlotState[5]         per-slot state word (0/1 pending, 2 delivered)
//   +0x0D8 mStatus                last XCamGetStatus() reading
//   +0x0DC muConnectArg0          first  Connect() argument
//   +0x0E0 muConnectArg1          second Connect() argument
//   +0x0E4 mbConnectRequested     a (re)connect is wanted
//   +0x0E5 mbConnectInFlight      a Connect() is outstanding
//   +0x0E8 miConnectResult        Connect() completion code (997 == pending)
// ===========================================================================

#include <cstddef> // offsetof

#include "types.hpp"
#include "SDKs/XCam/XCamOverlappedIO.h" // XCAM::XOVERLAPPED, KU_XCAM_IO_PENDING

namespace XCAM
{

// --- Win32 / Xbox-360 XDK entry points (platform boundary) -----------------
// Declared with the argument/return shapes the X360 call sites use so the
// reconstructed bodies compile store-for-store. Supplied by the platform layer;
// NOT reconstructed here.
extern "C" u32  XCamGetStatus();                                        // camera readiness code
extern "C" void XCamShutdown();                                         // tear the camera session down
extern "C" void Sleep(u32 dwMilliseconds);                             // Win32 Sleep
extern "C" u32  XGetOverlappedResult(void* lpOverlapped, u32* lpdwResult, u32 bWait);
// The real XDK XMemFree is void; the X360 tail of Shutdown returns the r3 the
// XMemFree call leaves behind, so it is modelled here as returning that word.
extern "C" s32  XMemFree(void* pAddress, u32 uAttributes);

// XCamGetStatus() code meaning "camera up and producing frames" (`cmplwi r3,2`).
static const u32 KU_XCAM_STATUS_READY = 2u;
// Number of overlapped frame requests kept in flight (`li r27,5`).
static const u32 KU_XCAM_FRAME_SLOT_COUNT = 5u;
// Attribute word baked into Shutdown's XMemFree (`lis r4,0x6090 ; ori r4,0x2068`).
static const u32 KU_XCAM_XMEM_FREE_ATTRIBUTES = 0x60902068u;

// One in-flight frame request descriptor (connector +0x10, stride 8 on X360).
// Only mpData is read by this TU (handed to the sink, and freed in Shutdown as
// the base of the shared frame allocation via maFrameSlots[0].mpData).
struct SFrameSlot
{
    u32   muField0; // +0x00 request context word (unread by this TU)
    void* mpData;   // +0x04 frame-data pointer
};

class CCameraConnector
{
public:
    // @ 0x82986128 -- one worker step: advance the (re)connect handshake while
    // the camera is ready, then, once connected and settled, service the 5
    // overlapped frame requests -- re-issuing reads and delivering completed
    // frames to the sink. Returns the last completion/status word (r3).
    u32 DoWork();

    // @ 0x82986090 -- tear the worker down: disconnect the device, shut the
    // camera session, spin until the connect request and all 5 overlapped reads
    // have drained, then free the frame allocation.
    s32 Shutdown();

private:
    // Never emitted; pins the one pointer-invariant offset. (All later offsets
    // depend on pointer width, so they are documented in comments, not asserted --
    // matching the sibling XCAM classes.)
    static void _AssertLayout()
    {
        static_assert(offsetof(CCameraConnector, muCompletedFrameCount) == 0,
                      "muCompletedFrameCount must be the interlocked word at this+0");
        static_assert(KU_XCAM_FRAME_SLOT_COUNT == 5u, "frame ring is 5 requests");
    }

    u32        muCompletedFrameCount;              // +0x000
    void*      mpFrameSink;                        // +0x004 foreign sink interface
    void*      mPad_008;                           // +0x008 (unread by this TU)
    void*      mpCameraDevice;                     // +0x00C foreign device interface
    SFrameSlot maFrameSlots[KU_XCAM_FRAME_SLOT_COUNT];  // +0x010
    XOVERLAPPED maOverlapped[KU_XCAM_FRAME_SLOT_COUNT];  // +0x038
    u32        maSlotState[KU_XCAM_FRAME_SLOT_COUNT];    // +0x0C4
    u32        mStatus;                            // +0x0D8
    u32        muConnectArg0;                      // +0x0DC
    u32        muConnectArg1;                      // +0x0E0
    u8         mbConnectRequested;                 // +0x0E4
    u8         mbConnectInFlight;                  // +0x0E5
    s32        miConnectResult;                    // +0x0E8
};

} // namespace XCAM
