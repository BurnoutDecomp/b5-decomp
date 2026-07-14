#pragma once

// ===========================================================================
// D3D::Hang -- the GPU-hang debug-report helpers inside BURNOUT_X360_ARTIST.XEX.
// This header is the canonical OWNING home for the D3D::Hang namespace helper
// bodied in Hang.cpp:
//
//     D3D::Hang::Out  @ 0x82952C50
//
// `Out` is the shared formatted-print sink used by the hang diagnostics
// (D3D::HangStateDump, D3D::HangCommandBufferDump, D3D::HangRecognizer -- each a
// separate, not-yet-homed TU). It renders a printf-style message into a local
// 260-byte buffer via RtlVsnprintf, then either forwards the finished string to
// a caller-supplied output callback (`*ppfnOut`) or, when no callback is
// installed, to the low-level D3D::DXGPRINT debug console.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (Hang, Out) are
// preserved verbatim per the naming convention.
//
// There is NO DWARF and NO reference source for this TU. The signature and body
// are reconstructed purely from the X360 asm: the two named leading parameters
// (the output-callback slot in r3 and the format string in r4) followed by the
// variadic argument tail homed from r5..r10.
// ===========================================================================

#include "types.hpp"

namespace D3D
{
namespace Hang
{

// Output callback: receives the fully formatted, NUL-terminated message and
// returns an int status. `Out` is handed the *address of* such a slot so a null
// slot means "no callback installed".
typedef int (*OutCallback)(char* pText);

// @ 0x82952C50 -- format `pFormat` (+ varargs) into a local 260-byte buffer,
// then dispatch: if *ppfnOut is non-null, call it with the buffer and return its
// result; otherwise route the buffer to D3D::DXGPRINT.
int Out(OutCallback* ppfnOut, const char* pFormat, ...);

} // namespace Hang
} // namespace D3D
