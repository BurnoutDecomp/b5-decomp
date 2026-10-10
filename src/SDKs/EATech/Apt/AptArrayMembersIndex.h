#pragma once

// ===========================================================================
// EATech Apt (ActionScript player) Array member-name -> member-id perfect-hash
// recognizer.
//
// gperf-GENERATED code (GNU gperf, C++ output, roughly
//   -C -t -L C++ --class-name=ArrayMembersIndex --lookup-function-name=in_word_set
// with no duplicate-key table: the hash indexes the wordlist directly). The
// gperf identifiers (ArrayMembersIndex, in_word_set, asso_values, wordlist, the
// Entry struct) are kept verbatim; per CXX_NAMING_CONVENTIONS.md generated code
// may keep its generator's names.
//
// Maps an ActionScript Array property name ("length", "push", "toString", ...)
// to the member id AptArray::objectMemberLookup switches on. On the console its
// only caller is AptArray::objectMemberLookup, which consults it only when the
// receiving value is non-null.
//
// gperf parameters, read off the console hash arithmetic:
//     TOTAL_KEYWORDS  = 13      (member ids 1..13)
//     MIN_WORD_LENGTH = 3       ( (len - 3) > 5  =>  reject  =>  3 <= len <= 8 )
//     MAX_WORD_LENGTH = 8
//     MIN_HASH_VALUE  = 3
//     MAX_HASH_VALUE  = 27      ( hash > 27 rejects;  28 wordlist slots )
//     key positions   = str[0], str[len - 1], plus len
//
// The asso_values[256] and wordlist[28] tables are dumped verbatim from the
// console image .rdata (AptArrayMembersIndex.cpp).
// ===========================================================================

class ArrayMembersIndex
{
public:
    // gperf wordlist entry (the "-t" struct): the keyword and the Array member
    // id the caller dispatches on. 8 bytes per entry on the console.
    struct Entry
    {
        const char* mpcName;
        int         miData;
    };

    // gperf perfect-hash lookup: the matching wordlist Entry, or null when
    // (lpcStr, luLen) is not a recognised Array member name.
    static const Entry* in_word_set(const char* lpcStr, unsigned int luLen);

private:
    enum
    {
        TOTAL_KEYWORDS  = 13,
        MIN_WORD_LENGTH = 3,
        MAX_WORD_LENGTH = 8,
        MIN_HASH_VALUE  = 3,
        MAX_HASH_VALUE  = 27,
        WORDLIST_SLOTS  = 28   // MAX_HASH_VALUE + 1
    };
};
