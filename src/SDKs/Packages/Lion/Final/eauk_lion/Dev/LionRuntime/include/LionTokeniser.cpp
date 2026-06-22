// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.cpp
//
// sLionMemberToken / cLionTokenTable EndianTwiddle implementation.
// Reconstructed from the DecFIGS DWARF hints + the X360 asm (asm is authority for
// the per-type swap widths and the struct-token dedup logic). Members are
// accessed by name; byte-swaps are written as clean reads/writes, not the X360's
// strength-reduced shift chains.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"

namespace
{
    // Byte-reverse a 16-bit value at lpByte (big-endian <-> little-endian).
    inline void SwapU16(u8* lpByte)
    {
        const u8 lb0 = lpByte[0];
        const u8 lb1 = lpByte[1];
        const u16 luValue = static_cast<u16>((static_cast<u16>(lb0) << 8) | lb1);
        *reinterpret_cast<u16*>(lpByte) = luValue;
    }

    // Byte-reverse a 32-bit value at lpByte.
    inline void SwapU32(u8* lpByte)
    {
        const u8 lb0 = lpByte[0];
        const u8 lb1 = lpByte[1];
        const u8 lb2 = lpByte[2];
        const u8 lb3 = lpByte[3];
        const u32 luValue = (((((static_cast<u32>(lb0) << 8) | lb1) << 8) | lb2) << 8) | lb3;
        *reinterpret_cast<u32*>(lpByte) = luValue;
    }
}

// ----------------------------------------------------------------------------
// sLionMemberToken::EndianTwiddle  @ 0x82908B48
// Byte-swap this member's value in place within the struct image based at
// apStruct. The swap width is selected by mType (matching the X360 jump table).
// ----------------------------------------------------------------------------
void sLionMemberToken::EndianTwiddle(void* apStruct) const
{
    if (!apStruct)
    {
        return;
    }

    u8* lpField = static_cast<u8*>(apStruct) + mOffset;

    switch (mType)
    {
        // mType 3,4 -> one 16-bit value.
        case E_LION_MEMBER_S16:
        case E_LION_MEMBER_U16:
            SwapU16(lpField);
            break;

        // mType 5..11,14 -> one 32-bit value.
        case E_LION_MEMBER_S32:
        case E_LION_MEMBER_U32:
        case E_LION_MEMBER_F32:
        case E_LION_MEMBER_ENUM:
        case E_LION_MEMBER_STRUCT:
        case E_LION_MEMBER_HASH:
        case E_LION_MEMBER_COLOUR:
        case E_LION_MEMBER_POINTER:
            SwapU32(lpField);
            break;

        // mType 12 -> 64-byte block (16 words), swapped four words at a time.
        case E_LION_MEMBER_MATRIX:
        {
            u8* lpWord = lpField;
            for (s32 lIndex = 4; lIndex != 0; --lIndex)
            {
                SwapU32(lpWord + 0);
                SwapU32(lpWord + 4);
                SwapU32(lpWord + 8);
                SwapU32(lpWord + 12);
                lpWord += 16;
            }
            break;
        }

        // mType 13,15 -> 16-byte block (4 words).
        case E_LION_MEMBER_VECTOR:
        case E_LION_MEMBER_QUAT:
            SwapU32(lpField + 0);
            SwapU32(lpField + 4);
            SwapU32(lpField + 8);
            SwapU32(lpField + 12);
            break;

        default:
            break;
    }
}

// ----------------------------------------------------------------------------
// cLionTokenTable::EndianTwiddle  @ 0x82908E08
// Walk every token in the table and byte-swap the corresponding member of the
// struct image based at apStruct. A struct-typed member (E_LION_MEMBER_STRUCT)
// that re-appears at the same offset later in the table is twiddled only once
// (the first occurrence is skipped so the later duplicate performs the swap).
// ----------------------------------------------------------------------------
void cLionTokenTable::EndianTwiddle(void* apStruct) const
{
    if (!apStruct)
    {
        return;
    }

    for (sLionMemberToken* lpToken = mpTokens; lpToken->mType != E_LION_MEMBER_NONE; ++lpToken)
    {
        bool lbSkip = false;

        if (lpToken->mType == E_LION_MEMBER_STRUCT)
        {
            for (sLionMemberToken* lpScan = lpToken + 1; lpScan->mType != E_LION_MEMBER_NONE; ++lpScan)
            {
                if (lpScan->mType == E_LION_MEMBER_STRUCT && lpToken->mOffset == lpScan->mOffset)
                {
                    lbSkip = true;
                }
            }
        }

        if (!lbSkip)
        {
            lpToken->EndianTwiddle(apStruct);
        }
    }
}
