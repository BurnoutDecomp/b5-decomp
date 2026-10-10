#pragma once
#include <d3d9.h>
namespace CgsPC::Shadows
{
    // FLAG PC-platform leaf: the existing lit debris mesh renderer can emit
    // depth into a cascade without replacing that cascade's depth/scissor/bias.
    inline thread_local bool sbDrawingDebris = false;
    inline thread_local DWORD suDepthFunction = D3DCMP_LESSEQUAL, suDepthBias = 0, suSlopeBias = 0;
    inline DWORD ColourMask(DWORD luMask) { return sbDrawingDebris ? 0u : luMask; }
    inline DWORD DepthWrite(DWORD luWrite) { return sbDrawingDebris ? TRUE : luWrite; }
    inline D3DCMPFUNC DepthFunction(D3DCMPFUNC leFunction)
    { return sbDrawingDebris ? static_cast<D3DCMPFUNC>(suDepthFunction) : leFunction; }
}
