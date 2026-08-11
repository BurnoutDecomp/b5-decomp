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

#include <cstddef>   // offsetof (the native-8 place-body layout static_asserts)

#include "types.hpp"

// Source place-object info record that AptPseudoData_t snapshots from.
// MINIMAL SLICE: only the offsets the constructor reads are named; the
// holes are explicit padding so the named members land at the binary
// offsets. The real type is the APT display-list / place-object info
// record (uncommitted in this batch â€” modelled here as an opaque slice).
// NATIVE-8 RE-LAY (2026-07-02; was the console 4-byte record view, whose
// straddled reads were half of the replay-merge crash). The record is the
// GUIAPT64 place COMMAND (tag@0, then the body at align8(cmd+4) -- the BODY is
// pointer-aligned, the record start is not): the same body map PlaceCommand /
// resolve64 case-3 / DoTemporaryFrameControls read. Console shadow offsets in brackets.

// The place record's BODY -- the part that follows the tag dword, laid out at the
// serialised native-8 offsets. Console shadow offsets (measured from the record base,
// where the console body is always record+4) in brackets.
struct AptPlaceObjectBody_t
{
    u32 muxFlags;            // body+0x00  PlaceObject flag bits          [c: cmd+0x04]
    s32 mi32Depth;           // body+0x04  display depth (i32)            [c: cmd+0x08]
    s32 mi32CharacterId;     // body+0x08  placed character id            [c: cmd+0x0C]
    u8  maMatrix[24];        // body+0x0C  2D affine (6 floats)           [c: cmd+0x10]
    u8  maColorTransform[8]; // body+0x24  packed colour transform        [c: cmd+0x28]
    f32 mfRatio;             // body+0x2C  morph/tween ratio              [c: cmd+0x30]
    const char* mpName;      // body+0x30  instance-name ptr8 (resolve64) [c: ptr4 cmd+0x34]
    s32 miClipDepth;         // body+0x38  clip depth                     [c: s16 cmd+0x38]
    u8  maPad3C[4];          // body+0x3C  (pad to the +0x40 pointer slot)
    void* mpClipActions;     // body+0x40  clipActions block ptr8         [c: value cmd+0x3C]
};
static_assert(offsetof(AptPlaceObjectBody_t, mpName)        == 0x30, "place body name@+0x30");
static_assert(offsetof(AptPlaceObjectBody_t, miClipDepth)   == 0x38, "place body clipDepth@+0x38");
static_assert(offsetof(AptPlaceObjectBody_t, mpClipActions) == 0x40, "place body clipActions@+0x40");
static_assert(sizeof(AptPlaceObjectBody_t)                  == 0x48, "place body native-8 size 0x48");

// The place COMMAND record itself: the FrameItem tag dword, then the pointer-aligned body.
struct AptPlaceObjectInfo_t
{
    u32 muTag;               // [0x00] FrameItem tag (3 == place)

    // The body base is align(record + 4, pointerSize) -- NOT a fixed record+8.
    // FrameItem::Write emits the u32 tag then Align()s the stream to the pointer size, so
    // the body is at cmd+8 when the record's ABSOLUTE address is 0 (mod 8) and at cmd+4
    // when it is 4 (mod 8). On the 4-byte console this degenerates to cmd+4 always, which
    // is exactly what the X360 ctor @0x82AD9910 reads (`v4 = a2 + 4`, matrix `a2 + 16`,
    // cxform `a2 + 40`, ratio `*(a2 + 48)`, clipDepth `*(a2 + 56)`).
    // BOTH cases occur in the shipped native-8 data: over the 290 GUIAPT bundles, 2,266 of
    // 106,704 tag-3 records (2.1%) sit at 4 (mod 8) -- e.g. TITLE_SCREEN02 root frame 0
    // 'esrb_anim' @chunk+0x6CF4 {0:203, 4:2}, B5CONTROLLERBUTTONS {0:21, 4:1},
    // SAVELOADCOMPONENT {0:34, 4:0}. (Only tag 3 is affected: the GUIAPT64 emitter never
    // 8-aligns record starts and apt8_fix_frametables.py re-emits the other tags aligned,
    // leaving tag 3 alone precisely because the engine self-adapts -- see
    // references/GUIAPT64_FRAMETABLE_BUG.md.) A fixed 8-byte prologue read every field of
    // those records 4 bytes late: the depth word as muxFlags (wrong HasName bit) and a
    // straddled garbage name pointer.
    // This is the SAME expression AptMovie.cpp's resolve64 case-3, DoTemporaryFrameControls
    // and AptDispatchPlaceCommand already use, so every reader of a place record now agrees.
    // (FLAG -- XB1 divergence, documented not accommodated: the XB1 retail resolve twin
    // sub_1408567D0 case 3 reads these fields at LITERAL cmd+0x38 / cmd+0x48
    // (`mov rax,[rdx+38h]` @0x140856936, `[rdx+48h]` @0x140856957), i.e. its own data had
    // every place record 8-aligned. The generalised align() form reproduces both the X360
    // and the XB1 read for 8-aligned records and additionally reads our emitter's
    // 4-aligned ones correctly.)
    const AptPlaceObjectBody_t* GetBody() const
    {
        return reinterpret_cast<const AptPlaceObjectBody_t*>(
            (reinterpret_cast<uintptr_t>(this) + 4u + 7u) & ~static_cast<uintptr_t>(7u));
    }
    AptPlaceObjectBody_t* GetBody()
    {
        return const_cast<AptPlaceObjectBody_t*>(
            static_cast<const AptPlaceObjectInfo_t*>(this)->GetBody());
    }
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
