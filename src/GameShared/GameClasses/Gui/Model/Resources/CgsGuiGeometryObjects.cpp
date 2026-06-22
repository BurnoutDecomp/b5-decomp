#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Load-/unload-time pointer relocation for
// the gui-geometry resource tree (object -> files -> meshes -> vertices). Every stored
// pointer is a 32-bit offset-from-base; FixUp adds the load delta as it descends, and
// FixDown subtracts it back to offsets as it descends. The descend/rebase order is
// faithful to the X360 pseudocode (FixUp rebases the parent table, then the child;
// FixDown rebases the children, then the parent table). Pointer tables are walked as
// u32* (the on-disk pointer width), matching the project resource FixUp idiom.

namespace CgsResource
{
    // @ 0x828500E0 - rebase one file's mesh tree by adding luDelta. Parent-then-child
    // order: bring the mesh pointer table into range, then each mesh's vertex table,
    // then each vertex pointer.
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
                    reinterpret_cast<GuiGeometryMesh*>(lpaMeshPointers[luMeshIter]);

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

    // @ 0x82858308 - un-relocate one file's mesh tree by subtracting luDelta. Child-then-
    // parent order: each vertex pointer, then the mesh's vertex table, then the mesh
    // pointer entry, finally the mesh pointer table.
    GuiGeometryFile* GuiGeometryFile::FixDown(u32 luDelta)
    {
        if (muNumberOfMeshes != 0)
        {
            u32* lpaMeshPointers = reinterpret_cast<u32*>(mppGeometryMeshes);
            for (u32 luMeshIter = 0; luMeshIter < muNumberOfMeshes; ++luMeshIter)
            {
                GuiGeometryMesh* lpMesh =
                    reinterpret_cast<GuiGeometryMesh*>(lpaMeshPointers[luMeshIter]);

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

    // @ 0x82852FC8 - rebase the whole object by adding luDelta. Parent-then-child:
    // bring the file pointer table into range, then each file pointer, then descend.
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
                reinterpret_cast<GuiGeometryFile*>(lpaFilePointers[luFileIter])->FixUp(luDelta);
            }
        }
        return this;
    }

    // @ 0x8285B828 - un-relocate the whole object by subtracting luDelta. Child-then-
    // parent: descend each file, subtract its pointer entry, then the file pointer table.
    GuiGeometryObject* GuiGeometryObject::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        (void)lbEndianSwap;   // carried for resource-interface parity; not used by the walk

        if (muNumberOfFiles != 0)
        {
            u32* lpaFilePointers = reinterpret_cast<u32*>(mppGeometryFiles);
            for (u32 luFileIter = 0; luFileIter < muNumberOfFiles; ++luFileIter)
            {
                reinterpret_cast<GuiGeometryFile*>(lpaFilePointers[luFileIter])->FixDown(luDelta);
                lpaFilePointers[luFileIter] -= luDelta;
            }
        }
        mppGeometryFiles -= luDelta;
        return this;
    }
}
