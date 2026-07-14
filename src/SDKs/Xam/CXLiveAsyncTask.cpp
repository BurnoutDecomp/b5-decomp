// CXLiveAsyncTask.cpp -- body reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See CXLiveAsyncTask.h for the layout and asm notes.
//
//   CXLiveAsyncTask::Initialize @ 0x8297F3F8

#include "SDKs/Xam/CXLiveAsyncTask.h"

#include <cstddef> // offsetof

// Win32 / Xbox-360 kernel entry points this TU calls. Declared here directly (the
// Xam headers pull no <windows.h>), mirroring CMarshaller.cpp's WideCharToMultiByte
// precedent. Supplied by the platform layer; compile-only for the PC gate.
extern "C" int __stdcall ResetEvent(void* hEvent);   // Win32: BOOL ResetEvent(HANDLE)
extern "C" int           KeGetCurrentProcessType();  // Xbox 360 kernel export

// Flag bits Initialize manipulates on the stored request flags (a5). The 0x40 bit
// gates a title-process check; when set in the title process, 0x10 is folded in.
// Raw X360 constants -- the exact XLive flag semantics beyond the asm are unattested.
static const u32 KU_FLAG_TITLE_GATED = 0x40u; // rlwinm. r11,r30,0,25,25 (bit 0x40)
static const u32 KU_FLAG_TITLE       = 0x10u; // ori r30,r30,0x10
// KeGetCurrentProcessType() == 1 is the title (user) process on the Xbox 360.
static const int KI_PROCTYPE_TITLE   = 1;     // cmpwi cr6,r3,1
// Win32 status stamped into the overlapped while the request is pending.
static const u32 KU_ERROR_IO_PENDING = 997u;  // li r10,0x3E5 -- ERROR_IO_PENDING

// ---------------------------------------------------------------------------
// CXLiveAsyncTask::Initialize @ 0x8297F3F8
// Record the request parameters, bind the wire buffer, and arm the overlapped.
// ---------------------------------------------------------------------------
s32 CXLiveAsyncTask::Initialize(u32 luField00, u32 luField04, u32 luField08, u32 luFlags,
                                u32 luField10, u32 luField14, u32 luField24,
                                u32 luField28, u32 luField18, u32 luField20,
                                u8* lpBufferData, u32 luBufferContext,
                                u32 luField2C, u32 luField30,
                                XLive::XOVERLAPPED* lpOverlapped)
{
    // Resolve the stored flags: bit 0x40 gates a "running in the title process"
    // check; when both hold, bit 0x10 is folded in (r30 |= 0x10).
    u32 luResolvedFlags = luFlags;
    if ((luFlags & KU_FLAG_TITLE_GATED) != 0 && KeGetCurrentProcessType() == KI_PROCTYPE_TITLE)
        luResolvedFlags |= KU_FLAG_TITLE;

    // Store the request fields (asm store order preserved).
    muField18 = luField18; // stw r11(arg_5C), 0x18
    muField2C = luField2C; // stw r3(arg_7C),  0x2C
    muField00 = luField00; // stw r29,         0x00
    muField04 = luField04; // stw r28,         0x04
    muField08 = luField08; // stw r27,         0x08
    muFlags   = luResolvedFlags; // stw r30,   0x0C
    muField10 = luField10; // stw r26,         0x10
    muField14 = luField14; // stw r25,         0x14
    muField1C = 0;         // stw r10(=0),     0x1C
    muField20 = luField20; // stw r9(arg_64),  0x20
    muField24 = luField24; // stw r24(=r10),   0x24
    muField28 = luField28; // stw r8(arg_54),  0x28
    muField30 = luField30; // stw r11(arg_84), 0x30

    // Bind the endian-aware wire buffer over the caller's span (size 0, swap on).
    mBuffer.Bind(lpBufferData, luBufferContext, 0, 1); // CBaseEndianBuffer::Bind(a1+0x34, a34, a36, 0, 1)

    // Arm the caller's overlapped: reset its event (if any) and stamp it pending.
    mpOverlapped = lpOverlapped; // stw r11(arg_8C), 0x48
    if (lpOverlapped)
    {
        if (lpOverlapped->hEvent)         // lwz r3, 0xC(r11); cmplwi r3, 0
            ResetEvent(lpOverlapped->hEvent);
        lpOverlapped->InternalLow = KU_ERROR_IO_PENDING; // stw r10(0x3E5), 0(overlapped)
    }
    return 0; // li r3, 0
}

// ---------------------------------------------------------------------------
// Never emitted. Pins the X360-attested field offsets of the pointer-invariant
// leading words on the LLP64 gate. Fields past mBuffer shift on the 64-bit host
// (the embedded buffer's leading pointer widens), so they are NOT asserted.
// ---------------------------------------------------------------------------
void CXLiveAsyncTask::_AssertLayout()
{
    static_assert(offsetof(CXLiveAsyncTask, muField00) == 0x00, "muField00 @ +0x00");
    static_assert(offsetof(CXLiveAsyncTask, muField04) == 0x04, "muField04 @ +0x04");
    static_assert(offsetof(CXLiveAsyncTask, muField08) == 0x08, "muField08 @ +0x08");
    static_assert(offsetof(CXLiveAsyncTask, muFlags)   == 0x0C, "muFlags @ +0x0C");
    static_assert(offsetof(CXLiveAsyncTask, muField10) == 0x10, "muField10 @ +0x10");
    static_assert(offsetof(CXLiveAsyncTask, muField14) == 0x14, "muField14 @ +0x14");
    static_assert(offsetof(CXLiveAsyncTask, muField18) == 0x18, "muField18 @ +0x18");
    static_assert(offsetof(CXLiveAsyncTask, muField1C) == 0x1C, "muField1C @ +0x1C");
    static_assert(offsetof(CXLiveAsyncTask, muField20) == 0x20, "muField20 @ +0x20");
    static_assert(offsetof(CXLiveAsyncTask, muField24) == 0x24, "muField24 @ +0x24");
    static_assert(offsetof(CXLiveAsyncTask, muField28) == 0x28, "muField28 @ +0x28");
    static_assert(offsetof(CXLiveAsyncTask, muField2C) == 0x2C, "muField2C @ +0x2C");
    static_assert(offsetof(CXLiveAsyncTask, muField30) == 0x30, "muField30 @ +0x30");
    // mBuffer sits at X360 +0x34 (13 u32 words), but the embedded buffer's leading
    // pointer forces 8-byte alignment on the host, padding it to +0x38 there -- so
    // its offset is not a pointer-invariant fact and is deliberately not asserted.
}
