#pragma once

// ===========================================================================
// XCAM::COverlappedIO -- the asynchronous overlapped-completion helper used by
// the XCAM (X360 camera/capture) SDK in BURNOUT_X360_ARTIST.XEX. It drives an
// XOVERLAPPED to completion through one of three mechanisms, chosen at Setup
// time from the fields the caller filled in:
//
//   * a queued kernel APC  (when the overlapped carries a completion routine),
//   * a signalled KEVENT   (when the overlapped carries an event handle),
//   * a direct/synchronous completion callback (SignalSynchronousComplete).
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamOverlappedIO.cpp:
//
//     XCAM::COverlappedIO::Setup                      @ 0x829883B8
//     XCAM::COverlappedIO::SignalComplete             @ 0x82988498
//     XCAM::COverlappedIO::SignalSynchronousComplete  @ 0x829882D8  (static)
//     XCAM::COverlappedIO::ApcKernelRoutine           @ 0x82988358  (static)
//     XCAM::COverlappedIO::ApcRundownRoutine          @ 0x82988320  (static)
//     XCAM::COverlappedIO::ApcNormalRoutine           @ 0x82988390  (static)
//
// There is no reference source and no DWARF for this TU; the layout below is
// reconstructed store-for-store from the X360 asm. `XCAM` is an X360 SDK
// boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// COverlappedIO instance LAYOUT (store-for-store with the asm):
//   +0x00 mpOverlapped    the XOVERLAPPED being driven (cleared on completion)
//   +0x04 mpApc           the queued KAPC (APC path), else null
//   +0x08 mpEventObject   the referenced KEVENT object (event path), else null
// ===========================================================================

#include "types.hpp"

namespace XCAM
{

// --- Xbox-360 kernel handle primitive (platform boundary) ------------------
typedef void* HANDLE;

// Forward declaration so the completion-routine typedef can name the struct.
struct XOVERLAPPED;

// The Win32/Xbox extended-overlapped completion callback. Invoked with the
// completed error code, the transferred/returned count, and the overlapped.
typedef void (*XOVERLAPPED_COMPLETION_ROUTINE)(u32 dwError, u32 dwReturned,
                                               XOVERLAPPED* pOverlapped);

// --- Xbox-360 XOVERLAPPED (extended overlapped) ----------------------------
// The extended overlapped structure the XDK asynchronous APIs complete. Field
// placement is store-for-store with the +0/+4/+0xC/+0x10/+0x18 accesses in
// Setup, SignalComplete and SignalSynchronousComplete.
struct XOVERLAPPED
{
    u32                            InternalLow;         // +0x00 Win32 status
    u32                            InternalHigh;        // +0x04 bytes transferred
    u32                            InternalContext;     // +0x08
    HANDLE                         hEvent;              // +0x0C event handle
    XOVERLAPPED_COMPLETION_ROUTINE pCompletionRoutine;  // +0x10 completion cb
    u32                            dwCompletionContext; // +0x14
    u32                            dwExtendedError;     // +0x18
};

// --- Xbox-360 kernel APC (platform boundary) -------------------------------
// The 40-byte KAPC block Setup allocates from pool and hands to KeInitializeApc.
// Only the Thread back-reference (+0x04) is read directly here (the self-freeing
// APC dereferences it and frees the block); the remaining fields are populated
// by KeInitializeApc and left opaque.
struct KAPC
{
    u8    Reserved0[4]; // +0x00 Type / SpareByte / ApcMode / Inserted
    void* Thread;       // +0x04 referenced KTHREAD back-pointer
    u8    Reserved8[32];// +0x08 remainder (list entry + routines + context)
};

// --- Xbox-360 kernel APC callback function-pointer types (platform boundary)
// The exact kernel-routine ABI so the three static callbacks are assignable to
// KeInitializeApc's parameters. The reconstructed bodies use only the arguments
// the asm touches; the rest are named-but-unused per the kernel signature.
typedef void (*PKKERNEL_ROUTINE)(KAPC* pApc, void* ppNormalRoutine,
                                 void** ppNormalContext, void** ppSystemArgument1,
                                 void** ppSystemArgument2);
typedef void (*PKRUNDOWN_ROUTINE)(KAPC* pApc);
typedef void (*PKNORMAL_ROUTINE)(void* pNormalContext, void* pSystemArgument1,
                                 void* pSystemArgument2);

// --- Xbox-360 kernel / Win32 entry points (platform boundary) --------------
// Declared with the argument/return shapes the X360 call sites use so the
// reconstructed bodies compile store-for-store. Supplied by the platform layer;
// NOT reconstructed here.
void* ExAllocatePoolWithTag(u32 uNumberOfBytes, u32 uTag);
void  ExFreePool(void* pAddress);
void  ObReferenceObject(void* pObject);
void  ObDereferenceObject(void* pObject);
s32   ObReferenceObjectByHandle(HANDLE hHandle, void* pObjectType, void** ppObject);
void* KeGetCurrentThread();
void  KeInitializeApc(KAPC* pApc, void* pThread, PKKERNEL_ROUTINE pKernelRoutine,
                      PKRUNDOWN_ROUTINE pRundownRoutine, PKNORMAL_ROUTINE pNormalRoutine,
                      u8 uApcMode, void* pNormalContext);
u8    KeInsertQueueApc(KAPC* pApc, void* pSystemArgument1, void* pSystemArgument2,
                       s32 iIncrement);
void  KeSetEvent(void* pEvent, s32 iIncrement, u8 bWait);
int   SetEvent(HANDLE hEvent);

// The event OBJECT_TYPE the kernel exposes as a global pointer; loaded by value
// and passed to ObReferenceObjectByHandle in Setup.
extern void* ExEventObjectType;

// Pool tag baked into Setup's KAPC allocation (`lis 0x7061 ; ori 0x7350`).
static const u32 KU_XCAM_APC_POOL_TAG = 0x70617350u; // 'pasP'
// Size of the pooled KAPC block Setup allocates (`li r3, 0x28`).
static const u32 KU_XCAM_APC_SIZE = 40u;
// Win32 status Setup stamps into the overlapped while the request is pending.
static const u32 KU_XCAM_IO_PENDING = 997u; // ERROR_IO_PENDING

class COverlappedIO
{
public:
    // @ 0x829883B8 -- initialise this helper from a freshly-issued overlapped:
    // stamp it pending, then arm either an APC (completion routine present) or a
    // referenced event (event handle present). Returns 0 on success, 8
    // (ERROR_NOT_ENOUGH_MEMORY) if the APC pool alloc fails, or 6
    // (ERROR_INVALID_HANDLE) if the event handle cannot be referenced.
    u32 Setup(XOVERLAPPED* pOverlapped);

    // @ 0x82988498 -- complete the overlapped: publish (extended-error, high,
    // status) with a release barrier, then fire the APC (freeing it if it could
    // not be queued) or signal+release the event, and detach the overlapped.
    void SignalComplete(u32 dwStatus, u32 dwExtendedError, u32 dwInternalHigh);

    // @ 0x829882D8 -- synchronous completion straight onto an overlapped (no APC
    // context): publish the fields, then either invoke the completion routine
    // directly or set the event handle.
    static void SignalSynchronousComplete(XOVERLAPPED* pOverlapped, u32 dwStatus,
                                          u32 dwExtendedError, u32 dwInternalHigh);

    // @ 0x82988358 -- APC kernel routine: dereference the APC's thread and free
    // the pooled APC block (the self-freeing APC idiom).
    static void ApcKernelRoutine(KAPC* pApc, void* ppNormalRoutine,
                                 void** ppNormalContext, void** ppSystemArgument1,
                                 void** ppSystemArgument2);

    // @ 0x82988320 -- APC rundown routine (same self-free as the kernel routine).
    static void ApcRundownRoutine(KAPC* pApc);

    // @ 0x82988390 -- APC normal routine: invoke the overlapped's completion
    // routine (if any) with its status and transferred count.
    static void ApcNormalRoutine(void* pNormalContext, void* pSystemArgument1,
                                 void* pSystemArgument2);

private:
    XOVERLAPPED* mpOverlapped;   // +0x00
    KAPC*        mpApc;          // +0x04
    void*        mpEventObject;  // +0x08
};

} // namespace XCAM
