#pragma once

// ===========================================================================
// EATech Apt (ActionScript player) TextField member-name -> member-id
// perfect-hash recognizer.
//
// This is gperf-GENERATED code (GNU gperf, C++ output, options roughly
//   -C -t -L C++ --class-name=TextMembersIndex
//    --lookup-function-name=in_word_set
// WITHOUT duplicate-key support -- the hash directly indexes the wordlist).
// The X360 binary contains the canonical machine translation at 0x82AD6EB8;
// this reconstruction reproduces the SAME algorithm, structure, and data.
//
// Per CXX_NAMING_CONVENTIONS.md: "Generated files may follow generator or
// schema names" -- so the gperf-idiomatic identifiers (in_word_set, the static
// asso_values/wordlist tables, the Entry struct) are kept verbatim rather than
// re-styled. Do NOT hand-edit; regenerate from the .gperf if the keyword set
// changes.
//
// The lookup maps an ActionScript TextField property name string (e.g.
// "text", "textColor", "autoSize", "wordWrap", "scroll", "border") to its
// member id. Resolved by AptCIH::objectMemberLookup / ::objectMemberSet (X360
// callers at 0x82B0DF70 / 0x82B09E58); both read Entry.miData, subtract 1, and
// use the result as a 0-based jump-table index (objectMemberLookup gates
// id-1 <= 0x14, objectMemberSet gates id-1 <= 0x13), so member ids run 1..21.
//
// Like the sibling TextFormatMembersIndex (and unlike ObjectIndex /
// StringMembersIndex, which carry a separate lookup[] dup-table), this
// recognizer's gperf output has NO collisions in the keyword set, so the hash
// value indexes the wordlist directly (X360: `result = &off_82F79AB0 + 2*v5`,
// no negative-index escape path).
//
// gperf parameters recovered from the X360 hash arithmetic at 0x82AD6EB8:
//     TOTAL_KEYWORDS  = 21      (member ids 1..21; id 0 is the unused/empty fill)
//     MIN_WORD_LENGTH = 4       ( (len - 4) > 0xD  =>  reject  =>  4 <= len <= 17 )
//     MAX_WORD_LENGTH = 17
//     MAX_HASH_VALUE  = 0x1D    ( hash > 0x1D rejects;  30 wordlist slots )
//     key positions   = str[0], str[1], str[8]   (the [1]/[8] indices are
//                                                  length-gated; the gate
//                                                  guarantees [1] is in range)
//     hash            = len + asso[str[0]] + asso[str[1]] (+ asso[str[8]])
//
// The two static data tables (gperf asso_values / wordlist) were extracted
// from the X360 .rdata via the IDA database:
//     asso_values[256] @ byte_82F72CB0
//     wordlist[30]     @ off_82F79AB0   (Entry{ const char* name; int data })
// (see tools/dump_text_tables.py) and are emitted verbatim in the .cpp. Each
// keyword's hash was verified to land in exactly its extracted wordlist slot.
class TextMembersIndex
{
public:
    // gperf wordlist entry (the "-t" struct). Each on-disk entry is 8 bytes:
    //   +0 const char* name   (the keyword string)
    //   +4 int         data   (the TextField member id consumed by the caller)
    // Kept as the generated layout so &wordlist[i] arithmetic (stride 2 dwords)
    // matches the X360.
    struct Entry
    {
        const char* mpcName;
        int         miData;
    };

    // gperf perfect-hash lookup.
    // Returns the matching wordlist Entry, or null if the (string,len) pair is
    // not a recognised TextField member name. __fastcall in the X360 ABI;
    // signature mirrors gperf's
    //   `static const Entry* in_word_set(const char* str, unsigned int len)`.
    static const Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    // gperf compile-time descriptors (values derived from the X360 hash code).
    enum
    {
        TOTAL_KEYWORDS  = 21,
        MIN_WORD_LENGTH = 4,
        MAX_WORD_LENGTH = 17,
        MIN_HASH_VALUE  = 0,
        MAX_HASH_VALUE  = 0x1D,  // 29
        WORDLIST_SLOTS  = 0x1E   // 30 == MAX_HASH_VALUE + 1
    };

    // --- Generated tables (extracted from X360 .rdata; see .cpp). ---
    // asso_values[256]: per-byte hash contribution (X360 byte_82F72CB0).
    static const unsigned char asso_values[256];
    // wordlist[WORDLIST_SLOTS]: keyword entries indexed directly by hash value
    //                           (X360 off_82F79AB0). Empty slots hold {"",0}.
    static const Entry         wordlist[WORDLIST_SLOTS];
};
