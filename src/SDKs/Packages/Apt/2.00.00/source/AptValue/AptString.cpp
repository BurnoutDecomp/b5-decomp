#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptString.h"

#include <cstdint>   // uintptr_t (the wordlist payload is the X360 dword member id)

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
    // {base,count} descriptor overlaid on lookup[]. The X360 computes the run
    // start as `lookup_base + base*8` (asm sets r9 = &lookup, then start = r9 +
    // base*8); since wordlist immediately precedes lookup in .rdata this equals
    // wordlist[TOTAL_KEYWORDS + base]. The COUNT is stored NEGATED -- the run-end
    // pointer is `start - count*8` (asm `subf r6,r10,r11`) -- so the real run
    // length is -lookup[liOffset + 1].
    const int liBase  = lookup[liOffset];                     // base (sign-extended)
    const int liCount = -(int)lookup[liOffset + 1];           // count (stored negated)

    const struct Entry* lpEntry = &wordlist[TOTAL_KEYWORDS + liBase];
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

// ===========================================================================
// gperf static data tables -- EXTRACTED from the decrypted X360 ARTIST.XEX
// .rdata (file_off = 0x3000 + vaddr - 0x82000000, big-endian):
//     asso_values[256] @ byte_82F72DF8   (per-byte hash contribution; default
//                                         23 == MAX_HASH_VALUE+1 sentinel)
//     lookup[23]       @ byte_82F79C08   (signed; direct slot >=0, empty/dup <0;
//                                         slots 12/13 and 18/19 double as the
//                                         {base,count} dup descriptors)
//     wordlist[13]     @ off_82F79BA0    (the 13 AptString method keywords; the
//                                         X360 payload is a 4-byte member id,
//                                         carried here in the void* mpPayload)
// All 13 keywords were verified to resolve back to their own slot through the
// reconstructed in_word_set above (the dup-keyed pairs route correctly).
// ===========================================================================

const unsigned char StringMembersIndex::asso_values[256] =
{
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23,  0, 23, 10,  0,  0, 10,  0, 23, 23,  0, 23, 23, 23,
    23, 23, 14,  0,  0, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23
};

const signed char StringMembersIndex::lookup[23] =
{
      -1,   -1,   -1,   -1,   -1,    0,  -26,    3,   -1,    4,    5,    6,
     -12,   -2,   -1,    7,    8,   -1,   -3,   -2,    9,  -32,   12
};

const struct StringMembersIndex::Entry StringMembersIndex::wordlist[13] =
{
    /*  0 */ { "split",         reinterpret_cast<void*>(static_cast<uintptr_t>(9)) },
    /*  1 */ { "charAt",        reinterpret_cast<void*>(static_cast<uintptr_t>(2)) },
    /*  2 */ { "concat",        reinterpret_cast<void*>(static_cast<uintptr_t>(4)) },
    /*  3 */ { "indexOf",       reinterpret_cast<void*>(static_cast<uintptr_t>(6)) },
    /*  4 */ { "substring",     reinterpret_cast<void*>(static_cast<uintptr_t>(11)) },
    /*  5 */ { "charCodeAt",    reinterpret_cast<void*>(static_cast<uintptr_t>(3)) },
    /*  6 */ { "lastIndexOf",   reinterpret_cast<void*>(static_cast<uintptr_t>(7)) },
    /*  7 */ { "slice",         reinterpret_cast<void*>(static_cast<uintptr_t>(8)) },
    /*  8 */ { "length",        reinterpret_cast<void*>(static_cast<uintptr_t>(1)) },
    /*  9 */ { "substr",        reinterpret_cast<void*>(static_cast<uintptr_t>(10)) },
    /* 10 */ { "toLowerCase",   reinterpret_cast<void*>(static_cast<uintptr_t>(12)) },
    /* 11 */ { "toUpperCase",   reinterpret_cast<void*>(static_cast<uintptr_t>(13)) },
    /* 12 */ { "fromCharCode",  reinterpret_cast<void*>(static_cast<uintptr_t>(5)) },
};
