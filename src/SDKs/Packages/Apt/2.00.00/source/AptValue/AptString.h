#pragma once

// ----------------------------------------------------------------------------
// StringMembersIndex
//
// gperf-generated perfect-hash recognizer for the JavaScript-style member /
// method names exposed by EA's Apt SDK type AptString (the ActionScript
// "String" object). The single member StringMembersIndex::in_word_set() is the
// classic gperf %struct-type "in_word_set" entry point; AptString::objectMemberLookup
// calls it to resolve a member name (e.g. "charAt", "concat", "substring",
// "length") to its descriptor entry.
//
// This is GENERATED code: gperf names (StringMembersIndex, in_word_set, hash,
// wordlist, lookup, asso_values, Entry) are preserved verbatim per the naming
// convention's "generated/external API" exception -- they are NOT renamed to the
// Brn/Cgs PascalCase scheme. The class is just a namespace gperf emits to scope
// the recognizer (gperf `%define class-name StringMembersIndex`).
//
// Shape derived from the X360 pseudocode at 0x82AD7918 (the DWARF and Feb-2007
// leak have no source for this TU). The keyword set is the AptString method
// table from SDKs/Packages/Apt/2.00.00/include/Apt/AptValue/AptString.h
// (charAt, charCodeAt, concat, fromCharCode, indexOf, lastIndexOf, slice,
// split, substr, substring, toLowerCase, toUpperCase) -- 12 methods + 1 more
// keyword ("length") == TOTAL_KEYWORDS 13 (proven by the duplicate-list pivot
// `v4 >= -13` / `v10 = -14 - v4`).
//
// gperf parameters recovered from the binary:
//     TOTAL_KEYWORDS  = 13      (lookup-table duplicate pivot: -1 - 13 - v4)
//     MIN_WORD_LENGTH = 5       ( (len - 5) > 7  =>  5 <= len <= 12 )
//     MAX_WORD_LENGTH = 12
//     MIN_HASH_VALUE  = (lo bound; exact value not in pseudocode)
//     MAX_HASH_VALUE  = 22      ( v3 > 0x16 => reject )
//
// gperf `%struct-type` entry: first field is the keyword string; the remaining
// field(s) carry the per-member payload AptString::objectMemberLookup consumes
// (the descriptor / native-function slot). Each entry is 8 bytes on X360
// (`2 * v4` indexing of `off_82F79BA0`, `v12 += 8` stride) == two 4-byte words,
// i.e. { const char *name; <one pointer-sized payload> }. The payload type is
// NOT recoverable from this TU's pseudocode (the recognizer only ever touches
// .name); modelled here as a void* placeholder -- see notes.
class StringMembersIndex
{
public:
    enum
    {
        TOTAL_KEYWORDS  = 13,
        MIN_WORD_LENGTH = 5,
        MAX_WORD_LENGTH = 12,
        MIN_HASH_VALUE  = 0,    // provisional: lower bound not pinned by pseudocode
        MAX_HASH_VALUE  = 22    // 0x16
    };

    // gperf %struct-type element. First field MUST be the keyword (gperf relies
    // on it). The trailing field is the member payload objectMemberLookup reads
    // out of the returned entry; its concrete type is unknown from this TU.
    struct Entry
    {
        const char* name;
        void*       mpPayload;  // provisional -- see notes (likely AptNativeFunction** / member id)
    };

    static const struct Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    static inline unsigned int hash(const char* lpcStr, unsigned int luLen);

    // Backing tables (gperf-emitted). Contents are NOT in the dossier/DWARF and
    // could not be dumped from the binary -- declared here, defined in the .cpp
    // with placeholder data that MUST be filled from a data dump of the .xex at
    // these addresses before this TU is byte-faithful:
    //     asso_values[] @ 0x82F72DF8   (256-entry unsigned char association table)
    //     lookup[]      @ 0x82F79C08   (signed char hash->index/dup table)
    //     wordlist[]    @ 0x82F79BA0   (Entry[], the 13 keyword descriptors)
    static const unsigned char asso_values[256];
    static const signed char   lookup[];
    static const struct Entry  wordlist[];
};