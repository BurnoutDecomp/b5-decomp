// =====================================================================================
// CRct -- rectangle value type used by the video (WMV/VP6) encoder pipeline.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CRct::operator=  @0x82A52A80
//
// CRct is a 16-byte (four 32-bit field) rectangle. The copy-assignment operator copies
// all four words verbatim (lwz/stw at +0,+4,+8,+0xC) and returns *this. The fields are
// the rectangle's two corner coordinates; their exact semantics are not load-bearing for
// the copy, so they are modelled as four signed 32-bit edges (left/top/right/bottom is the
// conventional CRct ordering for this encoder family).
// =====================================================================================

#include "types.hpp"

class CRct
{
public:
    CRct& operator=(const CRct& lrSrc);

private:
    s32 miLeft;    // +0x00
    s32 miTop;     // +0x04
    s32 miRight;   // +0x08
    s32 miBottom;  // +0x0C
};

// CRct::operator= @0x82A52A80 -- verbatim 4-word copy (lwz/stw +0,+4,+8,+0xC), return this.
CRct& CRct::operator=(const CRct& lrSrc)
{
    miLeft   = lrSrc.miLeft;
    miTop    = lrSrc.miTop;
    miRight  = lrSrc.miRight;
    miBottom = lrSrc.miBottom;
    return *this;
}
