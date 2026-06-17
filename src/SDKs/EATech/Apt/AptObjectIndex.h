#pragma once

// ===========================================================================
// EATech Apt (ActionScript player) global-object name -> entry perfect-hash.
//
// This is gperf-GENERATED code (GNU gperf, C++ output, options roughly
//   -C -t -E -L C++ --class-name=ObjectIndex --lookup-function-name=in_word_set
// with duplicate keys enabled). The X360 binary contains the canonical machine
// translation; this reconstruction reproduces the SAME algorithm and structure.
//
// Per CXX_NAMING_CONVENTIONS.md: "Generated files may follow generator or
// schema names" â€” so the gperf-idiomatic identifiers (in_word_set, the static
// asso/lookup/wordlist tables, the Entry struct) are kept verbatim rather than
// re-styled. Do NOT hand-edit; regenerate from the .gperf if the keyword set
// changes.
//
// The lookup maps an ActionScript global-object / member name string to a
// wordlist Entry. Resolved by AptValue::findChild (X360 caller).
//
// Derived gperf parameters (from the X360 hash arithmetic at 0x82AD7C80):
//   MIN_WORD_LENGTH = 3, MAX_WORD_LENGTH = 14   ( (len-3) > 0xB rejects )
//   MAX_HASH_VALUE  = 0x25 (37)                 ( hval > 0x25 rejects )
//   key positions   = str[0], str[4], str[6]    (the 4/6 indices are length-gated)
//   duplicate keys present (negative lookup[] entries route to a 2nd table)
// 38 (=AptVFT_NumVFTs) shows up as the duplicate-table cutoff, consistent with
// one entry per Apt virtual-function-table object type.
//
// NOTE: the three static data tables (gperf asso_values / lookup / wordlist)
// live in the X360 .rdata section (byte_82F72EF8, byte_82F79D50, off_82F79C20)
// and are NOT recoverable from the dossier pseudocode. They are declared here
// as opaque externs and MUST be populated from a binary data dump (or by
// re-running gperf on the recovered keyword list) before this TU links. See
// notes.
class ObjectIndex
{
public:
    // gperf wordlist entry (the "-t" struct). Each on-disk entry is 8 bytes:
    //   +0 const char* name   (the keyword string)
    //   +4 int         data   (per-object payload consumed by the caller;
    //                          exact semantic unconfirmed from binary)
    // Kept as the generated layout so &wordlist[i] arithmetic matches the X360.
    struct Entry
    {
        const char* mpcName;
        int         miData;
    };

    // gperf perfect-hash + linear-probe duplicate resolver.
    // Returns the matching wordlist Entry, or null if the (string,len) pair is
    // not a recognised keyword.  __fastcall in the X360 ABI; signature mirrors
    // gperf's `static const Entry* in_word_set(const char* str, unsigned int len)`.
    static const Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    // gperf compile-time descriptors (values derived from the X360 hash code).
    enum
    {
        TOTAL_KEYWORDS  = 38,   // == AptVFT_NumVFTs (one per object type)
        MIN_WORD_LENGTH = 3,
        MAX_WORD_LENGTH = 14,
        MIN_HASH_VALUE  = 0,
        MAX_HASH_VALUE  = 0x25  // 37
    };

    // --- Opaque generated tables (X360 .rdata; fill from binary dump). ---
    // asso_values[256]: per-byte hash contribution (X360 byte_82F72EF8).
    static const unsigned char asso_values[256];
    // lookup[]: hash -> wordlist index (>=0) or duplicate-table escape (<0)
    //           (X360 byte_82F79D50; byte_82F79D51 is &lookup[1], same array).
    static const signed char   lookup[];
    // wordlist[TOTAL_KEYWORDS]: the keyword entries (X360 off_82F79C20).
    static const Entry         wordlist[];
};