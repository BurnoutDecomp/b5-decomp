#pragma once

// ===========================================================================
// EATech Apt (ActionScript player) TextFormat member-name -> member-id
// perfect-hash recognizer.
//
// This is gperf-GENERATED code (GNU gperf, C++ output, options roughly
//   -C -t -L C++ --class-name=TextFormatMembersIndex
//    --lookup-function-name=in_word_set
// WITHOUT duplicate-key support -- the hash directly indexes the wordlist).
// The X360 binary contains the canonical machine translation at 0x82AD68A0;
// this reconstruction reproduces the SAME algorithm, structure, and data.
//
// Per CXX_NAMING_CONVENTIONS.md: "Generated files may follow generator or
// schema names" -- so the gperf-idiomatic identifiers (in_word_set, the static
// asso_values/wordlist tables, the Entry struct) are kept verbatim rather than
// re-styled. Do NOT hand-edit; regenerate from the .gperf if the keyword set
// changes.
//
// The lookup maps an ActionScript TextFormat property name string (e.g.
// "align", "color", "font", "bold", "url") to its member id. Resolved by
// AptTextFormat::objectMemberLookup / ::objectMemberSet (X360 callers at
// 0x82AF18E0 / 0x82AFB368).
//
// Unlike ObjectIndex / StringMembersIndex (which carry a separate lookup[]
// dup-table), this recognizer's gperf output has NO collisions in the keyword
// set, so the hash value indexes the wordlist directly (X360:
// `result = &off_82F798A8 + 2*v5`, no negative-index escape path).
//
// gperf parameters recovered from the X360 hash arithmetic at 0x82AD68A0:
//     TOTAL_KEYWORDS  = 16      (align..url member ids 1..16; id 0 is unused)
//     MIN_WORD_LENGTH = 3       ( (len - 3) > 8  =>  reject  =>  3 <= len <= 11 )
//     MAX_WORD_LENGTH = 11
//     MIN_HASH_VALUE  = 3
//     MAX_HASH_VALUE  = 0x24    ( hash > 0x24 rejects;  37 wordlist slots )
//     key positions   = str[0], str[5], str[7]   (the 5/7 indices are length-gated)
//
// The two static data tables (gperf asso_values / wordlist) were extracted
// from the X360 .rdata via the IDA database:
//     asso_values[256] @ byte_82F72AB0
//     wordlist[37]     @ off_82F798A8   (Entry{ const char* name; int data })
// (see tools/dump_textformat_tables.py) and are emitted verbatim in the .cpp.
class TextFormatMembersIndex
{
public:
    // gperf wordlist entry (the "-t" struct). Each on-disk entry is 8 bytes:
    //   +0 const char* name   (the keyword string)
    //   +4 int         data   (the TextFormat member id consumed by the caller)
    // Kept as the generated layout so &wordlist[i] arithmetic (stride 2 dwords)
    // matches the X360.
    struct Entry
    {
        const char* mpcName;
        int         miData;
    };

    // gperf perfect-hash lookup.
    // Returns the matching wordlist Entry, or null if the (string,len) pair is
    // not a recognised TextFormat member name. __fastcall in the X360 ABI;
    // signature mirrors gperf's
    //   `static const Entry* in_word_set(const char* str, unsigned int len)`.
    static const Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    // gperf compile-time descriptors (values derived from the X360 hash code).
    enum
    {
        TOTAL_KEYWORDS  = 16,
        MIN_WORD_LENGTH = 3,
        MAX_WORD_LENGTH = 11,
        MIN_HASH_VALUE  = 3,
        MAX_HASH_VALUE  = 0x24,  // 36
        WORDLIST_SLOTS  = 0x25   // 37 == MAX_HASH_VALUE + 1
    };

    // --- Generated tables (extracted from X360 .rdata; see .cpp). ---
    // asso_values[256]: per-byte hash contribution (X360 byte_82F72AB0).
    static const unsigned char asso_values[256];
    // wordlist[WORDLIST_SLOTS]: keyword entries indexed directly by hash value
    //                           (X360 off_82F798A8). Empty slots hold {"",0}.
    static const Entry         wordlist[WORDLIST_SLOTS];
};
