#pragma once

#include "types.hpp"

// ----------------------------------------------------------------------------
// SpriteMembersIndex
//
// gperf-generated perfect-hash recognizer for the ActionScript member / method
// names exposed by EA's Apt SDK "Sprite" value object (the sibling of
// StringMembersIndex / AptString). The single public entry point
// SpriteMembersIndex::in_word_set() is the classic gperf %struct-type lookup;
// AptCIH::objectMemberLookup / AptCIH::objectMemberSet (and
// AptNativeHash::UpdateObjectMethods) call it to resolve a member name to its
// descriptor entry.
//
// This is GENERATED code: the gperf identifiers (SpriteMembersIndex,
// in_word_set, hash, wordlist, asso_values, Entry) are kept verbatim per the
// naming convention's "generated/external API" exception -- they are NOT
// renamed to the Brn/Cgs PascalCase scheme. The class is just the namespace
// gperf emits to scope the recognizer (gperf `%define class-name`).
//
// Shape derived from the X360 pseudocode at 0x82AD6800 (in_word_set) and
// 0x82AD6798 (hash); the DecFIGS DWARF and the Feb-2007 partial source have no source for
// this TU, so the X360 binary is authoritative for the algorithm.
//
// This recognizer is the SIMPLE gperf form: a single, direct hash-indexed
// wordlist (NO `lookup[]` indirection table, NO duplicate-key resolution --
// contrast StringMembersIndex/ObjectIndex which have both). The X360 indexes the
// wordlist DIRECTLY by hash value:  entry = &wordlist[hash].
//
// gperf parameters recovered from the binary:
//     MIN_WORD_LENGTH = 2          ( (len - 2) > 0x12  =>  reject  =>  2..20 )
//     MAX_WORD_LENGTH = 20
//     MAX_HASH_VALUE  = 0x119      ( hash > 0x119 (281) => reject )
//     key positions   = str[0], str[1], str[5], str[7]   (length-gated)
//
// hash arithmetic (0x82AD6798):
//     h  = len
//     h += asso_values[str[0]]                 (always)
//     h += asso_values[str[1]]                 (len >= 2)
//     h += asso_values[str[5]]                 (len >= 6)
//     h += asso_values[str[7]]                 (len >= 8)
// asso_values is a 256-entry table of 32-bit contributions (X360 dword_82F725B0
// is dword-indexed: the `__ROL4__(b,2)` == b<<2 lowers the byte index to a dword
// byte-offset). It is therefore typed `u32`, not the `unsigned char` used by the
// indirection-style siblings.
//
// NOTE: the two static data tables (gperf asso_values / wordlist) live in the
// X360 .rdata section (dword_82F725B0[256] and off_82F78FD8[0x11A]) and are NOT
// recoverable from the per-function exports (no .rdata dump exists). They are
// declared here as opaque statics -- exactly as the LANDED siblings
// (StringMembersIndex/AptString.h, ObjectIndex/AptObjectIndex.h) do -- and MUST
// be populated from a binary data dump (or by re-running gperf on the recovered
// keyword list) before this TU links. The per-TU gate is `cl /c` (compile only),
// which the declarations satisfy; no placeholder data is fabricated. See notes.
class SpriteMembersIndex
{
public:
    enum
    {
        MIN_WORD_LENGTH = 2,
        MAX_WORD_LENGTH = 20,
        MIN_HASH_VALUE  = 0,        // provisional: lower bound not pinned by pseudocode
        MAX_HASH_VALUE  = 0x119     // 281
    };

    // gperf %struct-type element. The first field MUST be the keyword string
    // (gperf relies on it; the recognizer dereferences only `.mpcName`). The
    // trailing field is the per-member payload that objectMemberLookup /
    // objectMemberSet read out of the returned entry; it is one dword on X360
    // (the entry stride is 2 dwords == 8 bytes). Its precise semantic is the
    // resolved member index, so it is modelled with the complete, included
    // primitive type `u32`. (This is the fix for the prior C3646 'muMemberIndex
    // unknown override specifier': the field had been typed with an unresolved /
    // un-included type token.)
    struct Entry
    {
        const char* mpcName;
        u32         muMemberIndex;
    };

    // gperf perfect-hash lookup. Returns the matching wordlist Entry, or null if
    // the (string, len) pair is not a recognised member name. __fastcall in the
    // X360 ABI; signature mirrors gperf's
    //   static const Entry* in_word_set(const char* str, unsigned int len).
    static const Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    static unsigned int hash(const char* lpcStr, unsigned int luLen);

    // --- Opaque generated tables (X360 .rdata; fill from binary dump). ---
    // asso_values[256]: per-byte 32-bit hash contribution (X360 dword_82F725B0).
    static const u32   asso_values[256];
    // wordlist[]: the keyword entries, indexed DIRECTLY by hash value
    //             (X360 off_82F78FD8, 0x11A == MAX_HASH_VALUE + 1 slots).
    static const Entry wordlist[];
};
