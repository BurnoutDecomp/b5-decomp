#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"

namespace CgsResource
{
    // Native file relocation adds/subtracts a byte delta to each serialized
    // pointer, including zero. ARTIST828500E0/82858308 supplies the order.
    template<class T> static T* Relocate(T* lpPointer, u32 luDelta)
    {
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lpPointer) + luDelta);
    }
    void GuiGeometryFile::FixUp(u32 luDelta)
    {
        mppGeometryMeshes = Relocate(mppGeometryMeshes, luDelta);
        for (u32 i = 0; i < muNumberOfMeshes; ++i)
        {
            GuiGeometryMesh*& lpMesh = mppGeometryMeshes[i];
            lpMesh = Relocate(lpMesh, luDelta);
            lpMesh->mppVerticies = Relocate(lpMesh->mppVerticies, luDelta);
            for (u32 j = 0; j < lpMesh->muNumberOfVerticies; ++j)
                lpMesh->mppVerticies[j] = Relocate(lpMesh->mppVerticies[j], luDelta);
        }
    }
    void GuiGeometryFile::FixDown(u32 luDelta)
    {
        for (u32 i = 0; i < muNumberOfMeshes; ++i)
        {
            GuiGeometryMesh*& lpMesh = mppGeometryMeshes[i];
            for (u32 j = 0; j < lpMesh->muNumberOfVerticies; ++j)
                lpMesh->mppVerticies[j] = Relocate(lpMesh->mppVerticies[j], 0u - luDelta);
            lpMesh->mppVerticies = Relocate(lpMesh->mppVerticies, 0u - luDelta);
            lpMesh = Relocate(lpMesh, 0u - luDelta);
        }
        mppGeometryMeshes = Relocate(mppGeometryMeshes, 0u - luDelta);
    }
    void GuiGeometryObject::FixUp(u32 luDelta)
    {
        mppGeometryFiles = Relocate(mppGeometryFiles, luDelta);
        for (u32 i = 0; i < muNumberOfFiles; ++i)
        {
            mppGeometryFiles[i] = Relocate(mppGeometryFiles[i], luDelta);
            mppGeometryFiles[i]->FixUp(luDelta);
        }
    }
    void GuiGeometryObject::FixDown(u32 luDelta, bool lbEndianSwap)
    {
        (void)lbEndianSwap;
        for (u32 i = 0; i < muNumberOfFiles; ++i)
        {
            mppGeometryFiles[i]->FixDown(luDelta);
            mppGeometryFiles[i] = Relocate(mppGeometryFiles[i], 0u - luDelta);
        }
        mppGeometryFiles = Relocate(mppGeometryFiles, 0u - luDelta);
    }
}
