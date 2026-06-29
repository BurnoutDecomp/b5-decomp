// Canonical RenderWare SDK home for the rwcore standard-C wrappers
// (EARenderWare rwcore, rw/core/stdc/stdc.cpp). Bodies reconstructed from the
// Burnout X360 image; see rw/core/stdc/stdc.h for the per-entry-point contracts.
//
// SCOPE NOTE: the integer/hex formatters (ConvertIToA / ConvertI64ToA /
// ConvertXToA) and the printf family (Vsprintf / Vsnprintf / Snprintf) are
// DECLARATION-ONLY in the header. Their X360 bodies dispatch into private rwcore
// recursive digit-emit helpers and float/field-padding routines that are not part
// of this class' function set and have not been reconstructed; reconstructing the
// bodies here would require fabricating those callees, so they are intentionally
// left undefined (the per-TU `cl /c` gate is compile-only and does not link).
// This file homes the fourteen self-contained mem*/string* primitives, each
// matched against its disassembly.

#include "rw/core/stdc/stdc.h"

namespace rw
{
namespace core
{
namespace stdc
{

// memset-style 32-bit fill (0x82BC70B0). Splats the value across luSize bytes of
// lpDst. The X360 build head-aligns the destination to a 4-byte boundary by
// emitting the high byte of a left-rotating word, optionally consumes one extra
// word to reach 8-byte alignment, then stores 64-, 16- and 4-byte blocks of the
// 32-bit pattern before a byte-granular tail. luValue is treated as a big-endian
// 32-bit pattern: the head/tail bytes emit its most-significant byte while the
// word stores write the whole 32-bit value, so MemClear/MemFill8 (which pass an
// already-broadcast pattern) fill uniformly.
void MemFill32(void* lpDst, u32 luValue, u32 luSize)
{
    u8* lpByte = static_cast<u8*>(lpDst);
    u32 luVal = luValue;

    // Head: byte-fill until the cursor is 4-byte aligned.
    while ((reinterpret_cast<usize>(lpByte) & 3) != 0 && luSize != 0)
    {
        --luSize;
        *lpByte++ = static_cast<u8>(luVal >> 24);
        luVal = (luVal << 8) | (luVal >> 24);
    }

    // One extra 4-byte store to reach 8-byte alignment, if there is room.
    if (luSize >= 4 && (reinterpret_cast<usize>(lpByte) & 4) != 0)
    {
        *reinterpret_cast<u32*>(lpByte) = luVal;
        luSize -= 4;
        lpByte += 4;
    }

    // 64-byte unrolled block via paired 64-bit stores (the pattern duplicated into
    // both halves of each doubleword).
    if (luSize >= 0x40)
    {
        u64 luQword = (static_cast<u64>(luVal) << 32) | luVal;
        do
        {
            luSize -= 64;
            u64* lpQword = reinterpret_cast<u64*>(lpByte);
            lpQword[0] = luQword;
            lpQword[1] = luQword;
            lpQword[2] = luQword;
            lpQword[3] = luQword;
            lpQword[4] = luQword;
            lpQword[5] = luQword;
            lpQword[6] = luQword;
            lpQword[7] = luQword;
            lpByte += 64;
        }
        while (luSize >= 0x40);
    }

    // 16-byte block.
    while (luSize >= 0x10)
    {
        u32* lpWord = reinterpret_cast<u32*>(lpByte);
        lpWord[0] = luVal;
        luSize -= 16;
        lpWord[1] = luVal;
        lpWord[2] = luVal;
        lpWord[3] = luVal;
        lpByte += 16;
    }

    // 4-byte block.
    while (luSize >= 4)
    {
        *reinterpret_cast<u32*>(lpByte) = luVal;
        luSize -= 4;
        lpByte += 4;
    }

    // Byte tail (same MSB-of-rotating-word emit as the head).
    for (; luSize != 0; ++lpByte)
    {
        --luSize;
        *lpByte = static_cast<u8>(luVal >> 24);
        luVal = (luVal << 8) | (luVal >> 24);
    }
}

// memset(dst, 0, size) (0x82BC71B8): a tail-call into MemFill32 with a zero pattern.
void MemClear(void* lpDst, u32 luSize)
{
    MemFill32(lpDst, 0, luSize);
}

// memset(dst, byte, size) (0x82BC71A0): broadcast the byte into all four lanes of a
// 32-bit pattern (byte * 0x01010101), then tail-call MemFill32.
void MemFill8(void* lpDst, u8 luValue, u32 luSize)
{
    MemFill32(lpDst, static_cast<u32>(luValue) * 0x01010101u, luSize);
}

// memcmp, signed (0x82BC71C8): compare luSize bytes of lpA against lpB, treating each
// byte as a signed char (the X360 build sign-extends before comparing). Returns -1 if
// lpA's byte is smaller, +1 if larger, 0 if all luSize bytes match.
s32 MemCompare(const void* lpA, const void* lpB, u32 luSize)
{
    if (luSize == 0)
    {
        return 0;
    }

    const s8* lpcA = static_cast<const s8*>(lpA);
    const s8* lpcB = static_cast<const s8*>(lpB);
    u32 luIndex = 0;
    for (;;)
    {
        s32 liA = lpcA[luIndex];
        s32 liB = lpcB[luIndex];
        if (liA < liB)
        {
            return -1;
        }
        if (liA > liB)
        {
            return 1;
        }
        ++luIndex;
        if (luIndex >= luSize)
        {
            return 0;
        }
    }
}

// memcpy (0x82BC81A0): for >= 64 bytes, head-align the source to 4, then copy 16-byte
// (4 words) blocks while the destination is word-aligned, with a byte tail; otherwise a
// straight byte copy. Word stores are used on the aligned fast path.
void MemCopy(void* lpDst, const void* lpSrc, u32 luSize)
{
    u8* lpDstByte = static_cast<u8*>(lpDst);
    const u8* lpSrcByte = static_cast<const u8*>(lpSrc);

    if (luSize >= 0x40)
    {
        // Head: byte-copy until the source is 4-byte aligned.
        while ((reinterpret_cast<usize>(lpSrcByte) & 3) != 0 && luSize != 0)
        {
            --luSize;
            *lpDstByte = *lpSrcByte++;
            ++lpDstByte;
        }

        // Word fast path only when the destination is also word-aligned.
        if ((reinterpret_cast<usize>(lpDstByte) & 3) == 0)
        {
            while (luSize >= 0x10)
            {
                luSize -= 16;
                const u32* lpSrcWord = reinterpret_cast<const u32*>(lpSrcByte);
                u32* lpDstWord = reinterpret_cast<u32*>(lpDstByte);
                lpDstWord[0] = lpSrcWord[0];
                lpDstWord[1] = lpSrcWord[1];
                lpDstWord[2] = lpSrcWord[2];
                lpDstWord[3] = lpSrcWord[3];
                lpSrcByte += 16;
                lpDstByte += 16;
            }
        }

        // Byte tail.
        while (luSize != 0)
        {
            --luSize;
            *lpDstByte = *lpSrcByte++;
            ++lpDstByte;
        }
    }
    else
    {
        // Small copies: straight byte loop.
        while (luSize != 0)
        {
            --luSize;
            *lpDstByte++ = *lpSrcByte++;
        }
    }
}

// strcat (0x82BC7248): walk lpcDst to its NUL, then append lpcSrc, NUL-terminate, and
// return lpcDst.
char* StringCat(char* lpcDst, const char* lpcSrc)
{
    char* lpcEnd = lpcDst;
    while (*lpcEnd != '\0')
    {
        ++lpcEnd;
    }

    while (*lpcSrc != '\0')
    {
        *lpcEnd++ = *lpcSrc++;
    }
    *lpcEnd = '\0';
    return lpcDst;
}

// strcmp, signed (0x82BC7298): advance while lpcA's byte is non-zero and equal to
// lpcB's, then return the sign of the (signed-char) difference of the diverging bytes.
s32 StringCompare(const char* lpcA, const char* lpcB)
{
    while (*lpcA != '\0' && *lpcA == *lpcB)
    {
        ++lpcA;
        ++lpcB;
    }

    s32 liA = static_cast<s8>(*lpcA);
    s32 liB = static_cast<s8>(*lpcB);
    if (liA > liB)
    {
        return 1;
    }
    if (liA < liB)
    {
        return -1;
    }
    return 0;
}

// stricmp (0x82BC72F0): case-fold lpcA's character toward lpcB's case each step, then
// compare. Folding: an upper-case lpcA against a lower-case lpcB is lowered (+32); a
// lower-case lpcA against an upper-case lpcB is raised (-32). The loop continues while
// both (folded) bytes are non-zero and equal; the result is the sign of the diverging
// (signed-char) byte difference.
s32 StringNoCaseCompare(const char* lpcA, const char* lpcB)
{
    s8 lcA;
    s8 lcB;
    do
    {
        lcA = static_cast<s8>(*lpcA++);
        lcB = static_cast<s8>(*lpcB++);

        if (lcA >= 'A' && lcA <= 'Z' && lcB >= 'a' && lcB <= 'z')
        {
            lcA = static_cast<s8>(lcA + 32);
        }
        else if (lcA >= 'a' && lcA <= 'z' && lcB >= 'A' && lcB <= 'Z')
        {
            lcA = static_cast<s8>(lcA - 32);
        }
    }
    while (lcA != 0 && lcB != 0 && lcA == lcB);

    if (lcA > lcB)
    {
        return 1;
    }
    if (lcA < lcB)
    {
        return -1;
    }
    return 0;
}

// strlen (0x82BC73A0): count characters up to the NUL.
s32 StringLength(const char* lpcString)
{
    s32 liLength = 0;
    while (*lpcString != '\0')
    {
        ++lpcString;
        ++liLength;
    }
    return liLength;
}

// strupr-copy (0x82BC73C8): copy lpcSrc into lpcDst, raising ASCII a-z to A-Z, then
// NUL-terminate and return lpcDst.
char* StringToUpperCase(char* lpcDst, const char* lpcSrc)
{
    char* lpcOut = lpcDst;
    for (;;)
    {
        s8 lcChar = static_cast<s8>(*lpcSrc);
        if (lcChar == 0)
        {
            break;
        }
        ++lpcSrc;
        if (lcChar >= 'a' && lcChar <= 'z')
        {
            lcChar = static_cast<s8>(lcChar - 32);
        }
        *lpcOut = static_cast<char>(lcChar);
        ++lpcOut;
    }
    *lpcOut = '\0';
    return lpcDst;
}

// strncpy-style (0x82BC79B8): copy up to luCount characters of lpcSrc into lpcDst,
// stopping at the source NUL. If the copy stops because the source ended (rather than
// running the count to zero), the destination is NUL-terminated; if the count is
// exhausted first, no terminator is written. Returns lpcDst. A luCount of zero copies
// nothing.
char* StringnCopy(char* lpcDst, const char* lpcSrc, u32 luCount)
{
    char* lpcStart = lpcDst;
    if (luCount == 0)
    {
        return lpcStart;
    }

    bool lbCountExhausted = false;
    while (true)
    {
        --luCount;
        if (*lpcSrc == '\0')
        {
            break;
        }
        *lpcDst++ = *lpcSrc++;
        if (luCount == 0)
        {
            lbCountExhausted = true;
            break;
        }
    }

    if (!lbCountExhausted)
    {
        *lpcDst = '\0';
    }
    return lpcStart;
}

} // namespace stdc
} // namespace core
} // namespace rw
