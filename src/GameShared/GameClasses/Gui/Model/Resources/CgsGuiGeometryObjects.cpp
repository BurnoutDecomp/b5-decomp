#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Load-/unload-time pointer relocation for the
// gui-geometry resource tree {object -> files -> meshes -> vertices}. Every stored pointer is a
// 32-bit offset-from-base; FixUp adds the load delta as it descends, FixDown subtracts it back.
// The descend/rebase order is faithful to the X360 pseudocode (FixUp: parent table then child;
// FixDown: children then parent table).
//
// FLAG (x64 native-8 fork): our shipped .apt is GUIAPT64 "1:7:8" (8-byte pointers), so the geometry
// tree is WIDENED -- the file/mesh/vertex pointer TABLES are 8-byte-strided (a 4-byte offset + 4 pad
// per entry) and the in-struct pointer fields are 8 bytes. These CONSOLE bodies walk the tables as
// 4-byte u32* and so do NOT rebase the native-8 layout correctly. They are reached ONLY by the GATED
// in-place AptDataHeader relocate (CgsAptDataHeader.cpp, KB_APT_DATAHEADER_INPLACE_FIXUP == false),
// which is OFF on x64 -- so they never run at runtime here. The runtime native-8 rebase that DOES run
// is AptFixupGeometryFileNative8 (AptCharacterAnimation.cpp), driven from AptResolveShapeGeometry.
// These bodies are kept (compiling against the widened struct) so the gated console path builds; they
// are the faithful console form, reinstated for a future 4-byte-pointer standalone GuiGeometry resource.

namespace CgsResource
{
    // @0x828500E0 - rebase one file's mesh tree by adding luDelta (console 4-byte-stride walk).
    GuiGeometryFile* GuiGeometryFile::FixUp(u32 luDelta)
    {
        const u32 luMeshCount = muNumberOfMeshes;
        mppGeometryMeshes += luDelta;

        if (luMeshCount != 0)
        {
            u32* lpaMeshPointers = reinterpret_cast<u32*>(mppGeometryMeshes);
            for (u32 luMeshIter = 0; luMeshIter < luMeshCount; ++luMeshIter)
            {
                lpaMeshPointers[luMeshIter] += luDelta;
                GuiGeometryMesh* lpMesh =
                    reinterpret_cast<GuiGeometryMesh*>(static_cast<uintptr_t>(lpaMeshPointers[luMeshIter]));

                const u32 luVertexCount = lpMesh->muNumberOfVerticies;
                lpMesh->mppVerticies += luDelta;
                if (luVertexCount != 0)
                {
                    u32* lpaVertexPointers = reinterpret_cast<u32*>(lpMesh->mppVerticies);
                    for (u32 luVertexIter = 0; luVertexIter < luVertexCount; ++luVertexIter)
                        lpaVertexPointers[luVertexIter] += luDelta;
                }
            }
        }
        return this;
    }

    // @0x82858308 - un-relocate one file's mesh tree by subtracting luDelta.
    GuiGeometryFile* GuiGeometryFile::FixDown(u32 luDelta)
    {
        if (muNumberOfMeshes != 0)
        {
            u32* lpaMeshPointers = reinterpret_cast<u32*>(mppGeometryMeshes);
            for (u32 luMeshIter = 0; luMeshIter < muNumberOfMeshes; ++luMeshIter)
            {
                GuiGeometryMesh* lpMesh =
                    reinterpret_cast<GuiGeometryMesh*>(static_cast<uintptr_t>(lpaMeshPointers[luMeshIter]));

                if (lpMesh->muNumberOfVerticies != 0)
                {
                    u32* lpaVertexPointers = reinterpret_cast<u32*>(lpMesh->mppVerticies);
                    for (u32 luVertexIter = 0; luVertexIter < lpMesh->muNumberOfVerticies; ++luVertexIter)
                        lpaVertexPointers[luVertexIter] -= luDelta;
                }

                lpMesh->mppVerticies      -= luDelta;
                lpaMeshPointers[luMeshIter] -= luDelta;
            }
        }
        mppGeometryMeshes -= luDelta;
        return this;
    }

    // @0x82852FC8 - rebase the whole object by adding luDelta.
    GuiGeometryObject* GuiGeometryObject::FixUp(u32 luDelta)
    {
        const bool lbHasFiles = (muNumberOfFiles != 0);
        mppGeometryFiles += luDelta;

        if (lbHasFiles)
        {
            u32* lpaFilePointers = reinterpret_cast<u32*>(mppGeometryFiles);
            for (u32 luFileIter = 0; luFileIter < muNumberOfFiles; ++luFileIter)
            {
                lpaFilePointers[luFileIter] += luDelta;
                reinterpret_cast<GuiGeometryFile*>(
                    static_cast<uintptr_t>(lpaFilePointers[luFileIter]))->FixUp(luDelta);
            }
        }
        return this;
    }

    // @0x8285B828 - un-relocate the whole object by subtracting luDelta.
    GuiGeometryObject* GuiGeometryObject::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        (void)lbEndianSwap;   // carried for resource-interface parity; not used by the walk

        if (muNumberOfFiles != 0)
        {
            u32* lpaFilePointers = reinterpret_cast<u32*>(mppGeometryFiles);
            for (u32 luFileIter = 0; luFileIter < muNumberOfFiles; ++luFileIter)
            {
                reinterpret_cast<GuiGeometryFile*>(
                    static_cast<uintptr_t>(lpaFilePointers[luFileIter]))->FixDown(luDelta);
                lpaFilePointers[luFileIter] -= luDelta;
            }
        }
        mppGeometryFiles -= luDelta;
        return this;
    }
}
