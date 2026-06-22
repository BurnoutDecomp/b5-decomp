#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h
//
// Lion (eauk_lion) text/data tokeniser member-token types.
//
// sLionMemberToken describes one field of a Lion struct: its value type
// (mType), an enum/value tag (mValue), the byte offset of the field within the
// owning struct (mOffset), the member's text name (mpString) and its hash
// (mHash). A null-mType token terminates a token array.
//
// cLionTokenTable wraps a pointer to a (null-terminated) sLionMemberToken array
// (mpTokens) and provides struct-wide operations over it.
//
// Layout/declarations are from the DecFIGS DWARF (LionTokeniser.h:38 / :138) and
// verified against the X360 asm. sLionMemberToken is 5 words (20 bytes) on X360:
//   mType@0  mValue@4  mOffset@8  mpString@12(ptr)  mHash@16
// EndianTwiddle steps the token array by sizeof(sLionMemberToken); members are
// accessed by name, never by raw offset.
//
// NOTE: a minimal earlier stand-in for cLionTokenTable (with a nested TokenEntry
// and only BuildHashes) lives in eauk_common/Common/ElfHash.h, owned by another
// group. This header is the proper DWARF-backed home for the full type. The two
// are never included into the same TU today, so there is no ODR clash. Flagged
// for consolidation -- see dep_flags.
// ============================================================================

#include "types.hpp"

// Lion member value-type tags (mType). The byte-swap width per type is taken from
// the X360 EndianTwiddle jump table.
enum ELionMemberType
{
    E_LION_MEMBER_NONE   = 0,
    // mType 3,4 -> a 16-bit value.
    E_LION_MEMBER_S16    = 3,
    E_LION_MEMBER_U16    = 4,
    // mType 5..11,14 -> a single 32-bit value.
    E_LION_MEMBER_S32    = 5,
    E_LION_MEMBER_U32    = 6,
    E_LION_MEMBER_F32    = 7,
    E_LION_MEMBER_ENUM   = 8,
    E_LION_MEMBER_STRUCT = 9,
    E_LION_MEMBER_HASH   = 10,
    E_LION_MEMBER_COLOUR = 11,
    // mType 12 -> a 64-byte block (e.g. a 4x4 matrix), 16 words.
    E_LION_MEMBER_MATRIX = 12,
    // mType 13,15 -> a 16-byte block (4 words, e.g. a vector / quaternion).
    E_LION_MEMBER_VECTOR = 13,
    E_LION_MEMBER_POINTER = 14,
    E_LION_MEMBER_QUAT   = 15,
};

// LionTokeniser.h:38
struct sLionMemberToken
{
    // void EndianTwiddle(void* apStruct) const: byte-swap this member's value in
    // place within the struct image based at apStruct (apStruct + mOffset is the
    // field address). Width depends on mType.
    void EndianTwiddle(void* apStruct) const;

    u32   mType;
    u32   mValue;
    u32   mOffset;
    char* mpString;
    u32   mHash;
};

// LionTokeniser.h:138
struct cLionTokenTable
{
    // void EndianTwiddle(void* apStruct) const: walk every token and byte-swap each
    // member of the struct image based at apStruct. Nested struct members (mType
    // E_LION_MEMBER_STRUCT) that re-appear at the same offset later in the table are
    // twiddled only once.
    void EndianTwiddle(void* apStruct) const;

    sLionMemberToken* mpTokens;
};
