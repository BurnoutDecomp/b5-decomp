#pragma once

#include "types.hpp"

// ===========================================================================
// Attrib::EditSpecifier + its strict-weak-ordering comparator.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). An
// EditSpecifier identifies a single attribute edit by a four-field key; the
// editor/live-link edit machinery (Attrib::EditRecord, the X360 caller) keeps
// EditSpecifiers in a sorted container and orders them with the comparator
// reconstructed here:
//
//     Attrib::EditSpecifierLess::operator()  @ 0x82803DC0
//
// (The TU id's trailing `>` is the demangler artefact of the comparator's
// template-instance spelling; the operator() is the only entry point.)
//
// There is no reference source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 asm of the comparator. The comparator
// reads four key fields and nothing else, so EditSpecifier is modelled with
// exactly those fields at the asm-attested offsets (member-by-name access):
//
//     +0x00  muKey0   u64   (ld 0(r4)/0(r5); unsigned 64-bit compare)
//     +0x08  muKey1   u64   (ld 8(r4)/8(r5); unsigned 64-bit compare)
//     +0x10  muKey2   u64   (ld 0x10; unsigned 64-bit compare)
//     +0x18  muKey3   u32   (lwz 0x18; unsigned 32-bit compare, the tie-breaker)
//
// `Attrib` is a vendor (AttribSys) library boundary, so its identifiers
// (Attrib, EditSpecifier) are preserved per the naming convention.
// ===========================================================================

namespace Attrib
{

// The four-field edit key. Only the fields the comparator touches are modelled;
// any further EditSpecifier payload lives in its own (editor-only, non-boot) TU.
struct EditSpecifier
{
    u64 muKey0; // +0x00
    u64 muKey1; // +0x08
    u64 muKey2; // +0x10
    u32 muKey3; // +0x18
};

// Strict-weak-ordering comparator: a lexicographic, all-unsigned less-than over
// (muKey0, muKey1, muKey2, muKey3). Used by EditRecord's sorted EditSpecifier
// container.
struct EditSpecifierLess
{
    // @ 0x82803DC0 -- returns lhs < rhs in the (key0,key1,key2,key3) ordering.
    bool operator()(const EditSpecifier& lfLhs, const EditSpecifier& lfRhs) const;
};

} // namespace Attrib
