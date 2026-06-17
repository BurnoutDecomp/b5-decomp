#include <cstddef>
#include "SDKs/EATech/Apt/AptTextMembersIndex.h"

// gperf perfect-hash lookup for ActionScript TextField member names.
// Faithful reconstruction of the X360 pseudocode at 0x82AD6EB8. The control
// flow (length gate, length-gated 3-position hash, direct wordlist index,
// per-keyword strcmp) is preserved exactly; only register artifacts were
// lowered to named locals. Like the sibling TextFormatMembersIndex this gperf
// output has no key collisions, so the hash value indexes the wordlist
// directly -- there is no negative-index duplicate-table escape path.
//
//   length gate : (len - 4) > 0xD          =>  reject   (4 <= len <= 17)
//   hash        : len
//                 + asso_values[str[8]]      (only when len > 8)
//                 + asso_values[str[1]]      (str[1]; len >= 4 guarantees range)
//                 + asso_values[str[0]]
//   hash gate   : hash > 0x1D              =>  reject
//   wordlist[hash] : single keyword slot   =>  first-char + strcmp, return Entry*
//
// The two backing tables below were extracted from the X360 .rdata via the IDA
// database (tools/dump_text_tables.py):
//   asso_values[256] @ byte_82F72CB0
//   wordlist[30]     @ off_82F79AB0   (Entry{ const char* name; int data })
// Each keyword's hash was verified to land in exactly its extracted slot.

// asso_values[256]: per-byte hash contribution. Default 30 (== MAX_HASH_VALUE+1,
// the gperf "not a key byte" sentinel that pushes any non-keyword character
// past the hash gate). Only the bytes appearing at the key positions of the
// keyword set carry real weights (the 17 non-default entries, by ASCII index):
//   '_'(95)=5
//   'a'=0  'b'=0  'c'=15 'e'=0  'h'=0  'l'=5  'm'=0  'n'=5  'o'=0
//   'r'=15 's'=0  't'=0  'u'=10 'v'=5  'w'=15 'y'=25
const unsigned char TextMembersIndex::asso_values[256] =
{
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,  5,
    30,  0,  0, 15, 30,  0, 30, 30,  0, 30, 30, 30,  5,  0,  5,  0,
    30, 30, 15,  0,  0, 10,  5, 15, 30, 25, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
};

// wordlist[30]: indexed directly by hash value. Real keyword slots hold the
// TextField member name and its member id (the X360 .data payload). Empty
// slots hold {"", 0}; their first char '\0' can never match a length-gated
// (len >= 4) input string, exactly as the gperf-emitted empty entries behave.
const TextMembersIndex::Entry TextMembersIndex::wordlist[WORDLIST_SLOTS] =
{
    /* 0  */ { "",                  0  },
    /* 1  */ { "",                  0  },
    /* 2  */ { "",                  0  },
    /* 3  */ { "",                  0  },
    /* 4  */ { "text",              12 },
    /* 5  */ { "",                  0  },
    /* 6  */ { "border",            4  },
    /* 7  */ { "hscroll",           6  },
    /* 8  */ { "maxChars",          8  },
    /* 9  */ { "textWidth",         15 },
    /* 10 */ { "textHeight",        14 },
    /* 11 */ { "length",            7  },
    /* 12 */ { "_height",           19 },
    /* 13 */ { "variable",          17 },
    /* 14 */ { "maxscroll",         9  },
    /* 15 */ { "background",        2  },
    /* 16 */ { "borderColor",       5  },
    /* 17 */ { "mouseWheelEnabled", 21 },
    /* 18 */ { "autoSize",          1  },
    /* 19 */ { "multiline",         10 },
    /* 20 */ { "backgroundColor",   3  },
    /* 21 */ { "scroll",            11 },
    /* 22 */ { "",                  0  },
    /* 23 */ { "wordWrap",          18 },
    /* 24 */ { "textColor",         13 },
    /* 25 */ { "",                  0  },
    /* 26 */ { "_width",            20 },
    /* 27 */ { "",                  0  },
    /* 28 */ { "",                  0  },
    /* 29 */ { "type",              16 }
};

const TextMembersIndex::Entry*
TextMembersIndex::in_word_set( const char* lpcStr, unsigned int luLen )
{
    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if ( ( luLen - MIN_WORD_LENGTH ) > ( MAX_WORD_LENGTH - MIN_WORD_LENGTH ) )   // (len - 4) > 0xD
    {
        return 0;
    }

    // Hash value: len plus asso_values of the (length-gated) key positions
    // 8, 1, 0. The X360 branches on len==1 / len<=8 to skip the str[1] / str[8]
    // reads; the length gate already guarantees len >= 4, so the str[1] term
    // always applies and only the str[8] term stays length-gated.
    unsigned int luHashVal = luLen;                                  // v2 = a2
    if ( luLen > 8 )                                                 // a2 > 8
    {
        luHashVal += asso_values[ (unsigned char)lpcStr[8] ];        // byte_82F72CB0[a1[8]] + a2
    }
    luHashVal += asso_values[ (unsigned char)lpcStr[1] ];            // byte_82F72CB0[a1[1]]

    const char lcFirst = *lpcStr;                                    // v4 = *a1
    luHashVal += asso_values[ (unsigned char)lcFirst ];              // byte_82F72CB0[*a1]
    if ( luHashVal > MAX_HASH_VALUE )                                // v5 > 0x1D
    {
        return 0;
    }

    // Direct wordlist index (no duplicate table in this gperf variant).
    const Entry* lpEntry = &wordlist[ luHashVal ];                   // &off_82F79AB0 + 2*v5
    if ( lcFirst != *lpEntry->mpcName )                              // v4 != **result
    {
        return 0;
    }

    const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;   // v7 = *result + 1
    const char*          lpcCmp = lpcStr + 1;                        // v8 = a1 + 1
    int liDiff;
    do
    {
        liDiff = (unsigned char)*lpcCmp - *lpcKey;                   // v9 = *v8 - *v7
        if ( !*lpcCmp )                                              // if ( !*v8 ) break
        {
            break;
        }
        ++lpcCmp;
        ++lpcKey;
    }
    while ( !liDiff );

    if ( liDiff )                                                    // if ( v9 ) return 0
    {
        return 0;
    }
    return lpEntry;                                                  // return result
}
