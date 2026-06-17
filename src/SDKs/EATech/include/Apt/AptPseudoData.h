#pragma once

// =====================================================================
//  AptPseudoData.h  â€”  EA APT (ActionScript Player Technology) middleware
//
//  AptPseudoData_t is a small (28-byte) "snapshot" record that APT builds
//  for a display-list entry while interpreting a PlaceObject-style command.
//  It captures, by flag, the optional fields of a source place-object info
//  record (matrix / colour-transform pointers, ratio, clip-action value)
//  plus the character id and depth.
//
//  NO DWARF exists for this class (X360-only; not present in the PS3
//  DecFIGS dump nor in the EATech public Apt.h). All shape below is derived
//  STRICTLY from the X360 binary pseudocode @ 0x82AD9910. Member names are
//  inferred; the flag bit meanings match the SWF/GFx PlaceObject2 tag bits
//  (0x04 HasMatrix, 0x08 HasColorTransform, 0x10 HasRatio, 0x80
//  HasClipActions), which the capture logic matches exactly. See notes.
// =====================================================================
#pragma once

#include "types.hpp"

// Source place-object info record that AptPseudoData_t snapshots from.
// MINIMAL SLICE: only the offsets the constructor reads are named; the
// holes are explicit padding so the named members land at the binary
// offsets. The real type is the APT display-list / place-object info
// record (uncommitted in this batch â€” modelled here as an opaque slice).
struct AptPlaceObjectInfo_t
{
    u8  maPad00[4];        // [0x00] (unread by this ctor)
    u32 muxFlags;          // [0x04] PlaceObject flag bits (0x04/0x08/0x10/0x80)
    u8  maPad08[8];        // [0x08]
    u8  maMatrix[24];      // [0x10] transform matrix block (address captured)
    u8  maColorTransform[8]; // [0x28] colour-transform block (address captured)
    f32 mfRatio;           // [0x30] morph/tween ratio
    u8  maPad34[4];        // [0x34]
    s16 mi16Depth;         // [0x38] display depth
    u8  maPad3A[2];        // [0x3A]
    s32 miClipActionValue; // [0x3C] clip-actions / extra value
};

struct AptPseudoData_t
{
    // ---- layout (28 bytes; offsets verified against the X360 ctor) ----
    void* mpData;             // [0x00] caller-supplied data context (ctor arg)
    void* mpMatrix;           // [0x04] &source.maMatrix  if flag 0x04 set, else null
    void* mpColorTransform;   // [0x08] &source.maColorTransform if 0x08 set, else null
    s32   miClipActionValue;  // [0x0C] source clip value   if 0x80 set, else 0
    f32   mfRatio;            // [0x10] source ratio         if 0x10 set, else 0.0
    u32   muxFlags;           // [0x14] copy of source flag bits
    s16   mi16CharacterId;    // [0x18] character id (ctor arg)
    s16   mi16Depth;          // [0x1A] copy of source depth

    // Constructed from a source place-object info record. Returns *this in
    // the X360 fastcall convention (modelled as a normal constructor).
    AptPseudoData_t(const AptPlaceObjectInfo_t* lpSource,
                    s16                          li16CharacterId,
                    void*                        lpData);
};