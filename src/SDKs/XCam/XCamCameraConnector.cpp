#include "SDKs/XCam/XCamCameraConnector.h"

#include <intrin.h> // _InterlockedIncrement -- portable stand-in for the X360
                    // interrupt-masked (mfmsr/mtmsree) lwarx/stwcx reservation
                    // increment on this+0.

// ===========================================================================
// XCAM::CCameraConnector -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamCameraConnector.h for the member layout, the
// platform prototypes and the per-method X360 addresses. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// The camera DEVICE (mpCameraDevice) and frame SINK (mpFrameSink) are foreign
// COM-style interfaces that are not homed in this image. As in XCAM::CDecoder,
// each call is a raw dispatch through the one vtable slot the asm touches
// (`lwz r11,0(obj); lwz r11,<slot*4>(r11); bctrl`); no collaborator type shape
// is fabricated.
// ===========================================================================

namespace XCAM
{

// @ 0x82986128
u32 CCameraConnector::DoWork()
{
    mStatus = XCamGetStatus();
    if (mStatus == KU_XCAM_STATUS_READY)
    {
        if (mbConnectInFlight)
        {
            // A Connect() is outstanding: once it leaves the pending state, latch
            // whether it failed (nonzero code) back into the reconnect request.
            if (miConnectResult != static_cast<s32>(KU_XCAM_IO_PENDING))
            {
                mbConnectInFlight = 0;
                // X360 idiom `(_cntlzw(miConnectResult) & 0x20) == 0`: true iff the
                // connect returned a nonzero code -> ask for a retry.
                mbConnectRequested = (miConnectResult != 0) ? 1 : 0;
            }
        }
        else if (mbConnectRequested)
        {
            // Kick off a fresh connect through the device's vtable slot 2.
            mbConnectRequested = 0;
            mbConnectInFlight  = 1;
            void** ppDeviceVtable = *static_cast<void***>(mpCameraDevice);
            const s32 iResult = reinterpret_cast<s32 (*)(void*, u32, u32, s32*)>(
                ppDeviceVtable[2])(mpCameraDevice, muConnectArg0, muConnectArg1,
                                   &miConnectResult);
            if (iResult)
            {
                if (iResult != static_cast<s32>(KU_XCAM_IO_PENDING))
                    mbConnectRequested = 1;
            }
            else
            {
                miConnectResult = 0;
            }
        }
    }

    u32 uResult = XCamGetStatus();
    mStatus = uResult;

    // Pump frames only once the connect handshake has fully settled.
    if (!mbConnectRequested && !mbConnectInFlight)
    {
        for (u32 i = 0; i < KU_XCAM_FRAME_SLOT_COUNT; ++i)
        {
            XOVERLAPPED* pOverlapped = &maOverlapped[i];
            if (pOverlapped->InternalLow == KU_XCAM_IO_PENDING)
                continue;

            if (maSlotState[i] != 1 ||
                (uResult = XGetOverlappedResult(pOverlapped, nullptr, 0)) != 0)
            {
                // Slot idle, or its read finished with a non-success code: (re-)issue
                // a read through the device's vtable slot 3.
                if (mStatus == KU_XCAM_STATUS_READY)
                {
                    void** ppDeviceVtable = *static_cast<void***>(mpCameraDevice);
                    uResult = reinterpret_cast<u32 (*)(void*, SFrameSlot*, XOVERLAPPED*)>(
                        ppDeviceVtable[3])(mpCameraDevice, &maFrameSlots[i], pOverlapped);
                    maSlotState[i] = (uResult == KU_XCAM_IO_PENDING) ? 1u : 0u;
                }
            }
            else
            {
                // The read completed successfully: deliver the frame to the sink
                // through its vtable slot 0.
                void** ppSinkVtable = *static_cast<void***>(mpFrameSink);
                uResult = reinterpret_cast<u32 (*)(void*, void*, XOVERLAPPED*)>(
                    ppSinkVtable[0])(mpFrameSink, maFrameSlots[i].mpData, pOverlapped);
                // X360 idiom `(_cntlzw(uResult) >> 4) & 2` == 2 iff uResult == 0.
                maSlotState[i] = (uResult == 0) ? 2u : 0u;
                if (uResult == 0)
                {
                    _InterlockedIncrement(
                        reinterpret_cast<volatile long*>(&muCompletedFrameCount));
                }
            }
        }
    }

    return uResult;
}

// @ 0x82986090
s32 CCameraConnector::Shutdown()
{
    // Disconnect the device (vtable slot 1) if one is attached.
    if (mpCameraDevice)
    {
        void** ppDeviceVtable = *static_cast<void***>(mpCameraDevice);
        reinterpret_cast<void (*)(void*)>(ppDeviceVtable[1])(mpCameraDevice);
    }

    XCamShutdown();

    // Spin until the outstanding connect leaves the pending state.
    while (miConnectResult == static_cast<s32>(KU_XCAM_IO_PENDING))
        Sleep(1);

    // Spin until every overlapped frame read has drained.
    for (u32 i = 0; i < KU_XCAM_FRAME_SLOT_COUNT; ++i)
    {
        while (maOverlapped[i].InternalLow == KU_XCAM_IO_PENDING)
            Sleep(1);
    }

    // Free the shared frame allocation, whose base is stashed in slot 0. When no
    // buffer was ever allocated the X360 tail returns that null word (0).
    void* pBuffer = maFrameSlots[0].mpData;
    s32 result = 0;
    if (pBuffer)
        result = XMemFree(pBuffer, KU_XCAM_XMEM_FREE_ATTRIBUTES);
    return result;
}

} // namespace XCAM
