#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptString.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82AD7918
//   (StringMembersIndex::in_word_set -- gperf perfect-hash recognizer for
//    AptString member/method names; called by AptString::objectMemberLookup)
//
// Faithful to the X360 pseudocode:
//   - length gate:  (len - 5) > 7  =>  reject  (5 <= len <= 12)
//   - hash:         asso_values[str[len-1]] + asso_values[str[0]] + len
//   - hash gate:    hash > 0x16  =>  reject
//   - lookup[hash] >= 0            : single keyword at wordlist[index]
//   - lookup[hash] in [-13, -1]    : empty slot  =>  reject
//   - lookup[hash] <= -14          : duplicate list; offset = -14 - lookup[hash],
//                                    base  = &wordlist[lookup[offset]],
//                                    count =  lookup[offset + 1]
// Both match paths do first-char compare then strcmp(str+1, name+1) and return
// the matching Entry*, else 0.

const struct StringMembersIndex::Entry*
StringMembersIndex::in_word_set(const char* lpcStr, unsigned int luLen)
{
    if (luLen - MIN_WORD_LENGTH > MAX_WORD_LENGTH - MIN_WORD_LENGTH)  // (luLen - 5) > 7
        return 0;

    const unsigned int luKey = hash(lpcStr, luLen);
    if (luKey > MAX_HASH_VALUE)
        return 0;

    const int liIndex = lookup[luKey];

    if (liIndex >= 0)
    {
        // Single (non-duplicate) keyword slot.
        const struct Entry* lpEntry = &wordlist[liIndex];
        const char* lpcName = lpEntry->name;
        if (lpcStr[0] == lpcName[0])
        {
            const char* lpcS = lpcStr  + 1;
            const char* lpcN = lpcName + 1;
            int liDiff;
            do
            {
                liDiff = (unsigned char)*lpcS - (unsigned char)*lpcN;
                if (*lpcS == '\0')
                    break;
                ++lpcS;
                ++lpcN;
            }
            while (liDiff == 0);

            if (liDiff == 0)
                return lpEntry;
        }
        return 0;
    }

    // liIndex < 0 : either an empty slot or a duplicate list.
    if (liIndex >= -TOTAL_KEYWORDS)   // (-13..-1) -> no duplicate bucket here
        return 0;

    // Duplicate list: gperf encodes lookup[hash] == -(TOTAL_KEYWORDS + 1) - offset.
    const int liOffset = -(TOTAL_KEYWORDS + 1) - liIndex;     // == -14 - liIndex
    const int liBaseIndex = lookup[liOffset];                 // wordlist start index
    const int liCount     = lookup[liOffset + 1];             // number of duplicates

    const struct Entry* lpEntry = &wordlist[liBaseIndex];
    const struct Entry* lpEnd   = lpEntry + liCount;

    for (; lpEntry < lpEnd; ++lpEntry)
    {
        const char* lpcName = lpEntry->name;
        if (lpcStr[0] == lpcName[0])
        {
            const char* lpcS = lpcStr  + 1;
            const char* lpcN = lpcName + 1;
            int liDiff;
            do
            {
                liDiff = (unsigned char)*lpcS - (unsigned char)*lpcN;
                if (*lpcS == '\0')
                    break;
                ++lpcS;
                ++lpcN;
            }
            while (liDiff == 0);

            if (liDiff == 0)
                return lpEntry;
        }
    }
    return 0;
}

// --- gperf hash (asso_values[] @ 0x82F72DF8) -----------------------------
// hash = asso_values[(unsigned char)str[len-1]] + asso_values[(unsigned char)str[0]] + len
// (gperf reduced the keyword set to just first-char + last-char + length.)
unsigned int
StringMembersIndex::hash(const char* lpcStr, unsigned int luLen)
{
    return asso_values[(unsigned char)lpcStr[luLen - 1]]
         + asso_values[(unsigned char)lpcStr[0]]
         + luLen;
}
