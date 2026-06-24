#include "SDKs/XGraphics/XGraphicsIRFetch.h"

// ===========================================================================
// XGRAPHICS::IRFetch::InstType @ 0x82C2B8A0 -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. The asm loads the address of the string literal
// "fetch" (aFetch) into r3 and returns it; nothing else.
// ===========================================================================

namespace XGRAPHICS
{

const char* IRFetch::InstType()
{
    return "fetch";
}

} // namespace XGRAPHICS
