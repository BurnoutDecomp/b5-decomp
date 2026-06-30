#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/texture.h"

// X360 (Xenon) XGRAPHICS / D3D9 entry points the renderengine::PixelBuffer (render-target /
// depth-stencil surface) paths call. These are XDK platform externals (the EDRAM-surface
// header layout helpers and the engine's thin D3DDevice resolve/tiling helpers); they are
// declared here with their X360 ABI signatures so the reconstructed bodies compile and link
// store-for-store against the same calls the X360 image makes. On a non-X360 build they
// resolve to the platform shim layer; the declarations alone are what the compile gate
// exercises.
//
// HONEST PLACEHOLDER: these are external platform APIs with no Burnout-side home; only the
// signatures the call sites need are declared. They are NOT reconstructed here.

namespace renderengine
{
    // XGSurfaceSize: the EDRAM tile footprint (in tiles) of a surface of the given geometry,
    // used to sub-allocate the running EDRAM allocator. (Width, Height, Format, MultiSampleType.)
    u32 XGSurfaceSize(int liWidth, int liHeight, int liFormat, int liMultiSampleType);

    // XGSetSurfaceHeader: lay out a GPU surface header in place. pHierarchicalZSize/pParameters
    // are filled out by the call; pSurface is the header block being written.
    void XGSetSurfaceHeader(int liWidth, int liHeight, int liFormat, int liMultiSampleType,
                            int* lpParameters, PixelBuffer::SurfaceHeader* lpSurface,
                            u32* lpHierarchicalZSize);

    // D3DDevice helpers (thin engine wrappers over the GPU command stream).
    void D3DDevice_SetRenderTarget(void* lpDevice, u32 luRenderTargetIndex,
                                   PixelBuffer::SurfaceHeader* lpRenderTarget);
    void D3DDevice_SetDepthStencilSurface(void* lpDevice, PixelBuffer::SurfaceHeader* lpZStencil);

    // D3DDevice_EndTiling: end predicated tiling, resolving the tiled surface.
    int D3DDevice_EndTiling(void* lpDevice, u32 luFlags, void* lpResolveRects, Texture* lpDestTexture,
                            s32 liDestLevel, s32 liDestSlice, s32 liClearColour, s32 liUnused,
                            f32 lfClearZ);

    // D3DDevice_Resolve: resolve a render target into a linear destination texture.
    int D3DDevice_Resolve(void* lpDevice, u32 luFlags, const u32* lpSourceRect, Texture* lpDestTexture,
                          const void* lpDestPoint, s32 liDestLevel, s32 liDestSlice,
                          s32 liClearColour, f32 lfClearZ);

    // The device pointer the X360 image reads from off_83271608 (shared with the VB shims).
    extern void* gpD3DDevice;

    // The X360 "tiling enabled" global the resolve path checks (dword_8327161C); when set and the
    // surface is the active render target the resolve goes through D3DDevice_EndTiling.
    extern int gXbox2TilingEnabled;

    // The two running EDRAM allocators the surface Initialize sub-allocates from
    // (dword_832716C0 = tile cursor, capped at 0x800; dword_832716C4 = hi-Z cursor, capped 0xE10).
    extern u32 gXbox2EDRAMTileCursor;
    extern u32 gXbox2EDRAMHierZCursor;

    // The engine-global canonical surface-tag blocks the resolve path compares the surface against
    // to decide whether it is the currently-bound colour / depth target (X360 &unk_83271100 /
    // &unk_83271140). Opaque platform blocks; the resolve only takes their address for the compare.
    extern const void* gpColourSurfaceTag;
    extern const void* gpDepthSurfaceTag;
}
