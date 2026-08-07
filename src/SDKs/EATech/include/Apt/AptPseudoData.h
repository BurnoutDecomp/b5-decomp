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
// NATIVE-8 RE-LAY (2026-07-02; was the console 4-byte record view, whose
// straddled reads were half of the replay-merge crash). The record is the
// GUIAPT64 place COMMAND (tag@0, 8-aligned body at cmd+8 -- command records are
// pointer-aligned): the same body map PlaceCommand / resolve64 case-3 /
// DoTemporaryFrameControls read. Console shadow offsets in brackets.
struct AptPlaceObjectInfo_t
{
    u8  maPad00[8];          // [0x00] tag dword + the align pad to the body
    u32 muxFlags;            // [0x08] body+0x00  PlaceObject flag bits   [c:+0x04]
    s32 mi32Depth;           // [0x0C] body+0x04  display depth (i32)     [c: s16 @+0x38]
    s32 mi32CharacterId;     // [0x10] body+0x08  placed character id
    u8  maMatrix[24];        // [0x14] body+0x0C  2D affine (6 floats)    [c:+0x10]
    u8  maColorTransform[8]; // [0x2C] body+0x24  packed colour transform [c:+0x28]
    f32 mfRatio;             // [0x34] body+0x2C  morph/tween ratio       [c:+0x30]
    const char* mpName;      // [0x38] body+0x30  instance-name ptr8
    s32 miClipDepth;         // [0x40] body+0x38  clip depth
    u8  maPad44[4];          // [0x44]
    void* mpClipActions;     // [0x48] body+0x40  clipActions block ptr8  [c: value @+0x3C]
};

struct AptPseudoData_t
{
    // ---- layout (28 bytes; offsets verified against the X360 ctor) ----
    void* mpData;             // [0x00] caller-supplied data context (ctor arg)
    void* mpMatrix;           // [0x04] &source.maMatrix  if flag 0x04 set, else null
    void* mpColorTransform;   // [0x08] &source.maColorTransform if 0x08 set, else null
    // [0x0C] the console captured its 4-byte clip-actions VALUE here (flag 0x80);
    // the native-8 record carries a POINTER instead, which this 4-byte slot cannot
    // hold -- and the slot is UNREAD by the mergeState props-overlay pun (it lands
    // on AptFramePlacementProps::mnReserved0C), so it stays 0 on the native-8
    // path -- verified unread (the overlay pun lands it on a reserved slot);
    // nothing consumes the console value.
    s32   miClipActionValue;
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
