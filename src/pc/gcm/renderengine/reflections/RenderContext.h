#pragma once
#include <d3d9.h>
#include "pc/gcm/renderengine/reflections/SceneSettings.h"

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: particle/corona shaders use ordinary native clip
    // depth. Their authored main-view compares must not get reversed twice by
    // the console inverted-viewport adapter used for cube geometry.
    inline thread_local bool sbDrawingExtras = false;
    inline thread_local DWORD suExtrasClipMask = 0;
    class ExtrasScope
    {
        bool mbPrevious;
    public:
        ExtrasScope() : mbPrevious(sbDrawingExtras) { sbDrawingExtras = true; }
        ~ExtrasScope() { sbDrawingExtras = mbPrevious; }
    };
    inline D3DCMPFUNC ResolveExtrasDepth(D3DCMPFUNC leNative)
    {
        return sbDrawingExtras && leNative != D3DCMP_ALWAYS && leNative != D3DCMP_NEVER
            ? D3DCMP_LESSEQUAL : leNative;
    }
    inline DWORD ResolveExtrasDepthWrite(DWORD luWrite) { return sbDrawingExtras ? FALSE : luWrite; }
    inline D3DCULL ResolveExtrasCull(D3DCULL leCull)
    {
        if (!sbDrawingExtras || leCull == D3DCULL_NONE) return leCull;
        return leCull == D3DCULL_CW ? D3DCULL_CCW : D3DCULL_CW;
    }
    inline DWORD ResolveExtrasClipMask(DWORD luMask)
    { return sbDrawingExtras ? luMask | suExtrasClipMask : luMask; }
    inline bool AcceptParticleWorld(u32 luWorldIndex)
    {
        // FLAG PC-platform leaf: world 1 hides player effects from a bumper
        // camera; it does not hide that player's boost/exhaust from its cube.
        return luWorldIndex == 0 || (luWorldIndex == 1 && sbDrawingExtras && IncludePlayerParticles());
    }
}
