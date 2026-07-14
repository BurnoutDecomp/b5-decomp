#ifndef SDKS_XAM_CXLIVEASYNCTASK_H
#define SDKS_XAM_CXLIVEASYNCTASK_H

#include "types.hpp"
#include "SDKs/Xam/CBaseEndianBuffer.h"

// ===========================================================================
// SDKs/Xam/CXLiveAsyncTask.h
//
// CXLiveAsyncTask -- the Xbox 360 XLive/XOnline "user buffer" asynchronous RPC
// task descriptor in BURNOUT_X360_ARTIST.XEX. XOnlineSetupUserBuffer builds one
// of these, then calls Initialize to record the request parameters, bind an
// endian-aware wire buffer over the caller's span (CBaseEndianBuffer::Bind), and
// arm the caller's XOVERLAPPED for an asynchronous (ERROR_IO_PENDING) completion.
//
// Sibling of the Xam RPC marshalling cluster (CMarshaller / CArgumentList /
// CBaseEndianBuffer) which it embeds; the overlapped-arming half mirrors
// XCAM::COverlappedIO (SDKs/XCam) -- same XOVERLAPPED slice (hEvent @ +0x0C) and
// the same ERROR_IO_PENDING (997) pending-stamp. These are Microsoft-XDK-style
// C-prefixed classes, so the identifiers (CXLiveAsyncTask, Initialize) are kept
// verbatim per the naming convention's external/platform-API carve-out.
//
// There is no reference source and no DWARF for this TU; the SHAPE below is
// reconstructed store-for-store from the X360 asm of the single function
// (Initialize @ 0x8297F3F8). Hex-Rays inflated the argument list to 42 by
// misreading the eight 8-byte stack slots (it reads only their low words); the
// routine actually takes 15 arguments -- 7 GPR value args (a2..a8) plus 8 stack
// args (a28,a30,a32,a34,a36,a38,a40,a42) -- each consumed as a 32-bit word.
//
// LAYOUT (X360-attested store-for-store). The 13 leading fields are all 32-bit
// words, so their offsets hold on the 64-bit host up to the embedded buffer; the
// buffer's own leading pointer (CBaseEndianBuffer::mpData) widens on the host, so
// mBuffer's END and the XOVERLAPPED pointer after it shift. Only offsets up to and
// including mBuffer @ +0x34 are host-assertable:
//   +0x00 muField00     (Initialize a2)
//   +0x04 muField04     (a3)
//   +0x08 muField08     (a4)
//   +0x0C muFlags       (a5; if (a5 & 0x40) and running in the title process,
//                        bit 0x10 is OR'd in before storing)
//   +0x10 muField10     (a6)
//   +0x14 muField14     (a7)
//   +0x18 muField18     (a30)
//   +0x1C muField1C     (zeroed at init)
//   +0x20 muField20     (a32)
//   +0x24 muField24     (a8)
//   +0x28 muField28     (a28)
//   +0x2C muField2C     (a38)
//   +0x30 muField30     (a40)
//   +0x34 mBuffer       CBaseEndianBuffer -- wire buffer bound over (a34,a36)
//   +0x48 mpOverlapped  XOVERLAPPED* (a42; X360 offset -- shifts on the host)
// ===========================================================================

// Minimal XDK XOVERLAPPED slice (real definition lives in the Xbox 360 XDK
// <xtl.h>). Only InternalLow (+0x00) and hEvent (+0x0C) are touched by this TU;
// the placement matches the real XDK (and XCAM::XOVERLAPPED) -- hEvent at +0x0C.
// Namespaced so it does not collide with the differently-labelled global
// XOVERLAPPED that CgsXOverlapped.h reconstructs from its own (hEvent-blind) asm.
namespace XLive
{
    typedef void* HANDLE;   // Xbox-360 kernel handle primitive (platform boundary)

    struct XOVERLAPPED
    {
        u32    InternalLow;      // +0x00  Win32 status (stamped ERROR_IO_PENDING)
        u32    InternalHigh;     // +0x04
        u32    InternalContext;  // +0x08
        HANDLE hEvent;           // +0x0C  event handle (ResetEvent'd when present)
        // remaining XDK fields (completion routine / context / extended error) unused here
    };
}

class CXLiveAsyncTask
{
public:
    // @ 0x8297F3F8 -- record the request parameters, bind the wire buffer over the
    // caller's span, and arm the caller's overlapped for asynchronous completion.
    // If (luFlags & 0x40) and the current process is the title process, bit 0x10 is
    // folded into the stored flags. When an overlapped is supplied, its event is
    // reset (if any) and its status word is stamped ERROR_IO_PENDING (997). Always
    // returns 0. Parameters are in X360 ABI order (a2..a8 in GPRs, then the eight
    // stack args by ascending slot); see the header note on the inflated count.
    s32 Initialize(u32 luField00, u32 luField04, u32 luField08, u32 luFlags,
                   u32 luField10, u32 luField14, u32 luField24,
                   u32 luField28, u32 luField18, u32 luField20,
                   u8* lpBufferData, u32 luBufferContext,
                   u32 luField2C, u32 luField30,
                   XLive::XOVERLAPPED* lpOverlapped);

private:
    // Never emitted; pins the X360-attested field offsets on the LLP64 gate for the
    // pointer-invariant leading words (everything up to the embedded buffer).
    static void _AssertLayout();

    u32                 muField00;    // +0x00
    u32                 muField04;    // +0x04
    u32                 muField08;    // +0x08
    u32                 muFlags;      // +0x0C
    u32                 muField10;    // +0x10
    u32                 muField14;    // +0x14
    u32                 muField18;    // +0x18
    u32                 muField1C;    // +0x1C  (zeroed at init)
    u32                 muField20;    // +0x20
    u32                 muField24;    // +0x24
    u32                 muField28;    // +0x28
    u32                 muField2C;    // +0x2C
    u32                 muField30;    // +0x30
    CBaseEndianBuffer   mBuffer;      // +0x34  embedded wire buffer (0x14 bytes on X360)
    XLive::XOVERLAPPED* mpOverlapped; // +0x48 (X360) -- shifts on the 64-bit host
};

#endif // SDKS_XAM_CXLIVEASYNCTASK_H
