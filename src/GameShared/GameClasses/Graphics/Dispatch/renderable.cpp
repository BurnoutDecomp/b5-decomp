#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"

// Renderable::UsesTexture @ 0x827E9A68 (DecFIGS DWARF Renderable.h:164).
//
// Semantic reconstruction from the X360 asm/pseudocode -- a texture-membership predicate.
// It first scans the object-scope texture set for a direct pointer match, then asks each
// mesh's material assembly whether it samples the texture, returning on the first hit.
bool Renderable::UsesTexture(const renderengine::Texture* lpTexture) const
{
    // 1) Object-scope textures: a direct hit on any pointer bound at this renderable's scope.
    const ObjectScopeTextureInfo* lpTextureInfo = mpObjectScopeTextureInfo;
    if (lpTextureInfo != nullptr)
    {
        const u32 luNumTextures = lpTextureInfo->mu16NumTextures;
        for (u32 luTextureIndex = 0; luTextureIndex < luNumTextures; ++luTextureIndex)
        {
            if (lpTextureInfo->mppTextures[luTextureIndex] == lpTexture)
                return true;
        }
    }

    // 2) Otherwise defer to each mesh's material assembly.
    for (u32 luMeshIndex = 0; luMeshIndex < mu16NumMeshes; ++luMeshIndex)
    {
        const CgsGraphics::MaterialAssembly* lpMaterialAssembly =
            mppMeshes[luMeshIndex]->mpMaterialAssembly;
        if (lpMaterialAssembly != nullptr && lpMaterialAssembly->UsesTexture(lpTexture))
            return true;
    }

    return false;
}
