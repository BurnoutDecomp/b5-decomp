#pragma once

// ===========================================================================
// EATech Apt (ActionScript player) Key-object member-name -> entry perfect-hash.
//
// This is gperf-GENERATED code (GNU gperf, C++ output, options roughly
//   -C -t -E -L C++ --class-name=KeyMembersIndex
//    --lookup-function-name=in_word_set  with duplicate keys enabled).
// The X360 binary contains the canonical machine translation at 0x82AD5FB8;
// this reconstruction reproduces the SAME algorithm and structure. It is the
// duplicate-key gperf variant (negative lookup[] entries route to a secondary
// {base,count} table), structurally identical to the sibling AptObjectIndex.
//
// Per CXX_NAMING_CONVENTIONS.md ("Generated files may follow generator or
// schema names") the gperf-idiomatic identifiers (in_word_set, the static
// asso/lookup/wordlist tables, the Entry struct) are kept verbatim rather than
// re-styled. Do NOT hand-edit; regenerate from the .gperf if the keyword set
// changes.
//
// The lookup maps an ActionScript Key-object member name string to a wordlist
// Entry. Resolved by AptKey::objectMemberLookup (X360 caller).
//
// gperf parameters recovered from the X360 hash arithmetic at 0x82AD5FB8:
//   MIN_WORD_LENGTH = 2, MAX_WORD_LENGTH = 20    ( (len-2) > 0x12 rejects )
//   MAX_HASH_VALUE  = 0x22 (34)                  ( hash > 0x22 rejects )
//   key positions   = str[1], str[5], str[7]     (the [5]/[7] indices are
//                                                  length-gated; [1] is always read)
//   hash = len + asso[str[1]]  (+ asso[str[5]])  (+ asso[str[7]])
//   duplicate keys present (negative lookup[] entries route to a 2nd table)
//   the duplicate-table cutoff is at lookup slot -28 (escape when slot < -27).
//
// NOTE: the gperf static data tables (asso_values / lookup / lengths / wordlist)
// live in the X360 .rdata section (byte_82F723B0, byte_82F78FB4, byte_82F78F98,
// off_82F78EC0) and are NOT recoverable from the dossier pseudocode (no DWARF,
// no leak source, and the .rdata bytes are not dumped here). They are declared
// below as opaque externs and MUST be populated from a binary data dump (or by
// re-running gperf on the recovered keyword list) before this TU links. The
// algorithm above is fully reconstructed; only the backing data is a documented
// floor. (Same disposition as the sibling AptObjectIndex TU.)
// ===========================================================================

class KeyMembersIndex
{
public:
    // gperf wordlist entry (the "-t" struct). Each on-disk entry is 8 bytes:
    //   +0 const char* name   (the keyword string)
    //   +4 int         data   (per-member payload consumed by the caller)
    // Kept as the generated layout so &wordlist[i] arithmetic (stride 2 dwords ==
    // 8 bytes) matches the X360.
    struct Entry
    {
        const char* mpcName;
        int         miData;
    };

    // gperf perfect-hash + linear-probe duplicate resolver.
    // Returns the matching wordlist Entry, or null if the (string,len) pair is
    // not a recognised Key-object member. __fastcall in the X360 ABI; signature
    // mirrors gperf's `static const Entry* in_word_set(const char* str, unsigned int len)`.
    static const Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    // gperf compile-time descriptors (values derived from the X360 hash code).
    enum
    {
        MIN_WORD_LENGTH = 2,
        MAX_WORD_LENGTH = 20,
        MIN_HASH_VALUE  = 0,
        MAX_HASH_VALUE  = 0x22,  // 34
        // Duplicate-table cutoff: slots in [-27 .. -1] are not duplicate ranges;
        // the true range descriptors begin past -27 (escape when slot < -27).
        DUP_CUTOFF      = 27
    };

    // --- Opaque generated tables (X360 .rdata; fill from binary dump). ---
    // asso_values[256]: per-byte hash contribution (X360 byte_82F723B0).
    static const unsigned char asso_values[256];
    // lookup[]: hash -> wordlist index (>=0) or duplicate-table escape (<0)
    //           (X360 byte_82F78FB4; byte_82F78FB5 == &lookup[1], same array).
    static const signed char   lookup[];
    // lengths[]: per-slot expected keyword length (X360 byte_82F78F98; the dup
    //            path's unk_82F78FB3 is an interior pointer into this same table).
    static const unsigned char lengths[];
    // wordlist[]: the keyword entries (X360 off_82F78EC0).
    static const Entry         wordlist[];
};
