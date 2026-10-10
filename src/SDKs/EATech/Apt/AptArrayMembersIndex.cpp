#include "SDKs/EATech/Apt/AptArrayMembersIndex.h"

// gperf perfect-hash lookup for ActionScript Array member names, reconstructed
// from the console's ArrayMembersIndex::in_word_set. Control flow is the
// console's exactly:
//
//   length gate : (len - 3) > 5               =>  reject   (3 <= len <= 8)
//   hash        : asso_values[str[len - 1]] + asso_values[str[0]] + len
//   hash gate   : hash > MAX_HASH_VALUE (27)  =>  reject
//   wordlist[hash] : first-char compare, then a strcmp of the tails
//
// Both tables are dumped from the console image .rdata. The asso_values table
// is a file-scope global (ArrayMembersIndex_asso_values), the wordlist a static
// local of in_word_set, the same placement gperf's C++ output uses.

// asso_values[256]: per-byte hash weight. 28 (MAX_HASH_VALUE + 1) is gperf's
// "not a key byte" value that pushes any foreign character past the hash gate;
// only the key-position bytes of the keyword set carry real weights:
//   'c'=5 'e'=15 'g'=0 'h'=10 'j'=5 'l'=0 'n'=0 'p'=0 'r'=5 's'=0 't'=0 'u'=0
static const unsigned char ArrayMembersIndex_asso_values[256] =
{
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28,  5, 28, 15, 28,  0, 10, 28,  5, 28,  0, 28,  0, 28,
     0, 28,  5,  0,  0,  0, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28
};

const ArrayMembersIndex::Entry*
ArrayMembersIndex::in_word_set(const char* lpcStr, unsigned int luLen)
{
    // wordlist[28], indexed directly by hash value. Keyword slots hold the
    // Array member name and its member id; empty slots hold {"", 0}, whose
    // leading NUL can never match a length-gated (len >= 3) input.
    static const Entry wordlist[WORDLIST_SLOTS] =
    {
        /* 0  */ { "",         0  },
        /* 1  */ { "",         0  },
        /* 2  */ { "",         0  },
        /* 3  */ { "pop",      4  },
        /* 4  */ { "sort",     9  },
        /* 5  */ { "shift",    6  },
        /* 6  */ { "sortOn",   12 },
        /* 7  */ { "unshift",  7  },
        /* 8  */ { "toString", 13 },
        /* 9  */ { "join",     3  },
        /* 10 */ { "",         0  },
        /* 11 */ { "concat",   2  },
        /* 12 */ { "",         0  },
        /* 13 */ { "",         0  },
        /* 14 */ { "push",     5  },
        /* 15 */ { "",         0  },
        /* 16 */ { "length",   1  },
        /* 17 */ { "",         0  },
        /* 18 */ { "",         0  },
        /* 19 */ { "",         0  },
        /* 20 */ { "slice",    11 },
        /* 21 */ { "splice",   10 },
        /* 22 */ { "",         0  },
        /* 23 */ { "",         0  },
        /* 24 */ { "",         0  },
        /* 25 */ { "",         0  },
        /* 26 */ { "",         0  },
        /* 27 */ { "reverse",  8  }
    };

    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if ((luLen - MIN_WORD_LENGTH) > (MAX_WORD_LENGTH - MIN_WORD_LENGTH))   // (len - 3) > 5
    {
        return 0;
    }

    // Hash value: the weights of the last and the first byte, plus the length.
    const char lcFirst = *lpcStr;
    const unsigned int luHashVal =
        ArrayMembersIndex_asso_values[(unsigned char)lpcStr[luLen - 1]] +
        ArrayMembersIndex_asso_values[(unsigned char)lcFirst] +
        luLen;
    if (luHashVal > MAX_HASH_VALUE)
    {
        return 0;
    }

    // Direct wordlist index (no duplicate table in this gperf variant).
    const Entry* lpEntry = &wordlist[luHashVal];
    if (lcFirst != *lpEntry->mpcName)                     // first-char fast reject
    {
        return 0;
    }

    // strcmp of the two tails.
    const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;
    const unsigned char* lpcCmp = (const unsigned char*)lpcStr + 1;
    int liDiff;
    do
    {
        liDiff = *lpcCmp - *lpcKey;
        if (!*lpcCmp)
        {
            break;
        }
        ++lpcCmp;
        ++lpcKey;
    }
    while (!liDiff);

    if (liDiff)
    {
        return 0;
    }
    return lpEntry;
}
