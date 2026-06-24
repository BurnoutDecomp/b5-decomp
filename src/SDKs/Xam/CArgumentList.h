#ifndef SDKS_XAM_CARGUMENTLIST_H
#define SDKS_XAM_CARGUMENTLIST_H

#include "types.hpp"

// ===========================================================================
// SDKs/Xam/CArgumentList.h
//
// CArgumentList -- the fixed-capacity argument buffer the Xbox 360 XAM (Xbox
// Account Manager) RPC marshalling layer fills before a cross-process call. The
// X* service shims (XFriendsCreateEnumerator, XInviteSend, XStringVerify,
// _XAccountGetUserInfo, ...) push each typed argument with AddArgument before
// dispatching, alongside the sibling CSchemaData / CMarshaller / CConformanceList
// marshalling classes.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AddArgument @ 0x8297DC68). There is
// no reference source and no DWARF for this TU; the SHAPE below is grounded
// purely on the X360 asm of AddArgument. These are Microsoft-XDK-style C-prefixed
// marshalling classes, so the identifiers (CArgumentList, AddArgument) are kept
// verbatim per the naming convention's external/platform-API carve-out.
//
// LAYOUT (from AddArgument's asm):
//   AddArgument computes the slot pointer as `this + 16 * count`, writing the
//   element in place; count lives at byte offset +0x200. With a 16-byte element
//   stride that is exactly 32 element slots (0x200 / 16) ahead of the count word:
//     +0x000  maElements[32]   -- the argument slots (16 bytes each)
//     +0x200  muCount          -- number of arguments pushed so far (u32)
//
//   Each element (16 bytes; the std/stw widths are authoritative):
//     +0x00  muType   -- argument type tag; AddArgument stores the constant 4
//                        (`li r9,4 ; stw r9,0(r10)`) for the int64 argument it adds
//     +0x04  (unused padding -- the asm never touches it)
//     +0x08  miValue  -- the 64-bit argument value (`std r8,8(r10)`, where r8 is the
//                        incoming 32-bit argument sign-extended via `extsw r8,r4`)
// ===========================================================================

class CArgumentList
{
public:
    // Argument type tag values. AddArgument records type 4 for the int64 argument
    // variant reconstructed here; the full enumerator set lives in the wider XAM
    // marshalling headers, so only the load-bearing value is modelled.
    enum E_ArgType
    {
        E_ARG_INT64 = 4
    };

    // A single pushed argument slot (16-byte stride; see header LAYOUT).
    struct Element
    {
        u32 muType;   // +0x00
        u32 muPad;    // +0x04 (unused by AddArgument)
        s64 miValue;  // +0x08
    };

    // @ 0x8297DC68 -- append a 64-bit integer argument to the list.
    // Writes maElements[muCount] = { type=4, value=sign_extend(liValue) }, then
    // post-increments muCount. Returns the index the argument was stored at.
    int AddArgument(int liValue);

    Element maElements[32]; // +0x000 .. +0x1FF
    u32     muCount;        // +0x200
};

#endif // SDKS_XAM_CARGUMENTLIST_H
