#include <cstddef>
#include "SDKs/EATech/Apt/AptObjectIndex.h"

// ===========================================================================
// gperf static data tables -- EXTRACTED from the decrypted X360 ARTIST.XEX
// .rdata (file_off = 0x3000 + vaddr - 0x82000000, big-endian):
//     asso_values[256] @ byte_82F72EF8
//     lookup[38]       @ byte_82F79D50   (signed; direct slot >=0, dup escape <0)
//     wordlist[38]     @ off_82F79C20    (Entry{ const char* name; int data })
// Every one of the 38 global-object names was verified to resolve back to its
// own slot through the reconstructed in_word_set below (the dup-keyed _levelNN
// runs route correctly through the {base,count} table).
//
// As in the sibling KeyMembersIndex, the {base,count} descriptor stores the run
// COUNT as a NEGATED value -- the X360 forms the run-end pointer as
// `end = start - count*8` (asm `subf r6,r10,r11`) -- so the dup walk negates
// lookup[liDupIndex + 1] to recover the positive run length. The base is in
// Entry-stride units measured from the lookup base, which equals
// `TOTAL_KEYWORDS + base` in wordlist-index terms (wordlist immediately precedes
// lookup in .rdata).
// ===========================================================================

// asso_values[256]: per-byte hash contribution. Default 38 (== TOTAL_KEYWORDS,
// the gperf "not a key byte" sentinel that pushes any non-keyword character past
// the hash gate). Only the bytes at the key positions of the keyword set carry
// real weights.
const unsigned char ObjectIndex::asso_values[256] =
{
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
     5,  0, 15, 20, 25, 30, 23,  8, 13, 18, 38, 38, 38, 38, 38, 38,
    38,  0, 38, 38, 38, 38, 38, 38, 38, 38, 38,  0, 38,  5, 38, 38,
    38, 38, 38,  0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,  0,
    38,  0, 10, 38, 38,  0, 38, 10, 38, 38, 38, 38,  0, 38,  0, 38,
    38, 38,  5, 25,  9, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38
};

// lookup[38]: hash (0..MAX_HASH_VALUE) -> wordlist slot (>=0), or a negative
// duplicate-table escape. The dup descriptors are overlaid on the same array
// (slots 28/29 and 33/34 double as {base,count} pairs for the _levelNN runs).
const signed char ObjectIndex::lookup[38] =
{
      -1,   -1,   -1,    0,   -1,    1,    2,    3,  -72,   14,   15,   16,
      17,   18,   19,   20,   21,   22,   -1,   23,   24,   -1,   25,  -67,
      -1,   31,   32,   33,  -12,   -5,   34,   -1,   35,  -34,  -10,   36,
      -1,   37
};

// wordlist[38]: the global-object / member keywords and their per-object ids
// (the X360 .data payload consumed by AptValue::findChild). Entry stride is
// 8 bytes (const char*; int), matching the X360 &wordlist[i] arithmetic.
const ObjectIndex::Entry ObjectIndex::wordlist[38] =
{
    /*  0 */ { "Key",            4  },
    /*  1 */ { "Stage",          37 },
    /*  2 */ { "String",         36 },
    /*  3 */ { "_level1",        7  },
    /*  4 */ { "_level10",       21 },
    /*  5 */ { "_level19",       30 },
    /*  6 */ { "_level18",       29 },
    /*  7 */ { "_level17",       28 },
    /*  8 */ { "_level16",       27 },
    /*  9 */ { "_level15",       26 },
    /* 10 */ { "_level14",       25 },
    /* 11 */ { "_level13",       24 },
    /* 12 */ { "_level12",       23 },
    /* 13 */ { "_level11",       22 },
    /* 14 */ { "Math",           5  },
    /* 15 */ { "Mouse",          20 },
    /* 16 */ { "extern",         17 },
    /* 17 */ { "_level0",        6  },
    /* 18 */ { "this",           2  },
    /* 19 */ { "_root",          3  },
    /* 20 */ { "_level7",        13 },
    /* 21 */ { "_parent",        16 },
    /* 22 */ { "_global",        19 },
    /* 23 */ { "AlternateInput", 38 },
    /* 24 */ { "_level8",        14 },
    /* 25 */ { "_level2",        8  },
    /* 26 */ { "_level20",       31 },
    /* 27 */ { "_level24",       35 },
    /* 28 */ { "_level23",       34 },
    /* 29 */ { "_level22",       33 },
    /* 30 */ { "_level21",       32 },
    /* 31 */ { "_level9",        15 },
    /* 32 */ { "_target",        1  },
    /* 33 */ { "_level3",        9  },
    /* 34 */ { "_level6",        12 },
    /* 35 */ { "_level4",        10 },
    /* 36 */ { "super",          18 },
    /* 37 */ { "_level5",        11 }
};

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
    //   base  = lookup[liDupIndex]       (sign-extended)
    //   count = -lookup[liDupIndex + 1]  (stored NEGATED: end = start - count*8,
    //                                     asm `subf r6,r10,r11`)
    // The base is in Entry-stride units measured from the LOOKUP base, not the
    // wordlist base; since wordlist immediately precedes lookup in .rdata, the
    // effective wordlist start index is TOTAL_KEYWORDS + base (matching the X360
    // `r9 = lookup; start = r9 + base*8`).
    const int    liBase  = lookup[ liDupIndex ];                    // v12 (sign-extended)
    const int    liCount = -(int)lookup[ liDupIndex + 1 ];          // count (stored negated)
    const Entry* lpEntry = &wordlist[ (int)TOTAL_KEYWORDS + liBase ];   // v13 start
    const Entry* lpEnd   = lpEntry + liCount;                       // v14 = v13 + count
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
