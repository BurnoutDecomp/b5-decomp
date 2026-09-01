#include "rw/RwHash32String.h"

#include <cstring>  // memcpy (the X360 operator- opens with memcpy(result, lhs, 40))

// ===========================================================================
// rw:: top-level free functions -- out-of-line home.
//
// OWNING HOME for:
//     rw::RwHash32String              @ 0x82BBC458
//     rw::operator-(ResourceDescriptor) @ 0x82B63E50
//
// Both reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX; see the
// per-function notes below and the declarations in rw/RwHash32String.h.
// ===========================================================================

namespace rw
{

// --- rw::RwHash32String @ 0x82BBC458 ---------------------------------------
// FNV-1a 32-bit over a NUL-terminated string. The X360 loop:
//     loc: lbz r10,0(r11); if r10==0 break;            ; byte = *p; stop at NUL
//          r11++; r9 = 0x01000193; r9 = r4*r9; r4 = r9 ^ r10
// i.e. for each non-zero byte: acc = (acc * 0x01000193) ^ byte. 0x01000193 is
// the 32-bit FNV prime. The seed (r4) is the caller-supplied accumulator and is
// returned unchanged for an empty string.
uint32_t RwHash32String(const char* lpcString, uint32_t luSeed)
{
    const unsigned char* lpByte = reinterpret_cast<const unsigned char*>(lpcString);
    uint32_t luHash = luSeed;
    for (unsigned char luChar = *lpByte; luChar != 0; luChar = *++lpByte)
    {
        luHash = (luHash * 0x01000193u) ^ luChar;
    }
    return luHash;
}

// --- rw::operator-(ResourceDescriptor) @ 0x82B63E50 ------------------------
// result = lhs; then for the (X360) five entries subtract rhs.m_size from
// result.m_size at the 8-byte BaseResourceDescriptor stride (the asm subtracts
// only the first dword of each entry; m_alignment is carried over from lhs by
// the opening memcpy). The loop count and 0x28-byte copy both establish five lanes.
ResourceDescriptor operator-(const ResourceDescriptor& lrLhs, const ResourceDescriptor& lrRhs)
{
    ResourceDescriptor lResult = lrLhs;  // memcpy(result, lhs) -- carries m_size + m_alignment
    for (uint32_t luIndex = 0; luIndex < KU_RESOURCE_LANE_COUNT; ++luIndex)
    {
        lResult.m_baseResourceDescriptors[luIndex].m_size -=
            lrRhs.m_baseResourceDescriptors[luIndex].m_size;
    }
    return lResult;
}

}  // namespace rw
