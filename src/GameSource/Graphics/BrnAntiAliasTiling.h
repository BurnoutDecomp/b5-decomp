#ifndef BRN_ANTI_ALIAS_TILING_H
#define BRN_ANTI_ALIAS_TILING_H

#include "types.hpp"

// =================================================================================================
// The anti-alias (scene) buffer's PREDICATED-TILING PLANS -- the two read-only .rdata records the
// X360 image carries at unk_8203E080 and unk_8203E0C8, recovered byte-exact.
//
// WHY THIS IS A SHARED HEADER AND NOT A FILE-LOCAL TABLE.
// Three functions in TWO different translation units read these same two records:
//     BrnRendererMemory::CreateAntiAliasBuffer @0x823F6B40  (picks a plan, takes its format + extent)
//     BrnRendererModule::BeginRenderAntiAliased @0x823FFA18 (hands the rect list to BeginTiling)
//     BrnRendererModule::ResolveMSAA @0x823FFBE0            (walks the rect list resolving each tile)
// Landing them file-local in BrnRendererMemory.cpp would guarantee a second, independently-drifting
// copy of one piece of console data the moment the frame bracket lands. One definition, here.
//
// WHAT THE FIELDS ARE. The record STRIDE is fixed by the symbols themselves --
// 0x8203E0C8 - 0x8203E080 = 0x48 -- which is an 8-byte header plus FOUR 16-byte rectangles. The
// arithmetic then names the fields; none of this is assumed:
//     unk_8203E080 = { 1, 2, {0,0,1280,384}, {0,384,1280,720} }
//     unk_8203E0C8 = { 0, 1, {0,0,1280,720} }
// Rectangle 1's TOP (384) is rectangle 0's BOTTOM, and rectangle 1's BOTTOM (720) is the screen
// height, so the four dwords are left/top/right/bottom and the rectangles tile a 1280x720 screen
// exactly: two horizontal bands in the first record, one full-screen rectangle in the second. +0x04
// is therefore the rectangle count (2 / 1) and +0x00 is the multisample format, which
// CreateAntiAliasBuffer stores straight into CgsRenderTarget::mn32MultiSampleFormat.
//
// WHICH RECORD IS WHICH: the zero branch of CreateAntiAliasBuffer's test
// (clrlwi/cmplwi/beq @0x823F6BA8-0x823F6BD4) goes to unk_8203E0C8, so TRUE selects unk_8203E080.
// BrnRendererMemory::Construct @0x823FCA38 forwards its own trailing byte argument untouched
// (lbz r5, arg_97 @0x823FCAAC), and that argument is lbEnableMSAA. So MSAA ON -> two tiles at
// multisample format 1; MSAA OFF -> one tile at format 0.
//
// WHY MSAA NEEDS TWO TILES -- the number that proves the whole reading. The Xenos renders into 10 MB
// of EDRAM. At 1280x720x4 bytes a colour surface is 720 EDRAM tiles and its depth another 720:
// 1440 of the 2048 available. Double the sample count and it is 2880, which does not fit. Two
// 1280x384 bands cost 768 + 768 = 1536, which does. That is also why CreateAntiAliasBuffer computes
// its EDRAM footprint from ONE rectangle: predicated tiling holds a single tile in EDRAM at a time,
// and the rectangles are successive GPU passes over the same surface -- NOT bands of a taller one,
// which is why the target keeps ONE section.
//
// ⚠️ NONE OF THIS SURVIVES ON PC. pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp honours no EDRAM
// base, tile index, hierarchical-Z or compression base ("they describe hardware that does not exist
// here"), and D3D9 has no predicated tiling. The plans are reproduced because they are what the
// binary reads; the PC path uses only the multisample format and the full extent.
// =================================================================================================

namespace BrnGraphics
{
    struct AntiAliasTilingPlan
    {
        struct TileRect
        {
            u32 mu32Left;    // +0x00
            u32 mu32Top;     // +0x04
            u32 mu32Right;   // +0x08
            u32 mu32Bottom;  // +0x0C
        };

        s32      mn32MultiSampleFormat;  // +0x00 -> CgsRenderTarget::SetMultisampleFormat
        u32      mu32NumTiles;           // +0x04
        TileRect maTile[4];              // +0x08, stride 0x10
    };

    // Pointer-free records, so their sizes are invariant across the 32-bit guest and the LLP64 host --
    // these two are safe to pin, and they are the only layout facts here that are.
    static_assert(sizeof(AntiAliasTilingPlan::TileRect) == 0x10, "tiling rectangle stride is 0x10");
    static_assert(sizeof(AntiAliasTilingPlan) == 0x48,
                  "tiling record stride is 0x48 == 0x8203E0C8 - 0x8203E080");

    // X360 unk_8203E080: multisample format 1, the screen split into two horizontal bands.
    const AntiAliasTilingPlan KMSAA_TILING_PLAN =
    {
        1, 2, { { 0, 0, 1280, 384 }, { 0, 384, 1280, 720 } }
    };

    // X360 unk_8203E0C8: no multisampling, one full-screen tile.
    //
    // Rectangles past mu32NumTiles are left zero-initialised. For this record that is a deliberate,
    // disclosed gap rather than a transcription: the dump window for unk_8203E0C8 ran to +0x3C while
    // the record stride is 0x48, so 0x8203E108 / 0x8203E10C were never observed. They have no reader
    // in the image -- every consumer stops at mu32NumTiles -- so the zeros cannot be load-bearing.
    // If a reader is ever found, dump those two dwords before trusting them.
    const AntiAliasTilingPlan KNO_MSAA_TILING_PLAN =
    {
        0, 1, { { 0, 0, 1280, 720 } }
    };
}

#endif // BRN_ANTI_ALIAS_TILING_H
