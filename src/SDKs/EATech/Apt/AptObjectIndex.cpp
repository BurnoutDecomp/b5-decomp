#include <cstddef>
#include "SDKs/EATech/Apt/AptObjectIndex.h"

// gperf perfect-hash lookup with duplicate-key resolution.
// Faithful reconstruction of the X360 pseudocode at 0x82AD7C80. The control
// flow (length gate, 3-position hash, direct-vs-duplicate split, per-keyword
// strcmp) is preserved exactly; only register artifacts were lowered to named
// locals and the opaque .rdata tables to the static members declared above.
const ObjectIndex::Entry* ObjectIndex::in_word_set( const char* lpcStr, unsigned int luLen )
{
    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if ( ( luLen - MIN_WORD_LENGTH ) > ( MAX_WORD_LENGTH - MIN_WORD_LENGTH ) )   // (a2 - 3) > 0xB
    {
        return 0;
    }

    // Hash value: asso_values of the (length-gated) key positions 6, 4, 0.
    unsigned int luHashVal = luLen;
    if ( luLen > 4 )
    {
        if ( luLen > 6 )
        {
            luHashVal += asso_values[ (unsigned char)lpcStr[6] ];
        }
        luHashVal += asso_values[ (unsigned char)lpcStr[4] ];
    }

    const char lcFirst = *lpcStr;                                    // v3 = str[0]
    luHashVal += asso_values[ (unsigned char)lcFirst ];              // v4
    if ( luHashVal > MAX_HASH_VALUE )                                // v4 > 0x25
    {
        return 0;
    }

    const int liSlot = lookup[ luHashVal ];                         // v5 (signed)

    if ( liSlot >= 0 )
    {
        // Direct slot: a single keyword hashes here. Compare and return.
        const Entry* lpEntry = &wordlist[ liSlot ];                 // &off_82F79C20 + 2*v5
        if ( lcFirst == *lpEntry->mpcName )                         // first-char fast reject
        {
            const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;  // v7
            const char*          lpcCmp = lpcStr + 1;               // v8
            int liDiff;
            do
            {
                liDiff = (unsigned char)*lpcCmp - *lpcKey;
                if ( !*lpcCmp )
                {
                    break;
                }
                ++lpcCmp;
                ++lpcKey;
            }
            while ( !liDiff );
            if ( !liDiff )
            {
                return lpEntry;
            }
        }
        return 0;
    }

    // Duplicate slot: liSlot < 0 routes into the secondary (range) table that
    // gperf overlays on lookup[]. Entries -1..-TOTAL_KEYWORDS are not duplicate
    // ranges, so reject them; the true range descriptors start past that.
    if ( liSlot >= -(int)TOTAL_KEYWORDS )                           // v5 >= -38
    {
        return 0;
    }

    const int liDupIndex = -( (int)TOTAL_KEYWORDS + 1 ) - liSlot;   // v11 = -39 - v5
    // The X360 reads a {base,count} pair from the overlaid lookup[] table and
    // walks a contiguous run of wordlist entries (stride = 2 dwords == 8 bytes).
    const Entry* lpEntry = &wordlist[ lookup[ liDupIndex ] ];       // v13 start
    const Entry* lpEnd   = lpEntry + lookup[ liDupIndex + 1 ];      // v14 = v13 + count
    if ( lpEntry >= lpEnd )                                         // empty/invalid run
    {
        return 0;
    }

    for ( ; ; )
    {
        if ( lcFirst == *lpEntry->mpcName )
        {
            const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;  // v15
            const char*          lpcCmp = lpcStr + 1;               // v16
            int liDiff;
            do
            {
                liDiff = (unsigned char)*lpcCmp - *lpcKey;
                if ( !*lpcCmp )
                {
                    break;
                }
                ++lpcCmp;
                ++lpcKey;
            }
            while ( !liDiff );
            if ( !liDiff )
            {
                return lpEntry;     // match
            }
        }
        ++lpEntry;                  // v13 += 8 bytes == one Entry
        if ( lpEntry >= lpEnd )
        {
            return 0;
        }
    }
}
