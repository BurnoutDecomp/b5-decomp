#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsGraphics
{
    TextureScopeTable gTextureScopeTable;

    // ARTIST827EC180: the allocated name copy is an original side effect, while
    // the entry itself retains the supplied name pointer. Cache+18 survives.
    void TextureScopeTable::AddTexturePurpose(TexturePurpose purpose, const char* name, TextureScope scope)
    {
        if (purpose < E_TEXTURE_PURPOSE_NONE || purpose >= E_MAX_TEXTURE_PURPOSES)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "( lePurpose < E_MAX_TEXTURE_PURPOSES ) && ( lePurpose >= E_TEXTURE_PURPOSE_NONE )",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics\\Dispatch\\CgsTextureScopeTable.h", 242);
            CgsDev::Assert::EndAssert();
        }
        const char* scan = name;
        while (*scan++) {}
        const u32 length = static_cast<u32>(scan - name);
        char* destination = new char[length];
        const char* source = name;
        char value;
        do
        {
            value = *source++;
            *destination++ = value;
        }
        while (value);

        TextureScopeTableElement& entry = mTextureScopeElements[purpose];
        entry.mpName = name;
        entry.mePurpose = purpose;
        entry.meScope = scope;
        entry.SetTexture(0);
        entry.SetTextureState(0);
        entry.mpMostRecentCopyInDispatchBin[0] = 0;
    }

    // ARTIST827EC260: seven purposes and their original per-purpose scopes.
    TextureScopeTable::TextureScopeTable()
    {
        AddTexturePurpose(E_TEXTURE_PURPOSE_NONE, "none", E_TEXTURE_SCOPE_OBJECT);
        AddTexturePurpose(E_TEXTURE_PURPOSE_AMBIENT_OCCLUSION_MAP, "AmbientOcclusionMap", E_TEXTURE_SCOPE_OBJECT);
        AddTexturePurpose(E_TEXTURE_PURPOSE_SHADOW_MAP_0, "shadowMap", E_TEXTURE_SCOPE_GLOBAL);
        AddTexturePurpose(E_TEXTURE_PURPOSE_GLASS_FRACTURE_PERSISTENT, "glassFractureTexturePersistent", E_TEXTURE_SCOPE_GLOBAL);
        AddTexturePurpose(E_TEXTURE_PURPOSE_ENVIRONMENT_MAP, "environmentMap", E_TEXTURE_SCOPE_GLOBAL);
        AddTexturePurpose(E_TEXTURE_PURPOSE_GLASS_FRACTURE, "glassFractureTexture", E_TEXTURE_SCOPE_GLOBAL);
        AddTexturePurpose(E_TEXTURE_PURPOSE_SHADOW_MAP_ANISO, "shadowMapAniso", E_TEXTURE_SCOPE_GLOBAL);
    }
}
