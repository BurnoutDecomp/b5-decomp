#pragma once

// ===========================================================================
// XCAM::CXCamVideo -- the X360 USB-camera "video source" object of the XCAM
// (X360 camera/capture) SDK in BURNOUT_X360_ARTIST.XEX. It is a COM-style
// reference-counted object: a camera frame-source interface (IXCamVideo, +0x00)
// plus the shared reference-count base XCAM::CObjectRefCount (+0x04, see
// XCamObjectRefCount.h). ReadFrame issues an asynchronous XUsbcamReadFrame from
// the kernel USB-cam driver; the destructor shuts the camera down and drops the
// process-wide active-instance counter.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamVideo.cpp:
//
//     XCAM::CXCamVideo::ReadFrame     @ 0x82980BB0
//     XCAM::CXCamVideo::Release       @ 0x82980A80
//     XCAM::CXCamVideo::~CXCamVideo   @ 0x829809B8
//
// The two compiler-emitted deleting-destructor thunks -- the `vector deleting
// destructor' @ 0x82980CA0 and its `adjustor{4}' @ 0x82980A88 -- are the MSVC
// vtable-slot-0 / this-adjusting thunks and are dropped per policy (the virtual
// destructor below reproduces the slot-0 behaviour; the +4 base adjustment is
// synthesised by the compiler).
//
// There is no reference source and no DWARF for this TU; the shape below is
// reconstructed store-for-store from the X360 asm. `XCAM` is an X360 SDK
// boundary, so its identifiers (CXCamVideo, Release, ReadFrame) are preserved
// verbatim per the naming convention.
//
// LAYOUT (attested by the asm):
//   +0x00 IXCamVideo      primary interface vptr (restamped in ~CXCamVideo to
//                         off_8210BC30)
//   +0x04 CObjectRefCount base subobject: its vptr (restamped to off_8210BC28,
//                         i.e. CObjectRefCount's own table) + muRefCount @ +0x08
//   CXCamVideo adds NO data members of its own. (No pointer-invariant byte-offset
//   fact is worth pinning: every offset here is one pointer wide, so it scales
//   with the LLP64 gate rather than staying at the X360's 4-byte stride.)
//
// IXCamVideo is reconstructed from the two slots CXCamVideo overrides in this TU
// (Release @ the +0 adjustor, ReadFrame). Any further slots the real X360
// interface carries are not attested here and are deliberately NOT invented.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamObjectRefCount.h"
#include "SDKs/XCam/XCamConfig.h" // XCamOverlappedInfo, GetOverlappedInfo/SetupOverlap/
                                  // ReturnOverlappedInfo, gXCamDeviceHandle

namespace XCAM
{

// --- Win32/XDK status codes returned through *lpStatus (values attested by the
// asm: 0x15 / 0xAA / 0x7A / 0x3E5). ------------------------------------------
static const int KI_XCAM_ERROR_NOT_READY            = 21;  // 0x15  camera not started
static const int KI_XCAM_ERROR_INSUFFICIENT_BUFFER  = 122; // 0x7A  frame buffer too small
static const int KI_XCAM_ERROR_BUSY                 = 170; // 0xAA  no overlap slot / read pending
static const int KI_XCAM_ERROR_IO_PENDING           = 997; // 0x3E5 async read queued

// The (byteSize, data) frame-buffer descriptor handed to ReadFrame. 8-byte
// stride is attested by ReadFrame's loads: *a2 (size) -> lwz 0(r29), a2[1]
// (data) -> lwz 4(r29).
struct XCamFrameBuffer
{
    u32   uSize;   // +0x00  frame byte size
    void* lpData;  // +0x04  destination pixel buffer
};

// --- the camera frame-source interface CXCamVideo implements (primary base) --
// COM-style: a virtual destructor + Release + ReadFrame. Reconstructed from the
// slots CXCamVideo overrides; abstract so the two-vptr MI layout matches the asm.
class IXCamVideo
{
public:
    virtual ~IXCamVideo() {}
    virtual int Release() = 0;
    virtual int ReadFrame(XCamFrameBuffer* lpFrame, int* lpStatus) = 0;
};

// --- collaborators reconstructed in sibling XCAM TUs (declared, not homed here)
int CheckCameraReadyForUse(); // XCAM:: readiness gate; 0 == ready

// Async frame-completion callback (address-taken only; passed to the driver as
// an opaque function pointer). Reconstructed in its own TU.
int HandleFrameCompletion(int a1, int a2, int a3);

// --- kernel USB-cam driver + XCAM startup exports (platform boundary) --------
// Signatures widened to the argument registers the X360 call site populates
// (device handle, buffer data, byte size, completion callback, overlap context).
extern "C" int  XUsbcamReadFrame(int iDeviceHandle, void* lpBuffer, u32 uSize,
                                 void* lpCompletion, void* lpOverlap);
extern "C" void XCamShutdown();

// --- shared mutable XCAM camera-runtime state (file-scope globals in the image)
extern int          gXCamCaptureActive;         // dword_8322343C -- capture started flag
extern int          gXCamReadFramePending;      // dword_83223438 -- a read is already in flight
extern u32          gXCamFrameFormatWord;       // dword_82F3E234 -- high 16 bits x2 = min frame bytes
extern volatile long gXCamActiveInstanceCount;  // unk_83222CEC   -- process-wide live-object counter

class CXCamVideo : public IXCamVideo, public CObjectRefCount
{
public:
    // @ 0x829809B8 -- drop the process-wide active-instance counter (atomic) and
    // shut the camera down. The compiler emits the two vtable restamps (+0 and +4)
    // and the base-dtor chaining around this body.
    ~CXCamVideo() override;

    // @ 0x82980A80 -- forward the interface Release to the CObjectRefCount base
    // subobject (the X360 body is the +4 this-adjusting thunk around
    // CObjectRefCount::Release).
    int Release() override;

    // @ 0x82980BB0 -- issue an asynchronous camera frame read. Gates on camera
    // readiness / capture state / minimum buffer size, grabs an overlap slot and
    // calls XUsbcamReadFrame; writes the resulting status to *lpStatus and returns
    // it (returning the overlap slot unless the read went pending).
    int ReadFrame(XCamFrameBuffer* lpFrame, int* lpStatus) override;
};

} // namespace XCAM
