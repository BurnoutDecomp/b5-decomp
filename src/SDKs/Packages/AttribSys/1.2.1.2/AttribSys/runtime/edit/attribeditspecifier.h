#pragma once

#include "types.hpp"

// ===========================================================================
// Attrib::EditSpecifier -- the four-field edit key + its strict-weak-ordering
// comparator. Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//
// An EditSpecifier identifies a single attribute edit by a four-field key. The
// editor/live-link edit machinery (Attrib::EditRecord, Attrib::EditTable) keeps
// EditSpecifiers in a sorted container ordered by EditSpecifierLess::operator().
//
// The member SHAPE and NAMES are authoritative from the AttribSys PS3 DWARF
// (attriblivelink.cpp:110-134, struct Attrib::EditSpecifier): mClassKey /
// mCollectionKey / mAttribKey are Attribute::Key, mIndex is unsigned int. On the
// X360 spine Attribute::Key is a 64-bit hash key: Decode stores the three keys
// with 8-byte `std` at offsets 0/8/0x10 and the index with a 4-byte `stw` at
// 0x18; the comparator reads the three keys with `ld` (u64) and the index with
// `lwz` (u32).
//
//     +0x00  mClassKey       u64  (Attribute::Key)
//     +0x08  mCollectionKey  u64  (Attribute::Key)
//     +0x10  mAttribKey      u64  (Attribute::Key)
//     +0x18  mIndex          u32  (unsigned int, the tie-breaker)
//
// `Attrib` is a vendor (AttribSys) library boundary, so its identifiers are
// preserved per the naming convention.
// ===========================================================================

namespace Attrib
{

// Forward decl of the attribute Class (registry-resolved by GetClass). Full type
// lives in the class:Attrib::Class TU.
class Class;

// The four-field edit key (DWARF names + X360-attested 8-byte-strided offsets).
struct EditSpecifier
{
    u64 mClassKey;      // +0x00  Attribute::Key
    u64 mCollectionKey; // +0x08  Attribute::Key
    u64 mAttribKey;     // +0x10  Attribute::Key
    u32 mIndex;         // +0x18  unsigned int

    // @ 0x82805898 -- parse a dotted "<class>.<collection>.<attrib>.<index><term>"
    // path into the four fields; returns the delimiter that ended the index token,
    // or null on malformed input. Mutates *this (non-const per DWARF).
    const char* Decode(const char* lpcText);

    // @ 0x82808240 -- resolve mClassKey against the process database's class
    // registry; returns the owning Class* (null if unregistered). const per DWARF.
    Class* GetClass() const;
};

// Strict-weak-ordering comparator: a lexicographic, all-unsigned less-than over
// (mClassKey, mCollectionKey, mAttribKey, mIndex). Used by EditRecord's sorted
// EditSpecifier container. (DWARF spells this as EditSpecifier::operator<; the
// X360 emits it as the standalone functor EditSpecifierLess @ 0x82803DC0.)
struct EditSpecifierLess
{
    // @ 0x82803DC0 -- returns lhs < rhs in the (class,collection,attrib,index) order.
    bool operator()(const EditSpecifier& lfLhs, const EditSpecifier& lfRhs) const;
};

} // namespace Attrib
