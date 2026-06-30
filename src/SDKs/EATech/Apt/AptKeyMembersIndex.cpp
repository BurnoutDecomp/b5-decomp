#include <cstddef>
#include "SDKs/EATech/Apt/AptKeyMembersIndex.h"

// ===========================================================================
// gperf static data tables -- EXTRACTED from the decrypted X360 ARTIST.XEX
// .rdata (file_off = 0x3000 + vaddr - 0x82000000, big-endian):
//     asso_values[256] @ byte_82F723B0
//     lookup[35]       @ byte_82F78FB4   (signed; direct slot >=0, dup escape <0)
//     lengths[28]      @ byte_82F78F98   (per-slot expected keyword length)
//     wordlist[27]     @ off_82F78EC0    (Entry{ const char* name; int data })
// Every one of the 27 Key-object member names was verified to resolve back to
// its own slot through the reconstructed in_word_set below (and the dup-keyed
// pairs DOWN/HOME and PGDN/PGUP route correctly through the {base,count} table).
//
// The {base,count} descriptor that the negative lookup[] slots point at stores
// the run COUNT as a NEGATED value (the X360 computes the run-end pointer as
// `start - count*8`, i.e. count is subtracted -- see the asm `subf r6,r7,r11`).
// The dup walk below therefore negates lookup[liDupIndex + 1] to recover the
// real positive run length.
// ===========================================================================

// asso_values[256]: per-byte hash contribution. Default 35 (== TOTAL_KEYWORDS+...
// the gperf "not a key byte" sentinel: a non-key byte at a key position pushes
// the hash past MAX_HASH_VALUE). Only the bytes at the key positions of the
// keyword set carry real weights.
const unsigned char KeyMembersIndex::asso_values[256] =
{
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35,  0, 35, 25, 35,  5, 35, 15, 10,  5, 35, 20, 35, 35, 15,  0,
     0, 35, 35,  5,  0, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35,  5, 35,  0,  5,  0, 35, 20, 35,  0, 35, 35, 35, 35,  0,  0,
    35, 35,  0,  0, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35
};

// lookup[35]: hash (0..MAX_HASH_VALUE) -> wordlist slot (>=0), or a negative
// duplicate-table escape. The dup descriptors are overlaid on the same array
// (slots 26/27 and 30/31 double as {base,count} pairs for the dup runs).
const signed char KeyMembersIndex::lookup[35] =
{
      -1,   -1,    0,    1,  -58,    4,    5,    6,    7,    8,    9,   -1,
      10,   11,   12,   13,   14,   15,   16,  -54,   19,   20,   -1,   21,
      22,   23,  -10,   -2,   24,   25,  -25,   -2,   -1,   -1,   26
};

// lengths[28]: per-slot expected keyword length (slot order matches wordlist;
// the trailing 0 is the gperf sentinel). The dup path's interior pointer
// unk_82F78FB3 indexes into this same array.
const unsigned char KeyMembersIndex::lengths[28] =
{
    2, 3, 4, 4, 5, 6, 7, 8, 4, 5, 7, 13, 14, 5, 11, 7, 3, 4, 4, 5, 6, 18, 9, 20, 8, 9, 9, 0
};

// wordlist[27]: the Key-object member keywords and their per-member ids (the
// X360 .data payload consumed by AptKey::objectMemberLookup). Entry stride is
// 8 bytes (const char*; int), matching the X360 &wordlist[i] arithmetic.
const KeyMembersIndex::Entry KeyMembersIndex::wordlist[27] =
{
    /*  0 */ { "UP",                  18  },
    /*  1 */ { "TAB",                 17  },
    /*  2 */ { "DOWN",                5   },
    /*  3 */ { "HOME",                9   },
    /*  4 */ { "SPACE",               16  },
    /*  5 */ { "isDown",              100 },
    /*  6 */ { "CONTROL",             3   },
    /*  7 */ { "getAscii",            107 },
    /*  8 */ { "LEFT",                11  },
    /*  9 */ { "RIGHT",               14  },
    /* 10 */ { "getCode",             102 },
    /* 11 */ { "getController",       103 },
    /* 12 */ { "removeListener",      105 },
    /* 13 */ { "SHIFT",               15  },
    /* 14 */ { "addListener",         104 },
    /* 15 */ { "ESCAPED",             8   },
    /* 16 */ { "END",                 6   },
    /* 17 */ { "PGDN",                12  },
    /* 18 */ { "PGUP",                13  },
    /* 19 */ { "ENTER",               7   },
    /* 20 */ { "INSERT",              10  },
    /* 21 */ { "getAnalogStickInfo",  106 },
    /* 22 */ { "DELETEKEY",           4   },
    /* 23 */ { "getAnalogTriggerInfo",108 },
    /* 24 */ { "CAPSLOCK",            2   },
    /* 25 */ { "isToggled",           101 },
    /* 26 */ { "BACKSPACE",           1   }
};

// gperf perfect-hash lookup with duplicate-key resolution.
// Faithful reconstruction of the X360 pseudocode at 0x82AD5FB8. The control flow
// (length gate, length-gated 3-position hash, direct-vs-duplicate split, per-slot
// expected-length check, length-bounded byte compare) is preserved exactly; only
// register artifacts were lowered to named locals and the opaque .rdata tables to
// the static members declared in the header.
//
//   length gate : (len - 2) > 0x12        =>  reject   (2 <= len <= 20)
//   hash        : len
//                 + asso_values[str[7]]      (only when len > 7)
//                 + asso_values[str[5]]      (only when len > 5)
//                 + asso_values[str[1]]
//   hash gate   : hash > 0x22              =>  reject
//   lookup[hash]: >= 0  -> direct slot (length-checked, then byte-compare)
//                 <  0  -> duplicate range (walk a {base,count} run, byte-compare)
//
// Unlike the sibling text recognizers, the per-keyword compare here is
// LENGTH-BOUNDED (it walks exactly `len-1` trailing bytes; the asm's loop
// terminates on `ptr == str + len`, not on a NUL), so it is reconstructed as a
// bounded byte loop rather than a NUL-terminated strcmp.
const KeyMembersIndex::Entry*
KeyMembersIndex::in_word_set( const char* lpcStr, unsigned int luLen )
{
    // Length gate: only words of length [MIN_WORD_LENGTH .. MAX_WORD_LENGTH].
    if ( ( luLen - MIN_WORD_LENGTH ) > ( MAX_WORD_LENGTH - MIN_WORD_LENGTH ) )   // (len - 2) > 0x12
    {
        return 0;
    }

    // Hash value: len plus asso_values of the (length-gated) key positions 7, 5, 1.
    unsigned int luHashVal = luLen;                                  // v2 = a2
    if ( luLen > 5 )
    {
        if ( luLen > 7 )
        {
            luHashVal += asso_values[ (unsigned char)lpcStr[7] ];    // byte_82F723B0[a1[7]] + a2
        }
        luHashVal += asso_values[ (unsigned char)lpcStr[5] ];        // byte_82F723B0[a1[5]]
    }
    luHashVal += asso_values[ (unsigned char)lpcStr[1] ];            // byte_82F723B0[a1[1]]
    if ( luHashVal > MAX_HASH_VALUE )                                // v4 > 0x22
    {
        return 0;
    }

    const int liSlot = lookup[ luHashVal ];                          // v5 = byte_82F78FB4[v4] (signed)

    if ( liSlot >= 0 )
    {
        // Direct slot: a single keyword hashes here. Expected-length check first,
        // then first-char + length-bounded byte compare.
        if ( luLen != lengths[ liSlot ] )                            // a2 != byte_82F78F98[v5]
        {
            return 0;
        }
        const Entry* lpEntry = &wordlist[ liSlot ];                  // &off_82F78EC0 + 2*v5 (8 bytes)
        if ( (unsigned char)*lpcStr != (unsigned char)*lpEntry->mpcName )   // *a1 != **result
        {
            return 0;
        }

        // Compare the trailing (len-1) bytes of str[1..] against key[1..].
        const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;  // v8 = *result + 1
        const unsigned char* lpcCmp = (const unsigned char*)lpcStr + 1;            // v9 = a1 + 1
        const unsigned char* lpcEnd = (const unsigned char*)lpcStr + luLen;        // &v3[a2 - 1]
        int liDiff = 0;                                              // r8 = 0
        while ( lpcCmp != lpcEnd )
        {
            liDiff = (int)*lpcCmp - (int)*lpcKey;                    // subf. r8 = *v9 - *v8
            if ( liDiff != 0 )
            {
                break;
            }
            ++lpcCmp;
            ++lpcKey;
        }
        if ( liDiff != 0 )
        {
            return 0;
        }
        return lpEntry;                                              // return result
    }

    // Duplicate slot: liSlot < 0 routes into the secondary (range) table that
    // gperf overlays on lookup[]. Slots in [-27 .. -1] are not duplicate ranges,
    // so reject them; the true range descriptors start past that cutoff.
    if ( liSlot >= -(int)DUP_CUTOFF )                                // v5 >= -27  =>  reject
    {
        return 0;
    }

    const int liDupIndex = -( (int)DUP_CUTOFF + 1 ) - liSlot;        // v12 = -28 - v5
    // The X360 reads a {base,count} pair from the overlaid lookup[] table:
    //   base  = lookup[liDupIndex]       (byte_82F78FB4[v12], sign-extended)
    //   count = -lookup[liDupIndex + 1]  (byte_82F78FB5[v12] == lookup[v12 + 1])
    // The run COUNT is stored NEGATED: the asm forms the run-end pointer as
    // `end = start - count*8` (subf r6,r7,r11), so the real positive run length
    // is -lookup[liDupIndex + 1]. The walk covers a contiguous run of `count`
    // wordlist entries (stride 8 bytes); it also walks the parallel lengths[]
    // entries so each candidate's expected length is checked before the byte
    // compare.
    const int          liBase    = lookup[ liDupIndex ];             // v14 (sign-extended)
    const int          liCount   = -(int)lookup[ liDupIndex + 1 ];   // v13 (stored negated)
    const Entry*       lpEntry   = &wordlist[ DUP_CUTOFF + liBase ]; // v16 = &wordlist[27 + v14]
    const Entry*       lpEnd     = lpEntry + liCount;                // v17 = v16 + count
    const unsigned char* lpcLen  = &lengths[ DUP_CUTOFF + liBase ];  // v15 = &unk_82F78FB3 + v14

    for ( ; lpEntry < lpEnd; ++lpEntry, ++lpcLen )
    {
        if ( luLen == *lpcLen && (unsigned char)*lpcStr == (unsigned char)*lpEntry->mpcName )
        {
            const unsigned char* lpcKey = (const unsigned char*)lpEntry->mpcName + 1;  // v18 = *v16 + 1
            const unsigned char* lpcCmp = (const unsigned char*)lpcStr + 1;            // v19 = a1 + 1
            const unsigned char* lpcEnd = (const unsigned char*)lpcStr + luLen;        // &a1[a2]
            int liDiff = 0;
            while ( lpcCmp != lpcEnd )
            {
                liDiff = (int)*lpcCmp - (int)*lpcKey;                // v21 = *v19 - v20
                if ( liDiff != 0 )
                {
                    break;
                }
                ++lpcCmp;
                ++lpcKey;
            }
            if ( liDiff == 0 )
            {
                return lpEntry;                                      // match
            }
        }
    }
    return 0;
}
