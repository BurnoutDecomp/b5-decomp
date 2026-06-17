#include <cstddef>
#include "SDKs/EATech/Apt/AptTextFormatMembersIndex.h"

// gperf perfect-hash lookup for ActionScript TextFormat member names.
// Faithful reconstruction of the X360 pseudocode at 0x82AD68A0. The control
// flow (length gate, 3-position hash, direct wordlist index, per-keyword
// strcmp) is preserved exactly; only register artifacts were lowered to named
// locals. Unlike the ObjectIndex / StringMembersIndex recognizers this gperf
// output has no key collisions, so the hash value indexes the wordlist
// directly -- there is no negative-index duplicate-table escape path.
//
//   length gate : (len - 3) > 8            =>  reject   (3 <= len <= 11)
//   hash        : asso_values[str[0]]
//                 + asso_values[str[5]]     (only when len > 5)
//                 + asso_values[str[7]]     (only when len > 7)
//                 + len
//   hash gate   : hash > 0x24              =>  reject
//   wordlist[hash] : single keyword slot   =>  first-char + strcmp, return Entry*
//
// The two backing tables below were extracted from the X360 .rdata via the IDA
// database (tools/dump_textformat_tables.py):
//   asso_values[256] @ byte_82F72AB0
//   wordlist[37]     @ off_82F798A8   (Entry{ const char* name; int data })

// asso_values[256]: per-byte hash contribution. Default 37 (== MAX_HASH_VALUE+1,
// the gperf "not a key byte" sentinel that pushes any non-keyword character
// past the hash gate). Only the bytes of the keyword set carry real weights
// (the 16 non-default entries, by ASCII index):
//   'I'(73)=10  'M'(77)=0
//   'a'=5 'b'=15 'c'=10 'd'=0 'f'=10 'g'=5 'i'=10 'l'=0
//   'n'=0 'o'=0 'r'=0 's'=0 't'=0 'u'=0
const unsigned char TextFormatMembersIndex::asso_values[256] =
{
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 10, 37, 37, 37,  0, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37,  5, 15, 10,  0, 37, 10,  5, 37, 10, 37, 37,  0, 37,  0,  0,
    37, 37,  0,  0,  0,  0, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37
};

// wordlist[37]: indexed directly by hash value. Real keyword slots hold the
// TextFormat member name and its member id (the X360 .data payload). Empty
// slots hold {"", 0}; their first char '\0' can never match a length-gated
// (len >= 3) input string, exactly as the gperf-emitted empty entries behave.
const TextFormatMembersIndex::Entry TextFormatMembersIndex::wordlist[WORDLIST_SLOTS] =
{
    /* 0  */ { "",            0  },
    /* 1  */ { "",            0  },
    /* 2  */ { "",            0  },
    /* 3  */ { "url",         16 },
    /* 4  */ { "size",        14 },
    /* 5  */ { "",            0  },
    /* 6  */ { "target",      13 },
    /* 7  */ { "leading",     9  },
    /* 8  */ { "tabStops",    12 },
    /* 9  */ { "underline",   15 },
    /* 10 */ { "align",       1  },
    /* 11 */ { "rightMargin", 11 },
    /* 12 */ { "",            0  },
    /* 13 */ { "",            0  },
    /* 14 */ { "font",        6  },
    /* 15 */ { "color",       5  },
    /* 16 */ { "indent",      7  },
    /* 17 */ { "",            0  },
    /* 18 */ { "",            0  },
    /* 19 */ { "bold",        3  },
    /* 20 */ { "leftMargin",  10 },
    /* 21 */ { "bullet",      4  },
    /* 22 */ { "",            0  },
    /* 23 */ { "",            0  },
    /* 24 */ { "",            0  },
    /* 25 */ { "",            0  },
    /* 26 */ { "italic",      8  },
    /* 27 */ { "",            0  },
    /* 28 */ { "",            0  },
    /* 29 */ { "",            0  },
    /* 30 */ { "",            0  },
    /* 31 */ { "",            0  },
    /* 32 */ { "",            0  },
    /* 33 */ { "",            0  },
    /* 34 */ { "",            0  },
    /* 35 */ { "",            0  },
    /* 36 */ { "blockIndent", 2  }
};

const TextFormatMembersIndex::Entry*
TextFormatMembersIndex::in_word_set( const char* lpcStr, unsigned int luLen )
{
    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if ( ( luLen - MIN_WORD_LENGTH ) > ( MAX_WORD_LENGTH - MIN_WORD_LENGTH ) )   // (len - 3) > 8
    {
        return 0;
    }

    // Hash value: len plus asso_values of the (length-gated) key positions 7, 5, 0.
    unsigned int luHashVal = luLen;
    if ( luLen > 5 )
    {
        if ( luLen > 7 )
        {
            luHashVal += asso_values[ (unsigned char)lpcStr[7] ];
        }
        luHashVal += asso_values[ (unsigned char)lpcStr[5] ];
    }

    const char lcFirst = *lpcStr;                                    // v4 = str[0]
    luHashVal += asso_values[ (unsigned char)lcFirst ];              // v5
    if ( luHashVal > MAX_HASH_VALUE )                                // v5 > 0x24
    {
        return 0;
    }

    // Direct wordlist index (no duplicate table in this gperf variant).
    const Entry* lpEntry = &wordlist[ luHashVal ];                   // &off_82F798A8 + 2*v5
    if ( lcFirst != *lpEntry->mpcName )                              // first-char fast reject
    {
        return 0;
    }

    const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;   // v7
    const char*          lpcCmp = lpcStr + 1;                        // v8
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

    if ( liDiff )
    {
        return 0;
    }
    return lpEntry;
}
