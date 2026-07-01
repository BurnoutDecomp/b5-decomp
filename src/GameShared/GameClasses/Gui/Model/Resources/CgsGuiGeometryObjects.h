#pragma once

#include "types.hpp"

// CgsResource gui-geometry resource family: a GuiGeometryObject owns an array of
// GuiGeometryFiles, each of which owns an array of GuiGeometryMeshes, each of which
// owns an array of vertices. On disk every "pointer" is a 32-bit offset-from-base; the
// FixUp / FixDown methods walk the whole tree and rebase those pointers in place
// (FixUp adds the load delta, FixDown subtracts it back to offsets).
//
// Recovered from the X360 spine (GuiGeometryFile::FixDown @0x82858308, FixUp
// @0x828500E0; GuiGeometryObject::FixDown @0x8285B828, FixUp @0x82852FC8). Member set,
// order and types are from the DecFIGS DWARF (CgsGuiGeometryObjects.h). The serialized
// pointer arrays are modelled as 32-bit words (the X360 on-disk pointer width) and
// walked as u32*, matching the project's resource FixUp idiom
// (CgsGui::GuiHudMessageListResource::FixUp). Only the file/object relocation methods
// are bodied here; the mesh-level FixUp/FixDown are homed by their own ledger TUs.
namespace CgsResource
{
    // One serialised mesh.
    //
    // FLAG (x64 native-8 fork): our .apt bundles are GUIAPT64 "1:7:8" (8-byte pointers), so the mesh
    // header is WIDENED vs the console's 24-byte 4-byte-pointer layout. LAYOUT verified byte-for-byte
    // from the loaded bundle (crash-dump memory of TITLE_SCREEN02 + B5HelperComponents shape meshes):
    //   +0x00 miMeshType(4)  +0x04 miTextureMode(4)  +0x08 miTextureId(4)  +0x0C pad(4)
    //   +0x10 mpTexture(8, an already-resolved GuiTexture* -- the converter widened + relocated it)
    //   +0x18 muNumberOfVerticies(4)  +0x1C pad(4)
    //   +0x20 mppVerticies(8, the vertex pointer TABLE -- a FILE-RELATIVE OFFSET until rebased)
    //   total sizeof == 0x28. (Console: miMeshType@0/miTextureMode@4/miTextureId@8/mpTexture@0xC/
    //   muNumberOfVerticies@0x10/mppVerticies@0x14, sizeof 0x18 -- 4-byte pointers.)
    struct GuiGeometryMesh
    {
        s32       miMeshType;          // +0x00 (mesh header)
        s32       miTextureMode;       // +0x04
        s32       miTextureId;         // +0x08
        u32       muPad0;              // +0x0C (native-8 alignment pad before the 8-byte pointer)
        uintptr_t mpTexture;           // +0x10 (GuiTexture*, 8 bytes, already a live pointer)
        u32       muNumberOfVerticies; // +0x18
        u32       muPad1;              // +0x1C (pad)
        uintptr_t mppVerticies;        // +0x20 (GuiVertex** table, 8 bytes; file offset until rebased)
    };

    // The vertex pointer table entries + the file/mesh table entries are 8-byte-strided on native-8
    // (each a 4-byte offset followed by 4 pad bytes). Modelled as this cell so the rebase + the
    // render walk index the table with the correct 8-byte stride.
    struct GuiGeometryPtrTableEntry
    {
        u32 muOffsetOrPtrLo;   // the 4-byte file-relative offset (rebased in place to the low half)
        u32 muPad;             // +4 pad (the high half after rebase; offsets fit in 32 bits)
    };

    struct GuiGeometryFile
    {
        u32       muID;                // +0x00
        u32       muNumberOfMeshes;    // +0x04
        uintptr_t mppGeometryMeshes;   // +0x08 (GuiGeometryMesh** table, 8 bytes; offset until rebased)

        // The console relocate methods (@0x828500E0 / @0x82858308). FLAG (x64 native-8 fork): these
        // are the CONSOLE 4-byte-stride walk; on native-8 the tables are 8-byte-strided, so they do
        // NOT rebase this layout correctly -- they are reached ONLY by the GATED, x64-OFF in-place
        // AptDataHeader relocate (KB_APT_DATAHEADER_INPLACE_FIXUP == false), never at runtime on x64.
        // The runtime native-8 rebase is AptFixupGeometryFileNative8 (AptCharacterAnimation.cpp).
        // Kept declared+bodied so the gated console path compiles.
        GuiGeometryFile* FixUp(u32 luDelta);
        GuiGeometryFile* FixDown(u32 luDelta);
    };

    struct GuiGeometryObject
    {
        u32       muNumberOfFiles;       // +0x00
        u32       muNumberOfTexturePages;// +0x04
        uintptr_t mppGeometryFiles;      // +0x08 (GuiGeometryFile** table, 8 bytes)

        // Console relocate (@0x82852FC8 / @0x8285B828); gated-off on x64 (see GuiGeometryFile above).
        GuiGeometryObject* FixUp(u32 luDelta);
        GuiGeometryObject* FixDown(u32 luDelta, bool lbEndianSwap);
    };
}
