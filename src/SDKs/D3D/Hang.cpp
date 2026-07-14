#include "SDKs/D3D/Hang.h"

#include <cstdarg> // va_list / va_start / va_end

// ===========================================================================
// D3D::Hang::Out -- reconstructed from BURNOUT_X360_ARTIST.XEX. See Hang.h for
// the role. Source-of-truth: X360 asm only (no DWARF, no reference source).
//
// The asm homes the variadic tail (r5..r10 -> arg_20..arg_48) onto the stack and
// builds a va_list pointing at that home area before calling RtlVsnprintf with a
// 260-byte count into a 264-byte stack buffer. That compiler-emitted homing is
// exactly what the portable <cstdarg> va_start/va_end idiom expresses, so it is
// de-optimised to that idiom with no change in behaviour. The trailing dispatch
// (test *ppfnOut, indirect-call-through-ctr vs. fall through to D3D::DXGPRINT)
// is reproduced faithfully with no inverted branch.
// ===========================================================================

// --- External X360 platform symbols referenced below -----------------------
// NT/X360 runtime bounded formatted-print into a caller buffer. Platform API.
extern "C" int RtlVsnprintf(char* Dest, unsigned int Count, const char* Format,
                            va_list Args);

namespace D3D
{

// Low-level D3D debug-console sink -- separate D3D TU (not yet homed). The asm
// loads only r3 (the message pointer) before the call, so it is surfaced here by
// name with the single attested argument.
int DXGPRINT(const char* pText);

namespace Hang
{

// --- @ 0x82952C50 ----------------------------------------------------------
int Out(OutCallback* ppfnOut, const char* pFormat, ...)
{
    char szBuffer[264]; // [sp+70h] var_120, BYREF

    va_list args;
    va_start(args, pFormat);            // home r5..r10 -> &arg_20, va_list -> that
    RtlVsnprintf(szBuffer, 260, pFormat, args); // Count = 0x104
    va_end(args);

    if (*ppfnOut != nullptr)            // lwz r11, 0(r31); cmplwi r11, 0
        return (*ppfnOut)(szBuffer);    // mtctr r11; bctrl
    return DXGPRINT(szBuffer);          // beq -> bl D3D__DXGPRINT
}

} // namespace Hang
} // namespace D3D
