#pragma once
#include "pc/gcm/renderengine/ShaderConstantCache.h"
#include "pc/gcm/renderengine/SamplerStateCache.h"
#include "pc/gcm/renderengine/GeometryBindings.h"
#include "pc/gcm/renderengine/ShaderBindings.h"

namespace renderengine
{
    // FLAG PC-platform leaf: native state-block restoration bypasses the normal
    // cached writers used by the world, immediate and vehicle rendering paths.
    inline HRESULT RestoreStateBlockPC(IDirect3DStateBlock9* lpStateBlock)
    {
        const HRESULT lhResult = lpStateBlock->Apply();
        // Apply changes hardware without going through any cached setter. Even
        // on failure, no cached value can claim to describe the device anymore.
        gPCShaderConstantCache.Invalidate();
        gPCSamplerStateCache.Invalidate();
        GeometryBindingsPC::gCache.Invalidate();
        gPCShaderBindingCache.Invalidate();
        return lhResult;
    }
}
