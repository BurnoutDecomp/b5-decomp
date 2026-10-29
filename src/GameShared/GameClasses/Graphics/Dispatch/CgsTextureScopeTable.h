#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"
#include <cstddef>

namespace renderengine { class Texture; class TextureState; }

namespace CgsGraphics
{
    // DecFIGS CgsTextureScopeTable.h24..57; ARTIST ctor827EC260 registers0..6.
    enum TextureScope
    {
        E_TEXTURE_SCOPE_INVALID = -1,
        E_TEXTURE_SCOPE_MATERIAL = 0,
        E_TEXTURE_SCOPE_OBJECT = 1,
        E_TEXTURE_SCOPE_GLOBAL = 2,
        E_TEXTURE_SCOPE_PERSISTENT = 3,
        E_MAX_TEXTURE_SCOPES = 4
    };
    enum TexturePurpose
    {
        E_TEXTURE_PURPOSE_INVALID = -1,
        E_TEXTURE_PURPOSE_NONE = 0,
        E_TEXTURE_PURPOSE_AMBIENT_OCCLUSION_MAP = 1,
        E_TEXTURE_PURPOSE_SHADOW_MAP_0 = 2,
        E_TEXTURE_PURPOSE_GLASS_FRACTURE_PERSISTENT = 3,
        E_TEXTURE_PURPOSE_ENVIRONMENT_MAP = 4,
        E_TEXTURE_PURPOSE_GLASS_FRACTURE = 5,
        E_TEXTURE_PURPOSE_SHADOW_MAP_ANISO = 6,
        E_MAX_TEXTURE_PURPOSES = 7
    };
    typedef TexturePurpose E_TexturePurpose;

    // Serialized material record: ARTIST827E6830..6844 reads state at+16.
    struct Sampler
    {
        Ptr32<const char> purpose;
        u32 id;
        s16 channel;
        s16 scope;
        TexturePurpose ePurpose;
        Ptr32<renderengine::TextureState> state;
    };
    CGS_STATIC_ASSERT((sizeof(Sampler) == 20), "ARTIST serialized sampler stride");
    CGS_STATIC_ASSERT((offsetof(Sampler, state) == 16), "ARTIST serialized sampler state");

    struct TextureScopeTableElement
    {
        void SetTexture(const renderengine::Texture* texture)
        {
            if (mpTexture != texture)
            {
                mpTexture = texture;
                mpMostRecentCopyInDispatchBin[0] = 0;
            }
        }
        void SetTextureState(const renderengine::TextureState* state)
        {
            if (mpTextureState != state)
            {
                mpTextureState = state;
                mpMostRecentCopyInDispatchBin[0] = 0;
            }
        }
        const renderengine::Texture* GetTexture() const { return mpTexture; }

    private:
        friend struct TextureScopeTable;
        const char* mpName;
        TexturePurpose mePurpose;
        TextureScope meScope;
        const renderengine::Texture* mpTexture;
        const renderengine::TextureState* mpTextureState;
        const renderengine::TextureState* mpMostRecentCopyInDispatchBin[2];
    };
    CGS_STATIC_ASSERT((sizeof(TextureScopeTableElement) == 0x1C), "ARTIST texture-purpose entry");

    struct TextureScopeTable
    {
        TextureScopeTable();
        void AddTexturePurpose(TexturePurpose purpose, const char* name, TextureScope scope);

        // ARTIST8240BE3C..BEB8, over the single registry at83011A78.
        void SetTexturePurposeData(TexturePurpose purpose, const renderengine::Texture* texture)
        { mTextureScopeElements[purpose].SetTexture(texture); }
        void SetTexturePurposeTextureState(TexturePurpose purpose, const renderengine::TextureState* state)
        { mTextureScopeElements[purpose].SetTextureState(state); }
        const renderengine::Texture* GetTexture(TexturePurpose purpose) const
        { return mTextureScopeElements[purpose].GetTexture(); }

    private:
        TextureScopeTableElement mTextureScopeElements[E_MAX_TEXTURE_PURPOSES];
    };
    CGS_STATIC_ASSERT((sizeof(TextureScopeTable) == 0xC4), "ARTIST texture-purpose registry");

    // Existing storage name for the one ARTIST83011A78 registry.
    extern TextureScopeTable gTextureScopeTable;
}
