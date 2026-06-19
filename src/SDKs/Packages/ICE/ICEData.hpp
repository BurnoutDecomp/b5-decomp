#ifndef SDKS_PACKAGES_ICE_ICEDATA_HPP
#define SDKS_PACKAGES_ICE_ICEDATA_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEData.hpp
//
// ICE::ICETakeData -- one camera-take record in an ICE take dictionary -- plus
// ICE::ICEElementCount. This is the canonical mirrored home (the DecFIGS DWARF
// declares both types in SDKs/Packages/ICE/ICEData.hpp, and the ledger maps the
// ICE::ICETakeData::* methods to SDKs/Packages/ICE/ICEData.cpp).
//
// This file currently models a MINIMAL but FAITHFUL slice: the fixed ICETakeData
// header (matched field-for-field against the DWARF) is enough for the consumers
// landed so far (BrnResource::ICEList reads only miGuid @+0x8 in
// GetICETakeDataFromGuid). The future `class:ICE::ICETakeData` / ICEData.cpp TU
// EXTENDS this header in place (its bTNode tree-node behaviour, the variable
// channel data, and the GetElementCounts/GetNumKeys/GetNumIntervals accessors) --
// it must NOT re-declare these types in a parallel header.
//
// LAYOUT (32-bit, DWARF ICEData.hpp:51-79; miGuid @+0x8 also X360-attested):
//   @0x00  bTNode<ICETakeData> base       (tree node, 8 bytes on 32-bit)
//   @0x08  s32  miGuid
//   @0x0C  char macTakeName[KI_MAX_TAKENAME_LENGTH]
//   @0x2C  f32  mfLength        (DWARF float32_t -> project f32; float32_t is NOT a project type)
//   @0x30  u32  muAllocated
//   @0x34  ICEElementCount mElementCounts[12]   (one per ICE channel)
// ============================================================================

namespace ICE
{

// ICEData.hpp:64 (DWARF). Fixed take-name buffer length.
static const s32 KI_MAX_TAKENAME_LENGTH = 32;

// ICEData.hpp:51 (DWARF). Per-channel interval/key counts -- 4 bytes (two u16); a
// 12-entry array spans the 0x34..0x5B region of the take header.
struct ICEElementCount
{
    u16 mu16Intervals;
    u16 mu16Keys;
};

// ICEData.hpp:61 (DWARF). ICE::ICETakeData : public bTNode<ICE::ICETakeData>.
// The bTNode<ICETakeData> tree-node base (bNode: Next/Prev) occupies the first 8
// bytes on 32-bit; that base has no reconstructed home yet, so it is modeled here
// as an explicit padding buffer (a placeholder for the not-yet-reconstructed base,
// to be replaced when the bNode/bTNode TU lands -- NOT an offset hack). The named
// fields below match the DWARF field-for-field (only miGuid @+0x8 is additionally
// X360-attested, via the BrnResource::ICEList guid lookup).
struct ICETakeData
{
    u8              mPadNodeBase[8];                    // @0x00  bNode/bTNode tree base (8 bytes)
    s32             miGuid;                             // @0x08  GameDB id (read by ICEList)
    char            macTakeName[KI_MAX_TAKENAME_LENGTH];// @0x0C  take name
    f32             mfLength;                           // @0x2C  take length (seconds)
    u32             muAllocated;                        // @0x30  allocated size/flag
    ICEElementCount mElementCounts[12];                 // @0x34  per-channel interval/key counts
};

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEDATA_HPP
