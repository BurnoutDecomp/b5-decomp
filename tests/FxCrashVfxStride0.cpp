// FX-CRASHVFX C3 (crash parity 2026-09-25, CC-15): THE STRIDE A STRIDE-0 FAST-SET STREAM IS DRAWN AT.
//
// run_fxcrashvfx_stride0.py hands this fixture the PRODUCTION DeclTypeBytes / DeclarationStream0Extent from the
// revision's pc/gcm/renderengine/XenonD3D9Shims.cpp (the FLAG PC platform leaf ResolveFastSetStrideFromShader reads
// the bound declaration through them) and checks them on the declarations the fast-set path meets:
//   * the debris meshes' WorldTexturedVertex -- the declaration ImRenderer<WorldTexturedVertex>::Construct builds
//     (element words 0x1A23A6 FLOAT4 @0, 0x2A23B9 FLOAT3 @16, 0x2C23A5 FLOAT2 @28): 36, which is what each ported
//     PARTICLES.BUNDLE collection's vertex run divides into (muNumVertices x 36 == the VertexBuffer's bytes);
//   * the Lion particle vertex (FLOAT4 @0, D3DCOLOR @16, FLOAT4 @20): 36, the stride the live [lionfx] line reports;
//   * a second stream's elements do not count; D3DDECL_END ends the walk whatever the count says; an empty
//     declaration gives 0 (and the draw's own early-out then skips);
//   * every D3DDECLTYPE's byte size, as d3d9types.h documents it.
#include "BrnCommonTypes.h"

#include <d3d9.h>
#include <cstdio>
#include <string>

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lrLabel.c_str());
    }
    else
    {
        std::printf("pass  %s\n", lrLabel.c_str());
    }
}

namespace renderengine
{
#include "fxcrashvfx_stride0_body.inc"   // the PRODUCTION DeclTypeBytes + DeclarationStream0Extent
}

static D3DVERTEXELEMENT9 Element(WORD luStream, WORD luOffset, BYTE luType, BYTE luUsage)
{
    D3DVERTEXELEMENT9 lElement = { luStream, luOffset, luType, D3DDECLMETHOD_DEFAULT, luUsage, 0 };
    return lElement;
}

int main()
{
    const D3DVERTEXELEMENT9 lEnd = D3DDECL_END();

    const D3DVERTEXELEMENT9 laWorldTextured[] = {
        Element(0, 0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION),
        Element(0, 16, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL),
        Element(0, 28, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD), lEnd };
    const u32 luWorldTextured = renderengine::DeclarationStream0Extent(laWorldTextured, 4);
    Check(luWorldTextured == 36u, "WorldTexturedVertex (FLOAT4 @0, FLOAT3 @16, FLOAT2 @28) -> 36, got "
                                  + std::to_string(luWorldTextured));

    const D3DVERTEXELEMENT9 laLion[] = {
        Element(0, 0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION),
        Element(0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR),
        Element(0, 20, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD), lEnd };
    const u32 luLion = renderengine::DeclarationStream0Extent(laLion, 4);
    Check(luLion == 36u, "the Lion particle vertex (FLOAT4 @0, D3DCOLOR @16, FLOAT4 @20) -> 36, got "
                         + std::to_string(luLion));

    const D3DVERTEXELEMENT9 laTwoStreams[] = {
        Element(1, 0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD),
        Element(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION),
        Element(1, 16, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD), lEnd };
    const u32 luTwo = renderengine::DeclarationStream0Extent(laTwoStreams, 4);
    Check(luTwo == 12u, "only stream 0's elements count (stream 1 FLOAT4 @0/@16, stream 0 FLOAT3 @0) -> 12, got "
                        + std::to_string(luTwo));

    const D3DVERTEXELEMENT9 laPastEnd[] = {
        Element(0, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_POSITION), lEnd,
        Element(0, 64, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD) };
    const u32 luPastEnd = renderengine::DeclarationStream0Extent(laPastEnd, 3);
    Check(luPastEnd == 8u, "D3DDECL_END ends the walk even when the count runs past it -> 8, got "
                           + std::to_string(luPastEnd));

    const D3DVERTEXELEMENT9 laEmpty[] = { lEnd };
    Check(renderengine::DeclarationStream0Extent(laEmpty, 1) == 0u, "an empty declaration -> 0 (the draw skips)");

    // d3d9types.h: FLOAT1..4 = 4/8/12/16, D3DCOLOR 4, UBYTE4 4, SHORT2 4, SHORT4 8, UBYTE4N 4, SHORT2N 4,
    // SHORT4N 8, USHORT2N 4, USHORT4N 8, UDEC3 4, DEC3N 4, FLOAT16_2 4, FLOAT16_4 8, UNUSED 0.
    const struct { u32 muType; u32 muBytes; } laSizes[] = {
        { D3DDECLTYPE_FLOAT1, 4 }, { D3DDECLTYPE_FLOAT2, 8 }, { D3DDECLTYPE_FLOAT3, 12 }, { D3DDECLTYPE_FLOAT4, 16 },
        { D3DDECLTYPE_D3DCOLOR, 4 }, { D3DDECLTYPE_UBYTE4, 4 }, { D3DDECLTYPE_SHORT2, 4 }, { D3DDECLTYPE_SHORT4, 8 },
        { D3DDECLTYPE_UBYTE4N, 4 }, { D3DDECLTYPE_SHORT2N, 4 }, { D3DDECLTYPE_SHORT4N, 8 },
        { D3DDECLTYPE_USHORT2N, 4 }, { D3DDECLTYPE_USHORT4N, 8 }, { D3DDECLTYPE_UDEC3, 4 }, { D3DDECLTYPE_DEC3N, 4 },
        { D3DDECLTYPE_FLOAT16_2, 4 }, { D3DDECLTYPE_FLOAT16_4, 8 }, { D3DDECLTYPE_UNUSED, 0 } };
    std::string lBad;
    for (const auto& lrSize : laSizes)
        if (renderengine::DeclTypeBytes(lrSize.muType) != lrSize.muBytes)
            lBad += " type " + std::to_string(lrSize.muType) + " -> " + std::to_string(renderengine::DeclTypeBytes(lrSize.muType));
    Check(lBad.empty(), "every D3DDECLTYPE's byte size as d3d9types.h documents it" + lBad);

    std::printf("FxCrashVfxStride0: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
