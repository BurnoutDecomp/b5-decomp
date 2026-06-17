#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptSprite.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   SpriteMembersIndex::in_word_set @ 0x82AD6800
//   SpriteMembersIndex::hash        @ 0x82AD6798
// (gperf perfect-hash recognizer for the Apt "Sprite" value object's member /
//  method names; called by AptCIH::objectMemberLookup / objectMemberSet and
//  AptNativeHash::UpdateObjectMethods).
//
// Faithful to the X360 pseudocode:
//   - length gate:  (len - 2) > 0x12  =>  reject   (2 <= len <= 20)
//   - hash:         len + asso[str0] + asso[str1] (+asso[str5] if len>=6)
//                                                 (+asso[str7] if len>=8)
//   - hash gate:    hash > 0x119 (281)  =>  reject
//   - entry:        &wordlist[hash]      (DIRECT hash index -- no lookup[] table,
//                                         no duplicate-key resolution)
//   - first-char compare, then strcmp(str+1, name+1); return Entry* or 0.

const SpriteMembersIndex::Entry*
SpriteMembersIndex::in_word_set(const char* lpcStr, unsigned int luLen)
{
    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if (luLen - MIN_WORD_LENGTH > MAX_WORD_LENGTH - MIN_WORD_LENGTH)   // (a2 - 2) > 0x12
        return 0;

    const unsigned int luKey = hash(lpcStr, luLen);                   // v3
    if (luKey > MAX_HASH_VALUE)                                       // v3 > 0x119
        return 0;

    // Direct hash index into the wordlist (X360: &off_82F78FD8 + 2 * v3).
    const Entry* lpEntry = &wordlist[luKey];                          // result
    const char*  lpcName = lpEntry->mpcName;                          // *result

    if ((unsigned char)lpcStr[0] != (unsigned char)lpcName[0])        // *a1 != **result
        return 0;

    const char* lpcN = lpcName + 1;                                   // v5 = *result + 1
    const char* lpcS = lpcStr  + 1;                                   // v6 = a1 + 1
    int liDiff;                                                       // v7
    do
    {
        liDiff = (unsigned char)*lpcS - (unsigned char)*lpcN;         // v7 = *v6 - *v5
        if (*lpcS == '\0')                                            // !*v6
            break;
        ++lpcS;
        ++lpcN;
    }
    while (liDiff == 0);                                              // while ( !v7 )

    if (liDiff != 0)                                                  // if ( v7 )
        return 0;
    return lpEntry;                                                   // return result
}

// --- gperf hash (asso_values[] @ 0x82F725B0) -----------------------------
// h  = len
// h += asso_values[str[0]]                  (always)
// h += asso_values[str[1]]                  (len >= 2 -- always true past the gate)
// h += asso_values[str[5]]                  (len >= 6)
// h += asso_values[str[7]]                  (len >= 8)
// The X360 control flow (LABEL_5 / LABEL_7 fall-through with the len 1/5/7
// branch points) is preserved structurally below.
unsigned int
SpriteMembersIndex::hash(const char* lpcStr, unsigned int luLen)
{
    const unsigned char* lpucStr = (const unsigned char*)lpcStr;     // a1
    unsigned int luHashVal = luLen;                                  // a2

    if (luLen != 1)
    {
        if (luLen > 1)
        {
            if (luLen <= 5)
            {
                // len in [2..5]: positions 1 then 0 only.
                luHashVal += asso_values[lpucStr[1]];
                return asso_values[lpucStr[0]] + luHashVal;
            }
            if (luLen > 7)
                luHashVal += asso_values[lpucStr[7]];                // len >= 8
            luHashVal += asso_values[lpucStr[5]];                    // len >= 6
        }
        // (len <= 1 with len != 1 is unreachable past the length gate; it would
        //  fall through to add position 7 -- harmless, omitted.)
        luHashVal += asso_values[lpucStr[1]];
        return asso_values[lpucStr[0]] + luHashVal;
    }
    return asso_values[lpucStr[0]] + luHashVal;                      // len == 1
}
