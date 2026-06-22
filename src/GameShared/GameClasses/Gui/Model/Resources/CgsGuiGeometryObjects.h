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
    // One serialised mesh. The leading mesh header is three ints plus a texture
    // pointer (16 bytes); the vertex pointer table follows the vertex count. Only the
    // fields the file-level relocation touches (vertex count + vertex pointer table)
    // are accessed by name here.
    struct GuiGeometryMesh
    {
        s32 miMeshType;          // +0  (mesh header)
        s32 miTextureMode;       // +4
        s32 miTextureId;         // +8
        u32 mpTexture;           // +12 (GuiTexture*, serialised as a 32-bit pointer)
        u32 muNumberOfVerticies; // +16
        u32 mppVerticies;        // +20 (GuiVertex**, serialised 32-bit pointer table)
    };

    struct GuiGeometryFile
    {
        u32 muID;                // +0
        u32 muNumberOfMeshes;    // +4
        u32 mppGeometryMeshes;   // +8  (GuiGeometryMesh**, serialised 32-bit pointer table)

        // Rebase this file's mesh tree. luDelta is the load base (FixUp) / unload base
        // (FixDown); lbEndianSwap is carried for parity with the resource interface but
        // is not consulted by the relocation walk.
        GuiGeometryFile* FixUp(u32 luDelta);
        GuiGeometryFile* FixDown(u32 luDelta);
    };

    struct GuiGeometryObject
    {
        u32 muNumberOfFiles;       // +0
        u32 muNumberOfTexturePages;// +4
        u32 mppGeometryFiles;      // +8  (GuiGeometryFile**, serialised 32-bit pointer table)

        GuiGeometryObject* FixUp(u32 luDelta);
        GuiGeometryObject* FixDown(u32 luDelta, bool lbEndianSwap);
    };
}
