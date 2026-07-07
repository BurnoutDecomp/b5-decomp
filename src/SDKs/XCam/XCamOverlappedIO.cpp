#include "SDKs/XCam/XCamOverlappedIO.h"

// ===========================================================================
// XCAM::COverlappedIO -- reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamOverlappedIO.h for the member layout, the
// XOVERLAPPED shape and the per-method X360 addresses. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

// The PPC full memory barrier (`sync`) the X360 image emits between publishing
// the overlapped result fields and its status word. This is the authentic XDK
// intrinsic name; supplied by the platform/compiler layer.
extern "C" void __sync();

namespace XCAM
{

// @ 0x829883B8
u32 COverlappedIO::Setup(XOVERLAPPED* pOverlapped)
{
    mpApc         = nullptr;
    mpEventObject = nullptr;

    // Stamp the overlapped pending and clear its result fields.
    pOverlapped->InternalLow     = KU_XCAM_IO_PENDING;
    pOverlapped->dwExtendedError = 0;
    pOverlapped->InternalHigh    = 0;

    if (pOverlapped->pCompletionRoutine)
    {
        // Completion routine present -> drive completion through a kernel APC.
        mpApc = static_cast<KAPC*>(ExAllocatePoolWithTag(KU_XCAM_APC_SIZE, KU_XCAM_APC_POOL_TAG));
        if (!mpApc)
            return 8; // ERROR_NOT_ENOUGH_MEMORY

        ObReferenceObject(KeGetCurrentThread());
        KeInitializeApc(mpApc, KeGetCurrentThread(),
                        &COverlappedIO::ApcKernelRoutine,
                        &COverlappedIO::ApcRundownRoutine,
                        &COverlappedIO::ApcNormalRoutine,
                        1, pOverlapped);
    }
    else
    {
        HANDLE hEvent = pOverlapped->hEvent;
        if (hEvent)
        {
            // Event handle present -> reference the KEVENT for later signalling.
            if (ObReferenceObjectByHandle(hEvent, ExEventObjectType, &mpEventObject) < 0)
                return 6; // ERROR_INVALID_HANDLE

            // Reset the referenced KEVENT's dispatcher-header SignalState (+0x04)
            // so it starts non-signalled. Raw access to a platform kernel object.
            *reinterpret_cast<s32*>(reinterpret_cast<u8*>(mpEventObject) + 4) = 0;
        }
    }

    mpOverlapped = pOverlapped;
    return 0;
}

// @ 0x82988498
void COverlappedIO::SignalComplete(u32 dwStatus, u32 dwExtendedError, u32 dwInternalHigh)
{
    // Publish the result fields, then the status word after a release barrier so
    // a consumer that observes the status sees the completed high/error values.
    mpOverlapped->InternalHigh    = dwInternalHigh;
    mpOverlapped->dwExtendedError = dwExtendedError;
    __sync();
    mpOverlapped->InternalLow     = dwStatus;

    if (mpApc)
    {
        // Queue the APC; if it could not be inserted, run its self-free path here.
        if (!KeInsertQueueApc(mpApc, nullptr, nullptr, 0))
        {
            KAPC* pApc = mpApc;
            ObDereferenceObject(pApc->Thread);
            ExFreePool(pApc);
        }
    }
    else if (mpEventObject)
    {
        KeSetEvent(mpEventObject, 0, 0);
        ObDereferenceObject(mpEventObject);
    }

    mpOverlapped = nullptr;
}

// @ 0x829882D8
void COverlappedIO::SignalSynchronousComplete(XOVERLAPPED* pOverlapped, u32 dwStatus,
                                              u32 dwExtendedError, u32 dwInternalHigh)
{
    XOVERLAPPED_COMPLETION_ROUTINE pRoutine = pOverlapped->pCompletionRoutine;

    pOverlapped->InternalHigh    = dwInternalHigh;
    pOverlapped->dwExtendedError = dwExtendedError;
    pOverlapped->InternalLow     = dwStatus;

    if (pRoutine)
    {
        pRoutine(dwStatus, dwInternalHigh, pOverlapped);
        return;
    }
    if (pOverlapped->hEvent)
        SetEvent(pOverlapped->hEvent);
}

// @ 0x82988358 -- self-freeing APC: dereference the thread it referenced and
// free the pooled APC block.
void COverlappedIO::ApcKernelRoutine(KAPC* pApc, void* /*ppNormalRoutine*/,
                                     void** /*ppNormalContext*/, void** /*ppSystemArgument1*/,
                                     void** /*ppSystemArgument2*/)
{
    ObDereferenceObject(pApc->Thread);
    ExFreePool(pApc);
}

// @ 0x82988320 -- identical self-free on rundown.
void COverlappedIO::ApcRundownRoutine(KAPC* pApc)
{
    ObDereferenceObject(pApc->Thread);
    ExFreePool(pApc);
}

// @ 0x82988390
void COverlappedIO::ApcNormalRoutine(void* pNormalContext, void* /*pSystemArgument1*/,
                                     void* /*pSystemArgument2*/)
{
    XOVERLAPPED* pOverlapped = static_cast<XOVERLAPPED*>(pNormalContext);
    if (pOverlapped->pCompletionRoutine)
        pOverlapped->pCompletionRoutine(pOverlapped->InternalLow,
                                        pOverlapped->InternalHigh, pOverlapped);
}

} // namespace XCAM
