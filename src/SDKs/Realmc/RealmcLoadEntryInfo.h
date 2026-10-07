#pragma once

// ===========================================================================
// RealmcIface::LoadEntryInfo -- a Realmc save/load "load-entry info" record used
// by the X360 memory-card / save-load interface (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX; sibling to RealmcIface::CardData in RealmcCardData.h
// and the RealmcCore primitives in RealmcCore.h).
//
// This header is the canonical OWNING home for the LoadEntryInfo struct and its
// three reconstructed member functions:
//
//     RealmcIface::LoadEntryInfo::LoadEntryInfo  @ 0x82B519E8   (default ctor)
//     RealmcIface::LoadEntryInfo::LoadEntryInfo                 (name + two pairs)
//     RealmcIface::LoadEntryInfo::operator=      @ 0x82B51A78   (copy assign)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, LoadEntryInfo) are preserved
// verbatim per the naming convention.
//
// LAYOUT (from asm -- the ctor zero-stores five fields, operator= copies a
// 32-byte head plus four trailing 32-bit words):
//
//   +0x00              a 32-byte head block. operator= memcpy's it wholesale from
//                      the source; the ctor zeroes ONLY its leading byte (+0x00),
//                      and operator= zeroes the LAST byte (+0x1F) after the copy
//                      (a one-byte flag/terminator). The interior 30 bytes are
//                      opaque save-entry payload (file/slot identifiers, name,
//                      etc.) the X360 moves wholesale, so they are modelled as a
//                      raw byte block rather than fabricated named members.
//   +0x20 .. +0x24     a second { pointer, size } pair: the three-argument ctor
//                      copies its first DataBuffer argument here word for word,
//                      exactly as it copies the second one into +0x28/+0x2C. The
//                      default ctor zero-stores each word; operator= copies each
//                      individually (NOT via the head memcpy). Every caller passes
//                      {0,0} and the Xenon file layer never reads it.
//   +0x28              the entry's DATA-BUFFER pointer. Attested by the X360 asm:
//                      RealmcIface::XenonUtil::ReadFileLayer2 loads *(entry+0x28)
//                      and hands it to ReadFile as the read destination, and
//                      SaveDataRegular hands it to WriteFile as the write source
//                      and to Crc32 as the checksum input. Named `mpData`. On the
//                      32-bit X360 it is a 4-byte word at +0x28; this project
//                      compiles host x64, so it widens to an 8-byte pointer (same
//                      pointer-widening rule as every recon -- the +0x28 offset is
//                      X360-authoritative, the host offset is not byte-identical).
//   +0x2C              the data byte count. Attested: ReadFileLayer2 passes it as
//                      ReadFile's length and compares it against the on-disk
//                      "MC02" header's stored size; SaveDataRegular writes it as
//                      the record's size and CRCs `mpData` over it. Named
//                      `muDataSize` (u32).
//
//   Total X360 modelled size = 0x30 (48) bytes; the host layout is larger because
//   the +0x28 data pointer widens to 8 bytes (semantic parity by named members,
//   not byte offsets).
//
// STORE ORDER (operator=, exact): the +0x20 pair is copied FIRST
// (+0x20, +0x24), then the data pointer (+0x28) and the size (+0x2C), THEN the
// 32-byte head is memcpy'd, THEN the +0x1F flag byte is zeroed last. Reproduced
// verbatim in the .cpp.
// ===========================================================================

#include <cstdint>

namespace RealmcIface
{

class DataBuffer;   // SDKs/Realmc/RealmcDataBuffer.h ({ mpData, muSize })

class LoadEntryInfo
{
public:
    // @ 0x82B519E8 -- zero the four trailing words and the leading head byte.
    LoadEntryInfo();

    // Build a record from an entry name plus two DataBuffer pairs: pA lands in the
    // +0x20 pair, pB in mpData/muDataSize; then the 32-byte name head is copied
    // (and its +0x1F byte cleared) when pName is given, else only the leading
    // head byte is cleared. Callers pass the name ("Mugshots" / the 32-byte
    // macTitle head), a zeroed {0,0} pair, and the { data, size } pair.
    LoadEntryInfo(const char* pName, const DataBuffer* pA, const DataBuffer* pB);

    // Copy the +0x20 pair, the data pointer and the size, then the 32-byte
    // head, then clear the +0x1F flag byte.
    LoadEntryInfo& operator=(const LoadEntryInfo& rOther);

    std::uint8_t  maHead[0x20];     // +0x00  opaque head; +0x1F is a flag byte
    void*         mpAuxData;        // +0x20  first DataBuffer argument's pointer
    std::uint32_t muAuxDataSize;    // +0x24  first DataBuffer argument's size
    void*         mpData;           // +0x28  data-buffer pointer (Read/WriteFile
                                    //         buffer + Crc32 input; asm-attested)
    std::uint32_t muDataSize;       // +0x2C  data byte count (asm-attested)
};
// X360 sizeof == 0x30 (48) bytes; host sizeof is larger (the +0x28 data pointer
// widens to 8 bytes on x64).

} // namespace RealmcIface
