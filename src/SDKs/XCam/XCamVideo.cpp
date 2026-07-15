#include "SDKs/XCam/XCamVideo.h"

#include <intrin.h> // _InterlockedDecrement (MSVC atomic intrinsic)

// ===========================================================================
// XCAM::CXCamVideo -- bodies reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamVideo.h for the layout and collaborators.
// ===========================================================================

namespace XCAM
{

// @ 0x829809B8 -- ~CXCamVideo. The X360 asm restamps both vptrs (+0 -> off_8210BC30,
// +4 -> off_8210BC2C, then +4 -> off_8210BC28) around a single interrupt-masked
// lwarx/stwcx. decrement of the global unk_83222CEC and a call to XCamShutdown.
// The vtable restamps + base-dtor chaining are what the compiler synthesises for
// a virtual destructor, so the body here is just the two observable effects:
// the atomic counter drop and the shutdown call.
CXCamVideo::~CXCamVideo()
{
    _InterlockedDecrement(&gXCamActiveInstanceCount); // interrupt-masked --unk_83222CEC
    XCamShutdown();
}

// @ 0x82980A80 -- Release. The X360 body is `addi r3,r3,4 ; b CObjectRefCount::Release`:
// adjust `this` to the CObjectRefCount base subobject (+4) and tail-call its
// Release. The qualified base call reproduces that +4 adjustment exactly.
int CXCamVideo::Release()
{
    return CObjectRefCount::Release();
}

// @ 0x82980BB0 -- ReadFrame. `this` is never referenced (the routine works purely
// off the shared camera-runtime globals). Gate order mirrors the asm:
//   camera-ready -> capture-active -> read-not-pending -> buffer-big-enough ->
//   grab an overlap slot -> XUsbcamReadFrame. *lpStatus receives the code; the
//   overlap slot is returned unless the driver reported ERROR_IO_PENDING (997).
int CXCamVideo::ReadFrame(XCamFrameBuffer* lpFrame, int* lpStatus)
{
    XCamOverlappedInfo* lpOverlap = nullptr;   // r30 / v5
    int iFrame = CheckCameraReadyForUse();      // r31

    if (iFrame == 0)
    {
        if (gXCamCaptureActive == 0)
        {
            iFrame = KI_XCAM_ERROR_NOT_READY; // 21
        }
        else if (gXCamReadFramePending != 0)
        {
            iFrame = KI_XCAM_ERROR_BUSY; // 170 (LABEL_9)
        }
        else if (lpFrame->uSize != 0 &&
                 lpFrame->uSize < ((gXCamFrameFormatWord >> 16) << 1)) // HIWORD x2
        {
            iFrame = KI_XCAM_ERROR_INSUFFICIENT_BUFFER; // 122
        }
        else
        {
            lpOverlap = GetOverlappedInfo();
            if (lpOverlap == nullptr)
            {
                iFrame = KI_XCAM_ERROR_BUSY; // 170 (LABEL_9)
            }
            else
            {
                iFrame = SetupOverlap(lpOverlap, lpStatus);
                if (iFrame == 0)
                {
                    lpOverlap->uMode = 1; // *(v5 + 12) = 1
                    iFrame = XUsbcamReadFrame(
                        gXCamDeviceHandle, lpFrame->lpData, lpFrame->uSize,
                        reinterpret_cast<void*>(&HandleFrameCompletion), lpOverlap);
                }
            }
        }
    }

    *lpStatus = iFrame;
    if (iFrame != KI_XCAM_ERROR_IO_PENDING && lpOverlap != nullptr)
        ReturnOverlappedInfo(lpOverlap);
    return iFrame;
}

} // namespace XCAM
