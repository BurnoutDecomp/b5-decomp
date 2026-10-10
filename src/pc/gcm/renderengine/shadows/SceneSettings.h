#pragma once
#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "pc/gcm/renderengine/GraphicsSettings.h"

namespace CgsPC::Shadows
{
    // FLAG PC-platform leaf: optional caster policies reuse the native capture
    // distance/LOD controls; original shadow switches and atlas settings stay
    // authoritative. Relative cutoffs follow the live shadow view distance.
    inline Reflections::ObjectSettings& SmallObjects()
    {
        static Reflections::ObjectSettings sSettings = [] {
            Reflections::ObjectSettings lSettings;
            lSettings.mfDrawDistanceScale = 1;
            lSettings.mLod.miMode = renderengine::E_ENVIRONMENT_MAP_LOD_RELATIVE;
            lSettings.mLod.mfDistanceScale = 1;
            return lSettings;
        }();
        return sSettings;
    }
    inline Reflections::ObjectSettings& Debris()
    { static Reflections::ObjectSettings sSettings; return sSettings; }
    inline f32& SmallObjectRadius() { static f32 sfRadius = 2; return sfRadius; }
    inline f32 NormalDistance() { return renderengine::GetGraphicsSettingsPC().mfShadowDistance; }
    template<class ModelType>
    inline bool IsSmallObject(const ModelType* lpModel)
    {
        if (!SmallObjects().mbEnabled || !lpModel) return false;
        for (u32 lu = 0; lu < lpModel->GetNumLods(); ++lu)
            if (lpModel->DoesStateExist(static_cast<typename ModelType::State>(lu)))
            {
                const auto* lpRenderable = lpModel->GetRenderable(static_cast<typename ModelType::State>(lu));
                return lpRenderable && lpRenderable->mBoundingSphere.w > 0
                    && lpRenderable->mBoundingSphere.w <= SmallObjectRadius();
            }
        return false;
    }
}
