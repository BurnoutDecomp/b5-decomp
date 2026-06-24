#include <cstddef>
#include "SDKs/EATech/Apt/AptKeyMembersIndex.h"

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
    //   base  = lookup[liDupIndex]      (byte_82F78FB4[v12])
    //   count = lookup[liDupIndex + 1]  (byte_82F78FB5[v12] == lookup[v12 + 1])
    // and walks a contiguous run of `count` wordlist entries (stride 8 bytes). It
    // also walks the parallel lengths[] entries so each candidate's expected length
    // can be checked before the byte compare.
    const int          liBase    = lookup[ liDupIndex ];             // v14
    const int          liCount   = lookup[ liDupIndex + 1 ];         // v13
    const Entry*       lpEntry   = &wordlist[ DUP_CUTOFF + liBase ]; // v16 = &byte_82F78F98[8*v14] = &wordlist[27 + v14]
    const Entry*       lpEnd     = lpEntry + liCount;                // v17 = v16 - 8*count region end
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
