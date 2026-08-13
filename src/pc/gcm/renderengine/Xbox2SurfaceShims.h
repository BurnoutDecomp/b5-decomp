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

    // ---------------------------------------------------------------------------------------------
    // THE PREDICATED-TILING / EDRAM-RESOLVE FOUR, with the ARGUMENT ORDER DECODED (2026-08-13,
    // post-fx frame-bracket wave). EndTiling and Resolve were already declared here; their PARAMETER
    // LISTS are corrected in place, not duplicated. BeginTiling and SetPredication are new and belong
    // beside them rather than as a second extern "C" set at global scope in a game TU.
    //
    // TWO ABI FACTS DRIVE ALL FOUR. Cited addresses are in BURNOUT_X360_ARTIST.XEX.
    //
    // FACT 1 -- inside r3..r10, a FLOAT argument consumes its positional GPR and leaves it UNWRITTEN.
    // D3DDevice_EndTiling shows it at both of its attested call sites: r8 is never written in the
    // call block (it still holds a stale member displacement -- 0xC4DC in
    // BrnRendererModule::ResolveMSAA @0x823FFD3C-0x823FFD5C, and likewise in
    // PixelBuffer::Xbox2ResolveTo @0x82B6239C-0x82B623BC), ClearZ rides f1, and the argument AFTER
    // ClearZ lands in r9. IDA's own "pParameters" comment on r9 is therefore off by one slot; the
    // real r9 is ClearStencil and r10 is pParameters. The same rule fixes D3DDevice_BeginTiling,
    // whose Hex-Rays 7th argument "50396" is just that stale r8 displacement showing through.
    //
    // FACT 2 -- PAST r10, an FPR-passed argument reserves NO stack slot; the rest pack from r1+0x58.
    // The outgoing-parameter area is anchored at r1+0x18 with eight 8-byte GPR homes, shown by
    // FRAME-SIZE INVARIANCE across four call sites with four different frames -- postfx::Target::
    // Resolve @0x823F9118 (frame 0x70), postfx::RenderTarget::Resolve @0x823F9338 (0x80),
    // PixelBuffer::Xbox2ResolveTo @0x82B62300 (0xE0) and BrnRendererModule::ResolveMSAA @0x823FFBE0
    // (0xF0) all write their overflow arguments at the same ABSOLUTE r1+0x5C and r1+0x64. Only a
    // fixed base can do that. (0x5C/0x64 rather than 0x58/0x60 because a 32-bit `stw` into an 8-byte
    // big-endian slot is right-justified at slot+4.) That the two postfx callers put their LAST
    // argument in the FIRST overflow slot while ClearZ rides f1 is what shows ClearZ reserves
    // nothing there.
    //
    // AND THE VALUE CHAIN NAMES THE OVERFLOW ARGUMENT: Xbox2ResolveTo reads one stack-passed value
    // (`lwz r9, 0xE0+arg_5C` @0x82B623A0) and hands it to EndTiling's r9 on one branch and stores it
    // into Resolve's own 0x5C (@0x82B62444) on the other. ResolveMSAA sends its stencil argument to
    // exactly those two positions. Two unrelated functions, one pairing: EndTiling's r9 ==
    // Resolve's argument 10 == ClearStencil.
    //
    // RETURN TYPES. D3DDevice_EndTiling and D3DDevice_Resolve KEEP the committed `int` -- this edit
    // changes their parameter lists only. No attested caller reads r3 from either of them
    // (BrnRendererModule::ResolveMSAA tail-branches `b __restgprlr_23` straight after its EndTiling
    // call; PixelBuffer::Xbox2ResolveTo discards both and returns its own constant 1), so the return
    // is unobservable from this image and there is no evidence on which to change it. The two NEW
    // declarations are `void`: r3 is likewise discarded at every attested call site, and for a
    // declaration that has to be written from scratch `void` is the honest shape for "nothing here
    // observes a result". That is a DECLARATION CHOICE under absent evidence, not a recovered fact.
    //
    // NONE OF THE FOUR HAS A D3D9 COUNTERPART. BeginTiling/EndTiling open and close a pass that
    // REPLAYS the frame's command stream once per EDRAM tile; SetPredication selects which tile's
    // replay a command joins; Resolve copies (and MSAA-downsamples) an EDRAM surface out to a
    // sampleable texture, optionally clearing the EDRAM behind it. None of them is DEFINED anywhere
    // in this tree -- see the BRN_ANTIALIAS_BRACKET_AVAILABLE banner in
    // GameSource/Graphics/BrnRendererModule.cpp, which is what keeps that fact from becoming a
    // LNK2019, and which lists what each PC definition would owe.
    // ---------------------------------------------------------------------------------------------

    // D3DDevice_BeginTiling: open a predicated-tiling pass over luCount tile rectangles, clearing
    // each tile to lpClearColour / lfClearZ / luClearStencil as it opens.
    // Attested call site: BrnRendererModule::BeginRenderAntiAliased @0x823FFA18, asm 0x823FFB44-
    // 0x823FFB60 -- r3 pDevice, r4 Flags(0), r5 Count(2), r6 pTileRects, r7 pClearColor, f1 ClearZ,
    // r9 ClearStencil (`clrlwi r9, r27, 24` @0x823FFB44). r8 and r10 are both left stale, so this
    // takes exactly SEVEN arguments -- there is no pParameters on BeginTiling.
    void D3DDevice_BeginTiling(void* lpDevice, u32 luFlags, u32 luCount, const void* lpTileRects,
                               const void* lpClearColour, f32 lfClearZ, u32 luClearStencil);

    // D3DDevice_EndTiling: end predicated tiling, resolving the tiled surface.
    // PARAMETER LIST CORRECTED (return type unchanged). Was (pDevice, Flags, pResolveRects,
    // pDestTexture, liDestLevel, liDestSlice, liClearColour, liUnused, ClearZ) -- three of those
    // parameters do not exist, the clear colour is a pointer, and the trailing float was in the
    // wrong position. Real order, from FACT 1 above:
    int D3DDevice_EndTiling(void* lpDevice, u32 luResolveFlags, const void* lpResolveRects,
                            Texture* lpDestTexture, const void* lpClearColour, f32 lfClearZ,
                            u32 luClearStencil, const void* lpParameters);

    // D3DDevice_Resolve: resolve a render target into a linear destination texture.
    // PARAMETER LIST CORRECTED (return type unchanged). Was nine parameters ending at ClearZ, with
    // r10 typed `s32 liClearColour`; r10 is a POINTER (the attested call sites pass either an
    // address or null) and two further arguments ride the overflow area. Real order, from FACTS 1
    // and 2 above:
    int D3DDevice_Resolve(void* lpDevice, u32 luFlags, const void* lpSourceRect,
                          Texture* lpDestTexture, const void* lpDestPoint, u32 luDestLevel,
                          u32 luDestSliceOrFace, const void* lpClearColour, f32 lfClearZ,
                          u32 luClearStencil, const void* lpParameters);

    // D3DDevice_SetPredication: select which EDRAM tile's replay the following commands join (two
    // bits per tile; 0 == unpredicated, i.e. submit to every tile).
    // Attested call sites: BeginRenderAntiAliased @0x823FFB74 (mask 0) and ResolveMSAA @0x823FFC8C
    // (mask 3 << 2*tile).
    void D3DDevice_SetPredication(void* lpDevice, u32 luPredicationMask);

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
