// =============================================================================
// XenonD3D9Shims.cpp  (pc/gcm/renderengine)
//
// [PC platform leaf] The Xenon D3DDevice_* fast-set surface, realised over the
// PC IDirect3DDevice9. Every project TU that drives the Xbox 360 fast-path
// binders (shadowingdevice.cpp, MeshHelper.cpp, CgsImRenderer.cpp, the
// dispatch walkers) declares these as externs with no home; this TU is that
// home, following the established Apt/GUI D3D9 backend pattern (CgsIm2d.cpp:
// direct device calls in a PC leaf).
//
// It also carries the WORLD-PASS bring-up support the mesh-dispatch flush
// (CgsGraphics::DispatchList::DispatchAllMeshes -> shadow::Device::*PC) binds
// through:
//   * the geometry stash + DrawIndexedPrimitiveUP draw path over the converted
//     world data's serialised IndexBuffer/VertexBuffer headers,
//   * a vertex-declaration cache built from the 32-bit serialised
//     VertexDescriptor images (Xenon GPU type dwords -> D3D9 decl types),
//   * the FLAGGED fallback world shader (compiled once via d3dcompiler_47)
//     used until the converted SHADERS bundle (tools/assets/shaders/out/
//     SHADERS_PC.BNDL) is loaded and the technique constant dispatch is
//     reconstructed -- a loud, temporary visual bring-up shim.
//
// FLAG [x64 data seam]: the serialised buffer headers keep the console u32
// muBaseAddress slot. The renderable fix-up currently stores low-4GB host
// pointers there (the PointerFromU32 convention); if that consumer widens,
// ResolveGuestPointer below is the single point to follow it.
// =============================================================================

#include "types.hpp"
#include "pc/gcm/renderengine/device.h"          // renderengine::gDevice
#include "pc/gcm/renderengine/VertexDescriptor.h" // renderengine::D3DVertexDeclaration (opaque)
#include "pc/gcm/renderengine/IndexBuffer.h"      // renderengine::IndexBuffer  (Xbox2CheckPhysicalMemoryFlags leaf)
#include "pc/gcm/renderengine/VertexBuffer.h"     // renderengine::VertexBuffer (Xbox2CheckPhysicalMemoryFlags leaf)
#include "pc/gcm/renderengine/texture.h"          // renderengine::Texture::mpD3DTexture (world sampler bind)
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h" // renderengine::PCSurfaceBracket_* (homed at the bottom of this TU)
#include "pc/gcm/renderengine/WorldGeometryPCLeaf.h" // the RETAINED dispatch-path geometry mirrors
#include "GameShared/GameClasses/Graphics/Dispatch/CgsXboxConditionalRenderShims.h" // the predicated-draw externs homed at the bottom of this TU
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <vector>

// The renderengine D3D device singleton alias the fast-path callers name
// (X360 off_83271608 dereferenced). Defined here; refreshed from
// renderengine::gDevice on every shim entry (device creation happens later
// than static init).
IDirect3DDevice9* gpD3DDevice = nullptr;

// The SAME device global under its NAMESPACED spelling, `renderengine::gpD3DDevice`
// (Xbox2SurfaceShims.h:45 -- "the device pointer the X360 image reads from off_83271608").
// Three reconstructed TUs name it in that form -- CgsRenderTarget.cpp,
// rw::graphics::postfx::RenderTarget's bind, and
// BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap, all of them passing it straight
// into D3DDevice_SetViewportF / D3DDevice_SetScissorRect -- and NOTHING defined it: the
// declaration's comment claims the "renderengine VertexBuffer/device TUs" own it, but neither
// does, and neither TU is on the exe source list. It is defined here, beside the unqualified
// alias, because this is where the device the Xenon fast-set surface uses actually lives.
// Refreshed by Dev() on every shim entry, exactly like its sibling.
// (Only ONE TU may define this -- if a render-target mount adds its own, drop this one.)
namespace renderengine { void* gpD3DDevice = nullptr; }

namespace
{
    inline IDirect3DDevice9* Dev()
    {
        gpD3DDevice = renderengine::gDevice;
        renderengine::gpD3DDevice = gpD3DDevice;
        return gpD3DDevice;
    }

    void LogOnce(const char* lpcKey, const char* lpcMessage)
    {
        static std::unordered_map<const char*, bool> sLogged;
        if (!sLogged[lpcKey])
        {
            sLogged[lpcKey] = true;
            CgsDev::Log::WriteToLog(lpcMessage);
        }
    }

    // ---- serialised world-buffer views (32-bit console images) --------------
    struct IndexBufferHeader32
    {
        u32 muCommon;         // +0x00: high bits select the index width
        u32 muReferenceCount; // +0x04
        u32 muFields[4];      // +0x08..0x14
        u32 muBaseAddress;    // +0x18: data pointer slot (low-4GB convention)
        u32 muSizeRounded;    // +0x1C
        u32 muIndexCount;     // +0x20
    };
    struct VertexBufferHeader32
    {
        u32 muCommon;         // +0x00
        u32 muFields[5];      // +0x04..0x14
        u32 muBaseAddress;    // +0x18: data pointer slot (low-4GB convention)
        u32 muUnused1C;       // +0x1C
        u32 muSize;           // +0x20: buffer size in bytes
        u32 muFormat;         // +0x24
    };

    // The console buffer headers carry their memory-protection class in the LOW TWO BITS of
    // muBaseAddress (RwRenderableResourceType::FixUpRenderableMesh @0x828A8968 relocates the
    // word with clrrwi/add/insrwi precisely to preserve them), so the address itself is the
    // slot with those bits cleared.
    inline const void* ResolveGuestPointer(u32 luSlot)
    {
        return reinterpret_cast<const void*>(static_cast<uintptr_t>(luSlot & ~3u));
    }

    // ---- Xenon primitive types -> D3D9 --------------------------------------
    bool MapPrimitive(u32 luXenonType, u32 luIndexCount,
                      D3DPRIMITIVETYPE* lpeType, UINT* lpuPrimCount)
    {
        switch (luXenonType)
        {
        case 1:  *lpeType = D3DPT_POINTLIST;     *lpuPrimCount = luIndexCount;          return luIndexCount != 0;
        case 2:  *lpeType = D3DPT_LINELIST;      *lpuPrimCount = luIndexCount / 2u;     return luIndexCount >= 2;
        case 3:  *lpeType = D3DPT_LINESTRIP;     *lpuPrimCount = luIndexCount - 1u;     return luIndexCount >= 2;
        case 4:  *lpeType = D3DPT_TRIANGLELIST;  *lpuPrimCount = luIndexCount / 3u;     return luIndexCount >= 3;
        case 5:  *lpeType = D3DPT_TRIANGLEFAN;   *lpuPrimCount = luIndexCount - 2u;     return luIndexCount >= 3;
        case 6:  *lpeType = D3DPT_TRIANGLESTRIP; *lpuPrimCount = luIndexCount - 2u;     return luIndexCount >= 3;
        default:
            LogOnce("prim", "[XenonD3D9] unsupported Xenon primitive type (RECTLIST/QUADLIST?) - draw skipped\n");
            return false;
        }
    }

    // ---- the world-draw stash (filled by shadow::Device::SetMeshBuffersPC) --
    const IndexBufferHeader32*  spIndexSource  = nullptr;
    const VertexBufferHeader32* spVertexSource = nullptr;
    u32                          suVertexStride = 0;
    u32                          suVertexSourceStride = 0;
    u32                          suVertexDec3nCount = 0;
    u16                          sau16VertexDec3nOffsets[16] = {};
    bool                         sbWorldDeclarationValid = false;
    // WHICH PUBLISHER bound the current stream -- the ROUTING FLAG for the draw path:
    //   false = the DISPATCH publisher (WorldVd32_GetDeclaration + WorldDraw_SetVertexSource,
    //           i.e. shadow::Device::SetMeshBuffersPC). Its buffers are the bundle's own
    //           STATIC serialised geometry, so the draw goes through the retained D3D9
    //           mirrors in WorldGeometryPCLeaf.cpp.
    //   true  = the Xenon FAST-SET publisher (D3DDevice_SetStreamSource ->
    //           WorldDraw_SetVertexSourceRaw): sky dome, immediate mode, particles,
    //           MeshHelper. That geometry is rewritten every frame, so it stays on the
    //           DrawIndexedPrimitiveUP path, which is what UP is for.
    bool                         sbVertexSourceFastSet = false;

    // Some D3D9 drivers (notably current NVIDIA drivers) do not expose
    // D3DDTCAPS_DEC3N. DrawIndexedPrimitiveUP already copies the submitted vertex
    // data, so unsupported packed normals are expanded into FLOAT3 records here
    // without touching the converted world bundle or the native AMD/Intel path.
    // (FAST-SET path only -- the dispatch path bakes the same expansion once per
    // buffer at mirror-creation time instead; see WorldGeometryPCLeaf.cpp.)
    std::vector<u8> sVertexFormatScratch;

    // ---- Xenos primitive reset ----------------------------------------------
    // The technique's rasteriser state (renderengine::RasterizerState
    // muPrimitiveResetEnable / muPrimitiveResetIndex, pushed by the X360 applier
    // @0x827E8690 through D3DDevice_SetRenderState_PrimitiveReset{Enable,Index}).
    // shadow::Device::SetMeshTechniquePC republishes it on every technique change.
    bool spResetEnabled = false;
    u32  suResetIndex   = 0xFFFFu;

    // FLAG PC-platform leaf: PRIMITIVE RESET HAS NO DIRECT3D 9 EQUIVALENT.
    // The Xenos restarts a strip -- and its winding parity -- at every index equal
    // to the reset index (PA_SU_SC_MODE_CNTL MULTI_PRIM_IB_ENA, bit 21). The world's
    // meshes are all D3DPT_TRIANGLESTRIP and every world material state enables the
    // reset with index 0xFFFF; feeding those markers to D3D9 as ordinary indices
    // drew a fan of garbage triangles anchored on vertex 65535 -- one black fan per
    // vertex buffer, radiating across the city. Primitive restart only arrives with
    // D3D10, so the strip runs are expanded here into an equivalent TRIANGLELIST:
    // identical triangles, identical winding, no marker ever reaching the device.
    //
    // This scratch now serves the FAST-SET path only, where the run really is rebuilt
    // every frame and there is nothing to retain. The DISPATCH path bakes the identical
    // expansion into a device index buffer ONCE per (buffer, run, primitive state) --
    // see WorldGeometryPCLeaf.cpp. The old "no cache, a cache would go stale when a
    // track unit is streamed out" note was the right worry and the wrong conclusion:
    // going stale is exactly what the resource pool's free path tells the mirror cache
    // about (CgsResource::Pool::FreeMemoryForResource ->
    // renderengine::WorldGeometry_OnResourceMemoryFreed), so the mirrors die with the
    // bundle memory they were built from, before anything can be loaded over it.
    std::vector<u8> sResetScratch;

    template <typename T>
    UINT ExpandStripRunsToList(const T* lpIndices, u32 luCount, T ltReset,
                               std::vector<u8>& lrScratch)
    {
        lrScratch.clear();
        UINT luTriangles = 0;
        u32  luRunStart  = 0;
        for (u32 lu = 0; lu <= luCount; ++lu)
        {
            if (lu != luCount && lpIndices[lu] != ltReset)
                continue;
            // [luRunStart, lu) is one strip run.
            for (u32 lv = luRunStart; lv + 2u < lu; ++lv)
            {
                const T lt0 = lpIndices[lv];
                const T lt1 = lpIndices[lv + 1];
                const T lt2 = lpIndices[lv + 2];
                if (lt0 == lt1 || lt1 == lt2 || lt0 == lt2)
                    continue;               // degenerate stitch triangle
                // A strip's odd triangles are wound the other way round; emitting
                // them as list triangles has to swap the first two indices back.
                const bool lbOdd = (((lv - luRunStart) & 1u) != 0u);
                const T laTriangle[3] = { lbOdd ? lt1 : lt0, lbOdd ? lt0 : lt1, lt2 };
                const u8* const lpBytes = reinterpret_cast<const u8*>(laTriangle);
                lrScratch.insert(lrScratch.end(), lpBytes, lpBytes + sizeof(laTriangle));
                ++luTriangles;
            }
            luRunStart = lu + 1u;
        }
        return luTriangles;
    }

    // ---- vertex-declaration cache over 32-bit VertexDescriptor images -------
    // On-disk image (attested by tools/assets/bundles/renderable_transcode.py
    // parse_vertex_descriptor + the TRK_UNIT285 set): 16B header with u16
    // numElements @+0x08; numElements x 16B elements {u16 stream, u16 offset,
    // u32 Xenon GPU type dword, u16 usage, u16 usageIndex, u32 1}; then one
    // stride byte per element.
    // A vs_3_0 shader that reads an input the declaration does not supply makes the
    // draw call FAIL SILENTLY on D3D9, so every shader/declaration pair has to be
    // checked before it is used. Both sides are reduced to the same 64-bit set:
    // bit (usage * 4 + min(usageIndex, 3)) for usage < 16.
    inline u64 DeclUsageBit(u32 luUsage, u32 luUsageIndex)
    {
        if (luUsage >= 16u)
            return 0;
        if (luUsageIndex > 3u)
            luUsageIndex = 3u;
        return 1ull << (luUsage * 4u + luUsageIndex);
    }

    struct Vd32Cached
    {
        IDirect3DVertexDeclaration9* mpDeclaration;
        u32                          muStride;
        u32                          muSourceStride;
        u32                          muDec3nCount;
        u16                          mau16Dec3nOffsets[16];
        // Whether the declaration exposes D3DDECLUSAGE_TEXCOORD index 0 (the fallback
        // textured variant's only extra input).
        bool                         mbHasTexcoord0;
        // Every {usage, usageIndex} the declaration supplies, as the bit set above.
        u64                          muUsageMask;
        // ---- [DIAG wheels] WHAT THE DESCRIPTOR RESOLVED TO ---------------------------
        // The POSITION0 element as the D3D9 declaration ended up describing it, plus a
        // printable rendering of the whole element list. Recorded once per descriptor image
        // and reported by the [wheel-decode] one-shot in WorldDraw_IndexedUP, which is what
        // separates "the leaf decodes the position stream through the wrong D3DDECLTYPE"
        // from "this mesh IS that shape in the shipped data" without a frame capture.
        // mu8PositionType is D3DDECLTYPE_UNUSED when the descriptor declares no POSITION0.
        u8                           mu8PositionType;
        u16                          mu16PositionSourceOffset;
        u16                          mu16PositionExpandedOffset;
        // "POSITION0:FLOAT3@0->0 NORMAL0:DEC3N@12->12 ..." (source offset -> expanded offset).
        char                         macElements[224];
    };
    std::unordered_map<const void*, Vd32Cached> sVdCache;

    // [DIAG] D3DDECLTYPE / D3DDECLUSAGE -> their D3D9 spelling, for the [wheel-decode] line.
    // Names only; nothing behavioural reads these.
    const char* DeclTypeName(u8 lu8Type)
    {
        switch (lu8Type)
        {
        case D3DDECLTYPE_FLOAT1:    return "FLOAT1";
        case D3DDECLTYPE_FLOAT2:    return "FLOAT2";
        case D3DDECLTYPE_FLOAT3:    return "FLOAT3";
        case D3DDECLTYPE_FLOAT4:    return "FLOAT4";
        case D3DDECLTYPE_D3DCOLOR:  return "D3DCOLOR";
        case D3DDECLTYPE_UBYTE4:    return "UBYTE4";
        case D3DDECLTYPE_SHORT2:    return "SHORT2";
        case D3DDECLTYPE_SHORT4:    return "SHORT4";
        case D3DDECLTYPE_UBYTE4N:   return "UBYTE4N";
        case D3DDECLTYPE_SHORT2N:   return "SHORT2N";
        case D3DDECLTYPE_SHORT4N:   return "SHORT4N";
        case D3DDECLTYPE_USHORT2N:  return "USHORT2N";
        case D3DDECLTYPE_USHORT4N:  return "USHORT4N";
        case D3DDECLTYPE_UDEC3:     return "UDEC3";
        case D3DDECLTYPE_DEC3N:     return "DEC3N";
        case D3DDECLTYPE_FLOAT16_2: return "FLOAT16_2";
        case D3DDECLTYPE_FLOAT16_4: return "FLOAT16_4";
        case D3DDECLTYPE_UNUSED:    return "UNUSED";
        default:                    return "?";
        }
    }

    const char* DeclUsageName(u8 lu8Usage)
    {
        switch (lu8Usage)
        {
        case D3DDECLUSAGE_POSITION:     return "POSITION";
        case D3DDECLUSAGE_BLENDWEIGHT:  return "BLENDWEIGHT";
        case D3DDECLUSAGE_BLENDINDICES: return "BLENDINDICES";
        case D3DDECLUSAGE_NORMAL:       return "NORMAL";
        case D3DDECLUSAGE_PSIZE:        return "PSIZE";
        case D3DDECLUSAGE_TEXCOORD:     return "TEXCOORD";
        case D3DDECLUSAGE_TANGENT:      return "TANGENT";
        case D3DDECLUSAGE_BINORMAL:     return "BINORMAL";
        case D3DDECLUSAGE_TESSFACTOR:   return "TESSFACTOR";
        case D3DDECLUSAGE_POSITIONT:    return "POSITIONT";
        case D3DDECLUSAGE_COLOR:        return "COLOR";
        case D3DDECLUSAGE_FOG:          return "FOG";
        case D3DDECLUSAGE_DEPTH:        return "DEPTH";
        case D3DDECLUSAGE_SAMPLE:       return "SAMPLE";
        default:                        return "?";
        }
    }

    // Xenon GPU vertex-format dword -> D3D9 D3DDECLTYPE.
    bool MapXenonDeclType(u32 luXenon, u8* lpu8Type)
    {
        switch (luXenon)
        {
        case 0x2C83A4: *lpu8Type = D3DDECLTYPE_FLOAT1;    return true;
        case 0x2C23A5: *lpu8Type = D3DDECLTYPE_FLOAT2;    return true;
        case 0x2A23B9: *lpu8Type = D3DDECLTYPE_FLOAT3;    return true;
        case 0x1A23A6: *lpu8Type = D3DDECLTYPE_FLOAT4;    return true;
        case 0x182886: *lpu8Type = D3DDECLTYPE_D3DCOLOR;  return true;
        case 0x1A2286: *lpu8Type = D3DDECLTYPE_UBYTE4;    return true;
        case 0x1A2086: *lpu8Type = D3DDECLTYPE_UBYTE4N;   return true;
        case 0x2C2359: *lpu8Type = D3DDECLTYPE_SHORT2;    return true;
        case 0x1A235A: *lpu8Type = D3DDECLTYPE_SHORT4;    return true;
        case 0x2C2159: *lpu8Type = D3DDECLTYPE_SHORT2N;   return true;
        case 0x1A215A: *lpu8Type = D3DDECLTYPE_SHORT4N;   return true;
        case 0x2C2059: *lpu8Type = D3DDECLTYPE_USHORT2N;  return true;
        case 0x1A205A: *lpu8Type = D3DDECLTYPE_USHORT4N;  return true;
        case 0x2A2287: *lpu8Type = D3DDECLTYPE_UDEC3;     return true;
        case 0x2A2187: *lpu8Type = D3DDECLTYPE_DEC3N;     return true;
        case 0x2C235F: *lpu8Type = D3DDECLTYPE_FLOAT16_2; return true;
        case 0x1A235F: *lpu8Type = D3DDECLTYPE_FLOAT16_4; return true;
        default: return false;
        }
    }

    // ---- the FLAGGED fallback world shader ----------------------------------
    // Compiled once through d3dcompiler_47 (loaded dynamically; no import-lib
    // dependency). Transforms by the row-vector WVP uploaded to c0..c3 and
    // shades by a screen-space-derivative face normal, so raw geometry reads as
    // 3D without any real material data. LOUD BRING-UP SHIM: replaced by the
    // converted per-technique shaders (SHADERS_PC.BNDL) when their load path +
    // constant dispatch land.
    typedef HRESULT (WINAPI* D3DCompileProc)(
        LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, const void* pDefines,
        void* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
        void** ppCode, void** ppErrorMsgs);

    struct ID3DBlobLite
    {
        // The two ID3DBlob vtable entries used (QueryInterface/AddRef/Release +
        // GetBufferPointer/GetBufferSize), reached positionally.
        virtual HRESULT __stdcall QueryInterface(const void*, void**) = 0;
        virtual ULONG   __stdcall AddRef() = 0;
        virtual ULONG   __stdcall Release() = 0;
        virtual LPVOID  __stdcall GetBufferPointer() = 0;
        virtual SIZE_T  __stdcall GetBufferSize() = 0;
    };

    // TWO variants. The UNTEXTURED pair is the original bring-up shader (flat
    // screen-space-derivative shading, POSITION only). The TEXTURED pair adds a
    // TEXCOORD0 input and samples the material's diffuse texture at s0.
    //
    // They cannot be merged: a vs_3_0 shader that declares TEXCOORD0 makes the draw
    // call fail outright on any mesh whose vertex declaration has no TEXCOORD0
    // element (D3D9 rejects the draw rather than feeding zeroes). So the pair is
    // chosen PER MESH, once its declaration is known -- see
    // WorldFallbackShader_SelectForMesh, called from shadow::Device::SetMeshBuffersPC.
    // The fallback WVP lives at c240..c243, NOT c0..c3: a mesh can fall back inside a
    // technique whose REAL programs are otherwise driving the walk (see
    // WorldFallbackShader_SelectForMesh), and the real world vertex shaders use the low
    // registers (ShadowMap_WorldToLight lands on c0 in several of them). c240+ is past
    // everything the converted set declares, so the two paths cannot clobber each other.
    const char* KPC_FALLBACK_VS =
        "float4 gWvp0 : register(c240);\n"
        "float4 gWvp1 : register(c241);\n"
        "float4 gWvp2 : register(c242);\n"
        "float4 gWvp3 : register(c243);\n"
        "struct VsOut { float4 hpos : POSITION; float3 opos : TEXCOORD0; };\n"
        "VsOut main(float4 pos : POSITION) {\n"
        "  VsOut o;\n"
        "  o.hpos = pos.x * gWvp0 + pos.y * gWvp1 + pos.z * gWvp2 + gWvp3;\n"
        "  o.opos = pos.xyz;\n"
        "  return o;\n"
        "}\n";

    const char* KPC_FALLBACK_PS =
        "float4 main(float3 opos : TEXCOORD0) : COLOR {\n"
        "  float3 n = normalize(cross(ddx(opos), ddy(opos)));\n"
        "  float l = 0.35 + 0.65 * saturate(abs(n.y) * 0.7 + (abs(n.x) + abs(n.z)) * 0.25);\n"
        "  return float4(l * 0.62, l * 0.66, l * 0.72, 1.0);\n"
        "}\n";

    const char* KPC_FALLBACK_TEX_VS =
        "float4 gWvp0 : register(c240);\n"
        "float4 gWvp1 : register(c241);\n"
        "float4 gWvp2 : register(c242);\n"
        "float4 gWvp3 : register(c243);\n"
        "struct VsOut { float4 hpos : POSITION; float3 opos : TEXCOORD0; float2 uv : TEXCOORD1; };\n"
        "VsOut main(float4 pos : POSITION, float2 uv : TEXCOORD0) {\n"
        "  VsOut o;\n"
        "  o.hpos = pos.x * gWvp0 + pos.y * gWvp1 + pos.z * gWvp2 + gWvp3;\n"
        "  o.opos = pos.xyz;\n"
        "  o.uv = uv;\n"
        "  return o;\n"
        "}\n";

    // The diffuse texel is the BASE COLOUR, modulated by the same screen-space-derivative
    // face term the flat variant uses. (Until 2026-07-28 the texel only modulated the flat
    // term through a 0.35 floor + 1.3 gain, so that a mesh could never disappear on a black
    // diffuse -- worth it while the pixel payload was suspect, but it lifts blacks and blows
    // out highlights, which now hides the real texture. The floor moved onto the LIGHTING
    // term instead: nothing goes fully black, and the albedo reads true.)
    const char* KPC_FALLBACK_TEX_PS =
        "sampler2D gDiffuse : register(s0);\n"
        "float4 main(float3 opos : TEXCOORD0, float2 uv : TEXCOORD1) : COLOR {\n"
        "  float3 n = normalize(cross(ddx(opos), ddy(opos)));\n"
        "  float l = 0.45 + 0.55 * saturate(abs(n.y) * 0.7 + (abs(n.x) + abs(n.z)) * 0.25);\n"
        "  float3 texel = tex2D(gDiffuse, uv).rgb;\n"
        "  return float4(texel * l, 1.0);\n"
        "}\n";

    // [DIAG, env-gated like BRN_WORLD_ONLY] BRN_WORLD_UVDEBUG=1 swaps the textured
    // pixel shader for one that paints the interpolated TEXCOORD0 (red = u, green = v),
    // which separates "the vertex declaration/UV lane is wrong" from "the sampled
    // texture has no content".
    const char* KPC_FALLBACK_UVDEBUG_PS =
        "float4 main(float3 opos : TEXCOORD0, float2 uv : TEXCOORD1) : COLOR {\n"
        "  return float4(frac(uv.x), frac(uv.y), 0.25, 1.0);\n"
        "}\n";

    IDirect3DPixelShader9*  spFallbackUvDebugPs = nullptr;
    IDirect3DVertexShader9* spFallbackVs = nullptr;
    IDirect3DPixelShader9*  spFallbackPs = nullptr;
    IDirect3DVertexShader9* spFallbackTexVs = nullptr;
    IDirect3DPixelShader9*  spFallbackTexPs = nullptr;
    bool                    sbFallbackCompileFailed = false;
    // Set by WorldMaterialSamplers_Bind for the current technique, consumed by
    // WorldFallbackShader_SelectForMesh once the mesh declaration is known.
    bool                    sbMaterialTextureBound = false;
    // ⭐ THE SAME QUESTION, ASKED ON THE PATH THAT ACTUALLY RUNS (2026-08-16, wheels leg 1).
    //
    // sbMaterialTextureBound above is written in exactly ONE function -- WorldMaterialSamplers_Bind
    // -- and that function is called from exactly ONE place: the `if (!lbRealBound)` early-return
    // arm of Device::SetMeshTechniquePC. On a build whose SHADERS.BNDL resolves, the real programs
    // DO bind, so that arm never executes and the flag is never set. Every mesh that then has to
    // drop back to the fallback pair (on this build: the console-instanced WHEEL renderable, and
    // only it) was told "this material has no texture" and drew with the FLAT pair -- pale
    // blue-grey, no tyre, no rim. MEASURED: the wtex one-shot never appears in a full boot log
    // while `[WorldSamplers] technique sampler tally: bound=4096 ... of 4096` says every single
    // technique sampler resolved to a live D3D texture. The texture was on the device; the
    // consumer was told it was not.
    //
    // So the REAL sampler path (Device::BindTechniqueSamplers -> WorldShader_BindTextureUnit)
    // records what it bound here, and the fallback selector reads it. Reset per technique change,
    // in both of SetMeshTechniquePC's program-bind outcomes (WorldPrograms_Bind and
    // WorldShader_ClearRealPrograms), so a technique that binds no sampler at all -- the z-only
    // walk -- cannot inherit the previous technique's texture.
    IDirect3DBaseTexture9*  spTechniqueUnit0Texture  = nullptr;  // what the technique put on s0
    IDirect3DBaseTexture9*  spTechniqueFirstTexture  = nullptr;  // the first it bound anywhere
    // Set by WorldVd32_GetDeclaration for the mesh currently being bound.
    bool                    sbLastDeclHasTexcoord0 = false;
    u64                     suLastDeclUsageMask = 0;
    u32                     suLastDeclSourceStride = 0;
    u32                     suLastDeclDec3nCount = 0;
    u16                     sau16LastDeclDec3nOffsets[16] = {};
    // The same three POSITION0 facts + the element text, for the descriptor of the mesh
    // currently being bound (see Vd32Cached's banner). spLastDeclElements points at the
    // CACHED entry's buffer, never at a local: std::unordered_map is node-based, so a
    // pointer to a mapped value stays valid for the life of that entry, and entries are
    // only ever added.
    u8                      su8LastDeclPositionType = D3DDECLTYPE_UNUSED;
    u32                     suLastDeclPositionSourceOffset = 0;
    u32                     suLastDeclPositionExpandedOffset = 0;
    const char*             spLastDeclElements = nullptr;
    // Set by WorldFallbackShader_MarkInstancedMesh (see its banner further down): the next
    // mesh is CONSOLE-INSTANCED. WorldFallbackShader_SelectForMesh turns that into a fallback
    // only when the technique's vertex program cannot be fed a `world` matrix.
    bool                    sbForceFallbackNextMesh = false;

    D3DCompileProc GetD3DCompile()
    {
        HMODULE lhCompiler = ::LoadLibraryA("d3dcompiler_47.dll");
        if (lhCompiler == nullptr)
            lhCompiler = ::LoadLibraryA("d3dcompiler_43.dll");
        return lhCompiler
            ? reinterpret_cast<D3DCompileProc>(::GetProcAddress(lhCompiler, "D3DCompile"))
            : nullptr;
    }

    void CompileOne(IDirect3DDevice9* lpDevice, D3DCompileProc lpfnCompile,
                    const char* lpcSource, const char* lpcName, bool lbPixel,
                    IDirect3DVertexShader9** lppVs, IDirect3DPixelShader9** lppPs)
    {
        ID3DBlobLite* lpCode = nullptr;
        ID3DBlobLite* lpErrors = nullptr;
        const HRESULT lhr = lpfnCompile(lpcSource, std::strlen(lpcSource), lpcName,
                                        nullptr, nullptr, "main", lbPixel ? "ps_3_0" : "vs_3_0", 0, 0,
                                        reinterpret_cast<void**>(&lpCode),
                                        reinterpret_cast<void**>(&lpErrors));
        if (SUCCEEDED(lhr) && lpCode != nullptr)
        {
            if (lbPixel)
                lpDevice->CreatePixelShader(static_cast<const DWORD*>(lpCode->GetBufferPointer()), lppPs);
            else
                lpDevice->CreateVertexShader(static_cast<const DWORD*>(lpCode->GetBufferPointer()), lppVs);
            lpCode->Release();
        }
        if (lpErrors != nullptr) lpErrors->Release();
    }

    bool CompileFallbackShaders(IDirect3DDevice9* lpDevice)
    {
        if (spFallbackVs != nullptr && spFallbackPs != nullptr)
            return true;
        if (sbFallbackCompileFailed)
            return false;

        D3DCompileProc lpfnCompile = GetD3DCompile();
        if (lpfnCompile == nullptr)
        {
            sbFallbackCompileFailed = true;
            LogOnce("d3dc", "[WorldFallback] d3dcompiler not available - world fallback shader disabled\n");
            return false;
        }

        CompileOne(lpDevice, lpfnCompile, KPC_FALLBACK_VS, "world_fallback_vs", false,
                   &spFallbackVs, nullptr);
        CompileOne(lpDevice, lpfnCompile, KPC_FALLBACK_PS, "world_fallback_ps", true,
                   nullptr, &spFallbackPs);
        CompileOne(lpDevice, lpfnCompile, KPC_FALLBACK_TEX_VS, "world_fallback_tex_vs", false,
                   &spFallbackTexVs, nullptr);
        CompileOne(lpDevice, lpfnCompile, KPC_FALLBACK_TEX_PS, "world_fallback_tex_ps", true,
                   nullptr, &spFallbackTexPs);
        CompileOne(lpDevice, lpfnCompile, KPC_FALLBACK_UVDEBUG_PS, "world_fallback_uvdebug_ps", true,
                   nullptr, &spFallbackUvDebugPs);

        if (spFallbackVs == nullptr || spFallbackPs == nullptr)
        {
            sbFallbackCompileFailed = true;
            LogOnce("d3dcf", "[WorldFallback] fallback shader compile/create FAILED - world draws skipped\n");
            return false;
        }
        if (spFallbackTexVs == nullptr || spFallbackTexPs == nullptr)
            LogOnce("d3dct", "[WorldFallback] TEXTURED fallback variant unavailable - flat shading only\n");
        else
            LogOnce("d3dcs2", "[WorldFallback] fallback world shaders compiled: flat + TEXTURED (BRING-UP SHIM)\n");
        LogOnce("d3dcs", "[WorldFallback] fallback world shader compiled + bound (BRING-UP SHIM)\n");
        return true;
    }

    // ---- the D3D9-bytecode shader cache (the converted-shader path) ---------
    // shadow::Device::SetPixelProgram / FlushVertexProgramState hand this leaf a
    // pointer to the program image at ProgramBufferData+0x14; when the converted
    // SHADERS bundle is live that memory holds D3D9 bytecode (version token
    // 0xFFFE03xx / 0xFFFF03xx). Shader objects are created once per image.
    std::unordered_map<const void*, IDirect3DVertexShader9*> sVsCache;
    std::unordered_map<const void*, IDirect3DPixelShader9*>  sPsCache;

    inline bool LooksLikeD3D9Bytecode(const void* lpShader, bool lbPixel)
    {
        if (lpShader == nullptr) return false;
        const u32 luToken = *static_cast<const u32*>(lpShader);
        return (luToken & 0xFFFF0000u) == (lbPixel ? 0xFFFF0000u : 0xFFFE0000u)
            && (luToken & 0x0000FF00u) <= 0x0300u;
    }

    // ---- the REAL per-technique shader path ---------------------------------
    // Which {usage, usageIndex} inputs a compiled vs_3_0 program actually reads, taken
    // from its own `dcl` instructions: opcode D3DSIO_DCL (0x1F) followed by a
    // declaration token (usage in bits 0..4, usage index in bits 16..19) and a
    // destination-parameter token whose register type (bits 28..30 high | 11..12 low)
    // is D3DSPR_INPUT (1). Cached per shader image.
    std::unordered_map<const void*, u64> sVsInputMaskCache;

    u64 VertexShaderInputMask(const void* lpShader)
    {
        std::unordered_map<const void*, u64>::iterator lIt = sVsInputMaskCache.find(lpShader);
        if (lIt != sVsInputMaskCache.end())
            return lIt->second;

        u64 luMask = 0;
        const u32* lpToken = static_cast<const u32*>(lpShader);
        u32 luWord = 1;                        // [0] is the version token
        for (u32 luGuard = 0; luGuard < 65536u; ++luGuard)
        {
            const u32 luInstruction = lpToken[luWord];
            if (luInstruction == 0x0000FFFFu)   // D3DSIO_END
                break;
            const u32 luOpcode = luInstruction & 0x0000FFFFu;
            u32 luLength;
            if (luOpcode == 0x0000FFFEu)        // comment block (CTAB lives here)
                luLength = (luInstruction >> 16) & 0x7FFFu;
            else
                luLength = (luInstruction >> 24) & 0x0Fu;

            if (luOpcode == 0x1Fu && luLength >= 2u)
            {
                const u32 luDeclaration = lpToken[luWord + 1];
                const u32 luDestination = lpToken[luWord + 2];
                const u32 luRegisterType = ((luDestination >> 28) & 0x07u)
                                         | ((luDestination >> 8) & 0x18u);
                if (luRegisterType == 1u)       // D3DSPR_INPUT
                {
                    luMask |= DeclUsageBit(luDeclaration & 0x1Fu,
                                           (luDeclaration >> 16) & 0x0Fu);
                }
            }
            luWord += luLength + 1u;
        }

        sVsInputMaskCache[lpShader] = luMask;
        return luMask;
    }

    // ---- the named-constant surface of a compiled D3D9 program ----------------------
    // ONE question has to be answered at BIND time: does this technique's vertex program
    // take a `world` matrix? It decides whether a console-instanced mesh can use its own
    // programs at all (see WorldFallbackShader_MarkInstancedMesh).
    //
    // The dispatch side answers the same class of question through the ShaderProgramBuffer's
    // descriptor table (ShaderConstantsExternal::FixUp -> ProgramBuffer::
    // GetVariableHandleByName). This TU cannot include programbuffer.h -- that header must
    // not precede <d3d9.h> here, see the note by the XG* leaf at the bottom of the file -- so
    // it reads the program's OWN CTAB. It is the same data: the converter BUILDS that
    // descriptor table out of this very CTAB (tools/assets/shaders/FORMAT_MAP.md section 5).
    //
    // Layout (D3DXSHADER_CONSTANTTABLE, the standard fxc comment block, little-endian): a
    // comment token (opcode 0xFFFE, dword length in bits 16..30) whose first payload dword is
    // 'CTAB'; the table header starts at the dword AFTER that FourCC and every offset in the
    // block is relative to the header's own first byte:
    //     +0x00 u32 Size (28)     +0x04 u32 Creator      +0x08 u32 Version
    //     +0x0C u32 Constants     +0x10 u32 ConstantInfo
    //     +0x14 u32 Flags         +0x18 u32 Target
    // D3DXSHADER_CONSTANTINFO, 20-byte stride:
    //     +0x00 u32 Name (offset) +0x04 u16 RegisterSet  +0x06 u16 RegisterIndex
    //     +0x08 u16 RegisterCount +0x0A u16 Reserved     +0x0C u32 TypeInfo
    //     +0x10 u32 DefaultValue
    // RegisterSet 2 == D3DXRS_FLOAT4, the only set this asks about.
    //
    // MEASURED against the shipped bundle: the four wheel vertex programs
    // (build/game/SHADERS.BNDL resources AB4DE44C / EB89C988 / 9CFA74B1 / DC3E5975) each
    // report `world` at register 20 count 4 -- the same answer `fxc /dumpbin` prints.
    bool ProgramDeclaresFloat4Constant(const void* lpShader, const char* lpcName)
    {
        if (lpShader == nullptr || lpcName == nullptr)
            return false;

        const u32* const lpToken = static_cast<const u32*>(lpShader);
        u32 luWord = 1;                                    // [0] is the version token
        for (u32 luGuard = 0; luGuard < 65536u; ++luGuard)
        {
            const u32 luInstruction = lpToken[luWord];
            if (luInstruction == 0x0000FFFFu)               // D3DSIO_END
                break;
            const u32 luOpcode = luInstruction & 0x0000FFFFu;
            u32 luLength;
            if (luOpcode == 0x0000FFFEu)                    // comment block
            {
                luLength = (luInstruction >> 16) & 0x7FFFu;
                if (luLength >= 8u && lpToken[luWord + 1] == 0x42415443u)   // 'CTAB'
                {
                    // Everything below is bounded by the comment block the token declares:
                    // its payload is luLength dwords, the first of which is the FourCC.
                    const u8* const lpTable = reinterpret_cast<const u8*>(&lpToken[luWord + 2]);
                    const u32 luTableBytes = 4u * (luLength - 1u);
                    u32 luNumConstants = 0, luConstantInfo = 0;
                    std::memcpy(&luNumConstants, lpTable + 0x0C, 4);
                    std::memcpy(&luConstantInfo, lpTable + 0x10, 4);
                    if (luConstantInfo > luTableBytes
                        || luNumConstants > (luTableBytes - luConstantInfo) / 20u)
                    {
                        LogOnce("ctabrange", "[WorldShader] a program's CTAB constant table is"
                                             " out of range - constant lookup skipped\n");
                        return false;
                    }
                    for (u32 luConstant = 0; luConstant < luNumConstants; ++luConstant)
                    {
                        const u8* const lpInfo = lpTable + luConstantInfo + 20u * luConstant;
                        u32 luNameOffset = 0;
                        u16 lu16RegisterSet = 0;
                        std::memcpy(&luNameOffset,    lpInfo + 0x00, 4);
                        std::memcpy(&lu16RegisterSet, lpInfo + 0x04, 2);
                        if (lu16RegisterSet != 2u || luNameOffset >= luTableBytes)
                            continue;
                        if (std::strcmp(reinterpret_cast<const char*>(lpTable + luNameOffset),
                                        lpcName) == 0)
                            return true;
                    }
                    return false;
                }
            }
            else
            {
                luLength = (luInstruction >> 24) & 0x0Fu;
            }
            luWord += luLength + 1u;
        }
        return false;
    }

    // Cached per vertex-program payload: the bind runs on every technique CHANGE and the
    // answer cannot change for a given payload.
    std::unordered_map<const void*, bool> sVsHasWorldCache;

    bool VertexProgramHasWorldMatrix(const void* lpShader)
    {
        std::unordered_map<const void*, bool>::iterator lIt = sVsHasWorldCache.find(lpShader);
        if (lIt != sVsHasWorldCache.end())
            return lIt->second;
        const bool lbHasWorld = ProgramDeclaresFloat4Constant(lpShader, "world");
        sVsHasWorldCache[lpShader] = lbHasWorld;
        return lbHasWorld;
    }

    // ---- [DIAG] THE PER-TECHNIQUE FALLBACK SURVEY -----------------------------------
    // One line per (technique, reason) the FIRST time a technique fails to draw with its own
    // programs, so one boot log lists every "shader that is not applying" instead of the
    // single LogOnce line the first failure used to swallow.
    //
    // Keyed on the technique NAME POINTER: that string lives in the streamed ShaderTechnique
    // blob (+148), is unique per technique and costs nothing to hash.
    // WARNING when reading the log: the first CHARACTER of every technique name is the
    // shader-PROFILE digit CgsResource::ShaderTechniqueResourceType::PostFixUp stamps over it
    // ('0' + code) -- the console does exactly the same -- so a line reads
    // "0ehicle_1Bit_Tyre_Textured_Default", not "Vehicle_...".
    // DELETE-WHEN the boot log's survey is empty (or only names techniques with no PC source).
    enum EWorldFallbackReason
    {
        E_WORLD_FALLBACK_NO_PROGRAM         = 0,
        E_WORLD_FALLBACK_NOT_D3D9_BYTECODE  = 1,
        E_WORLD_FALLBACK_CREATE_FAILED      = 2,
        E_WORLD_FALLBACK_INSTANCED_NO_WORLD = 3,
        E_WORLD_FALLBACK_DECLARATION_SHORT  = 4,
        E_WORLD_FALLBACK_REASON_COUNT       = 5,
    };
    const char* const KapcWorldFallbackReasons[E_WORLD_FALLBACK_REASON_COUNT] =
    {
        "no usable program pair (missing import or null payload)",
        "program payload is not D3D9 bytecode",
        "Create{Vertex,Pixel}Shader failed",
        "console-instanced mesh and the technique vertex program declares no `world`",
        "mesh declaration lacks an input the technique vertex program reads",
    };
    std::unordered_map<const void*, u32> sFallbackSurvey;   // name pointer -> reasons reported

    void WorldShader_ReportFallback(const char* lpcTechniqueName, u32 luReason)
    {
        if (luReason >= static_cast<u32>(E_WORLD_FALLBACK_REASON_COUNT))
            return;
        u32& lruReported = sFallbackSurvey[lpcTechniqueName];
        if ((lruReported & (1u << luReason)) != 0u)
            return;
        lruReported |= (1u << luReason);

        char lacMsg[320];
        std::snprintf(lacMsg, sizeof(lacMsg), "[WorldShader] FALLBACK %s reason=%s\n",
                      (lpcTechniqueName != nullptr) ? lpcTechniqueName : "<unnamed technique>",
                      KapcWorldFallbackReasons[luReason]);
        CgsDev::Log::WriteToLog(lacMsg);
    }

    // The fallback shader's WVP register base (see the KPC_FALLBACK_VS note).
    const u32 KU_FALLBACK_WVP_REGISTER = 240u;

    // The technique's real programs, as chosen by the last SetMeshTechniquePC.
    IDirect3DVertexShader9* spRealVs = nullptr;
    IDirect3DPixelShader9*  spRealPs = nullptr;
    u64         suRealVsInputMask = 0;
    bool        sbRealProgramsBound = false;
    // Whether the bound technique's VERTEX program declares a `world` matrix. That single
    // fact decides whether a CONSOLE-INSTANCED mesh can be drawn with its own programs --
    // see the banner on WorldFallbackShader_MarkInstancedMesh.
    bool        sbRealVsHasWorld = false;
    // The technique the last program bind considered, for the per-technique fallback survey.
    // Points into the streamed ShaderTechnique blob (+148), so it outlives the bind.
    const char* spCurrentTechniqueName = nullptr;
    // The most recent per-object WVP (the fallback shader's c0..c3). Kept so a mesh that
    // has to drop back to the fallback can restore it after a real-constant upload.
    f32         safLastWvp[16] = { 0 };
    bool        sbHaveLastWvp = false;

    // [FLAG PC bring-up probe] submitted world draw count -- see the increment site in
    // WorldDraw_IndexedUP and renderengine::WorldDrawCallCount below.
    u64         guWorldDrawCalls = 0;

    // True between ShadowPass_BeginScope/EndScope: the caster draws of the shadow-map pass.
    // Read only by the env-gated cull/depth-bias experiment in WorldDraw_IndexedUP.
    bool        sbShadowPassActive = false;

    // True for the length of PCStampMotionBlurMask's full-screen quads (the motion-blur mask's
    // PC carrier, at the bottom of this file). It is an input to the alpha-to-coverage
    // derivation for the same reason sbShadowPassActive is, and the reason is sharper here:
    // those quads' ENTIRE PURPOSE is the alpha they write, and alpha-to-coverage turns a
    // fragment's alpha into a COVERAGE MASK -- so with a vendor hook live a stamp of
    // alpha = carsByte/255 would kill (255 - carsByte)/255 of its own samples and the mask
    // would come out dithered instead of flat. Made impossible by construction for one bool
    // rather than argued about per vendor, exactly as the shadow pass is.
    bool        sbMaskStampActive = false;

    // [DIAG, mask-alpha] SET by PCStampMotionBlurMask on every stamp; CONSUMED (read and
    // cleared) on every post-fx COMPOSITE draw in the immediate-mode leaf below. It answers the
    // one question the "[mask-alpha] source alpha" line cannot, and whose absence cost a wave:
    // a mask of 0.000 on a frame where THE STAMP NEVER RAN is not a broken carrier, it is a
    // frame with motion blur switched off -- the stamp and the composite's blur permutation are
    // gated on the SAME byte (BrnRendererModule.cpp:4637 and :4943, both reading
    // lPostFxFrameBytes.mbMotionBlurActive inside the same `if (lbSceneBracketOpen)` at :4618) --
    // and the alpha the probe shows is then simply whatever the world pass left in the lane.
    // DELETE with the mask bring-up.
    bool        sbMaskStampedSinceComposite = false;

    // =========================================================================
    // [FLAG PC bring-up diagnostic] THE DEST-ALPHA CENSUS -- the fidelity question the
    // motion-blur mask's carrier turns on, answered with a counter instead of an opinion.
    //
    // The mask rides the SCENE TARGET'S ALPHA LANE (PCStampMotionBlurMask, at the bottom of
    // this file). That lane is only genuinely free if no world material BLENDS ITS COLOUR
    // against it -- i.e. if no shipped BlendState word ever names Xenos factor 8 (DEST_ALPHA)
    // or 9 (INV_DEST_ALPHA) in any of its four factor fields. The shipped material data is the
    // only authority on that and it cannot be grepped out of the source, so it is COUNTED at
    // the one place every blend word arrives -- shadow::Device::Xbox2SetStateLowLevelShadowed
    // is the SOLE caller of D3DDevice_SetBlendState (grep pasted in the wave report) -- and
    // reported once the sample is big enough to mean something.
    //
    // THE STAMP IS SAFE EITHER WAY, and that is WHY this is a diagnostic and not a gate: the
    // stamp runs AFTER every world draw, so during the world pass the alpha lane still holds
    // exactly what it held before this wave and a DEST_ALPHA blend still reads exactly what it
    // read before. A non-zero count does NOT break the mask; it means the alpha the world
    // leaves behind was MEANINGFUL to somebody, which is the fact a future wave would need
    // before moving the stamp earlier or masking the alpha write bit per draw.
    //
    // ⚠ WHY IT LIVES **HERE** AND NOT BESIDE THE BLEND DECODE IT SERVES. Its two call sites
    // (D3DDevice_SetBlendState / D3DDevice_SetRenderState_ColorWriteEnable) and the whole
    // Xenon*ToD3D9 translation block sit INSIDE this file's `extern "C"` region (opened at the
    // fast-set surface's banner, closed ~570 lines later), and C language linkage strips the
    // name mangling from everything declared in it -- which is why `dumpbin /SYMBOLS` shows the
    // existing `XenonBlendFactorToD3D9` as an UNDECORATED External symbol despite its
    // anonymous namespace. Adding four counters and two helpers there would have put six more
    // undecorated, link-visible names into the exe for no reason. Up here they are ordinary
    // anonymous-namespace statics (`?...@?A0x<hash>@@...`, internal linkage), the same as
    // every other piece of state this file owns.
    // DELETE with the mask bring-up: it counts a fact, it does not maintain one.
    // =========================================================================
    const u32 KU_BLEND_CENSUS_REPORT_AT = 1000u;

    u32  suBlendCensusCalls     = 0u;
    u32  suBlendCensusDestAlpha = 0u;
    u32  suBlendCensusWordMask  = 0u;   // OR of every colour-write word pushed (bit 3 = ALPHA)
    bool sbBlendCensusReported  = false;

    inline bool XenonBlendFactorIsDestAlpha(u32 luValue)
    {
        return luValue == 8u || luValue == 9u;   // DEST_ALPHA / INV_DEST_ALPHA
    }

    void BlendCensus_Account(u32 luColourSrc, u32 luColourDst, u32 luAlphaSrc, u32 luAlphaDst)
    {
        ++suBlendCensusCalls;
        if (XenonBlendFactorIsDestAlpha(luColourSrc) || XenonBlendFactorIsDestAlpha(luColourDst)
            || XenonBlendFactorIsDestAlpha(luAlphaSrc) || XenonBlendFactorIsDestAlpha(luAlphaDst))
        {
            ++suBlendCensusDestAlpha;
        }
        if (!sbBlendCensusReported && suBlendCensusCalls >= KU_BLEND_CENSUS_REPORT_AT)
        {
            sbBlendCensusReported = true;
            char lacMessage[320];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[mask-alpha] blend factors seen: DESTALPHA/INVDESTALPHA used by %u of"
                          " %u SetBlendState calls, colour-write words seen {%s%s%s%s}"
                          " (the OR of every word pushed; the A slot is bit 3)\n",
                          static_cast<unsigned>(suBlendCensusDestAlpha),
                          static_cast<unsigned>(suBlendCensusCalls),
                          (suBlendCensusWordMask & 1u) ? "R" : "-",
                          (suBlendCensusWordMask & 2u) ? "G" : "-",
                          (suBlendCensusWordMask & 4u) ? "B" : "-",
                          (suBlendCensusWordMask & 8u) ? "A" : "-");
            CgsDev::Log::WriteToLog(lacMessage);
        }
    }

    // =========================================================================
    // [FLAG PC bring-up probe] the CLIP-SPACE TALLY for the shadow pass.
    //
    // The occlusion probe answered "the casters rasterise nothing"; this answers WHY,
    // by classifying where the geometry actually lands. Every caster draw's first few
    // referenced vertices are pushed through the record's own baked WVP (the same rows
    // SetObjectTransformPC just handed the fallback shader) and bucketed:
    //
    //   behindW      w <= 0            -- the vertex is behind the light's eye plane
    //   outZNear     z < 0             -- in front of the shadow near plane
    //   outZFar      z > w             -- past the shadow far plane
    //   outXY        |x| > w or |y|>w  -- outside the cascade's X/Y extent
    //   inside       none of the above
    //
    // Those five are mutually exclusive in that order, so the tally reads as a single
    // cause rather than a fog of overlapping conditions. A pass where every vertex is
    // outZFar is a near/far problem; all-outXY is an extent/matrix problem; all-behindW
    // is a light-direction problem; all-inside with px==0 means the geometry IS in the
    // volume and the loss is downstream (cull, depth test, or -- since the REAL vertex
    // shader does not use this WVP but `world` x `ViewProjectionModified` -- the constant
    // block the real programs were given is not the cascade's).
    //
    // The device state at the cascade's FIRST caster draw is captured alongside, because
    // "which viewport/scissor/cull/depth state was actually in force" is otherwise pure
    // inference: the material walk rebinds all of them per technique
    // (Xbox2SetRasterizerStateLowLevelShadowed / ...DepthStencil...), so whatever
    // BeginRenderShadowMap set is NOT necessarily what the draws ran under.
    //
    // DELETE with the rest of the shadow bring-up probes.
    // =========================================================================
    struct ShadowClipTally
    {
        u32  muSampled;
        u32  muInside;
        u32  muOutXY;
        u32  muOutZNear;
        u32  muOutZFar;
        u32  muBehindW;
        u32  muNoWvp;

        // The FIRST sample, in full. The tally above only means something if the vertex was
        // decoded correctly, and this classifier assumes POSITION is three floats at vertex
        // byte 0 -- the same assumption the wheel probe makes, and one that a compressed
        // (DEC3N / int16) position stream would silently break. Printing the raw object
        // position next to its clip result makes a mis-decode obvious at a glance: Paradise
        // world coordinates are metres in the hundreds-to-thousands, so a first sample of
        // (1e-41, 3e+27, ...) is a decode bug and the buckets are meaningless.
        f32  mafFirstObject[3];
        f32  mafFirstClip[4];

        // ---- THE FITTED EXTENT, IN METRES -----------------------------------------------
        // The one number that decides whether the cascade projection is right, read straight
        // off the matrix instead of inferred from a sample.
        //
        // The record's WVP is world x view x projection, and the world transform of a world
        // chunk or a car is rigid (rotation + translation, unit scale). So the gradient of
        // clip-x with respect to object position -- the length of the matrix's first COLUMN,
        // |(m00, m10, m20)| -- is d(clip.x)/d(metre). An orthographic projection maps its
        // half-extent onto clip 1, so
        //
        //     half-width in metres = 1 / |(m00, m10, m20)|
        //
        // and likewise for height and for the depth span. Those are directly comparable with
        // what the cascade is SUPPOSED to cover: the subset-frustum slabs
        // KAF_SHADOWMAP_SUBSET_FRUSTUM_{NEAR,FAR}_CLIP = 0..10.5, 10.5..34, 34..120 m
        // (BrnShadowMap.cpp:139-140), i.e. tens of metres, not thousands.
        //
        // Caveat, stated rather than hidden: a non-rigid world matrix (a scaled prop) makes
        // this reading that object's scale as well. The clip AABB below is the scale-free
        // cross-check -- a correct fit puts the casters the cascade SELECTED across most of
        // [-1,1], because that is what fitting means.
        f32  mfHalfWidthMetres;
        f32  mfHalfHeightMetres;
        f32  mfDepthSpanMetres;

        // The clip-space AABB of every sampled vertex (after the perspective divide), and the
        // sub-pixel triangle census: for each sampled draw the first triangle's area is
        // computed in the cascade's own viewport pixels. "100% inside the volume and still no
        // fragments" has exactly one remaining explanation, and this is the number that
        // either proves or kills it.
        f32  mafClipMin[3];
        f32  mafClipMax[3];
        u32  muTrisSampled;
        u32  muTrisSubPixel;
        f32  mfMaxTriPixelArea;

        bool mbStateCaptured;
        bool mbEffectiveCaptured;
        u32  muZFuncEffective;    // read immediately BEFORE the draw, i.e. after any override
        u32  muCullEffective;
        u32  muVpX, muVpY, muVpW, muVpH;
        f32  mfVpMinZ, mfVpMaxZ;
        s32  miScissorL, miScissorT, miScissorR, miScissorB;
        u32  muScissorEnable;
        u32  muZEnable, muZWrite, muZFunc, muCull, muColourWrite;
    };

    const u32      KU_SHADOW_TALLY_SLOTS = 4u;   // 3 cascades + the world-pass control
    ShadowClipTally saShadowClip[KU_SHADOW_TALLY_SLOTS] = {};
    u32             suShadowClipSlot = 0xFFFFFFFFu;

    // =========================================================================
    // ALPHA-TO-COVERAGE -- the Xenos' ALPHA_TO_MASK realised over the D3D9 vendor hooks.
    // The console semantics, the shipped data and the vendor citations are on the two
    // LEAVES this serves (D3DDevice_SetRenderState_AlphaToMask{Enable,Offsets}); this is
    // the state and the reconciler.
    //
    // WHY IT LIVES UP HERE and not beside those leaves: five places have to reconcile
    // through it -- the two leaves, D3DDevice_SetRenderState_AlphaTestEnable,
    // renderengine::Device::SetWorldPassDefaultStates (:682), PCSurfaceBracket_Save/_Restore
    // (the shadow-map bracket) and PCSceneBlit_Begin/_End -- and all of them are defined
    // between here and the leaves.
    //
    // THE ONLY THING SHADOWED IS THE CONSOLE'S OWN REQUEST. The alpha-test half of the
    // decision is READ BACK from the device with GetRenderState rather than mirrored here:
    // shadowingdevice.cpp's mauLowLevelStateShadow[16] and the D3D9 runtime already own
    // that value, and a third copy is precisely the split-brain this project keeps paying
    // for. Everything else in the decision (the vendor capability, the bound target's
    // sample count, the shadow-pass flag, the config knob) is a live query.
    //
    // ...BUT A LIVE QUERY IS ONLY AS FRESH AS THE LAST RECONCILE, so every WRITER of an input
    // must reconcile. The verify round (2026-08-16) found the one that did not: the BOUND COLOUR
    // TARGET. Every engine render-target bind funnels through renderengine::Device::SetState
    // (PostFxRenderTargetPCLeaf.cpp -- the single writer of the surfaces, reached from
    // CgsRenderTarget::SetRenderTargetState{,InvertDepth} and rw::graphics::postfx::
    // RenderTarget::Begin), and it did not reconcile; the sample-count gate was enforced only
    // whenever one of the OTHER call sites happened to fire. Three accidents made that harmless
    // on today's frame graph (the shadow bracket reconciles on both edges; shadowingdevice.cpp's
    // ResetProgramShadows nulls mpBlendState before each of Render's five walks so every walk
    // opens on the unset path re-issuing word[10] AND word[16]; BeginRenderEnvironmentMapFace --
    // the one pass that binds a single-sampled colour target in the MIDDLE of world geometry --
    // is not reconstructed yet). The residual was the window from the last world draw to
    // PCSceneBlit_Begin (resolve + post-fx chain, single-sampled): inert on NVIDIA (the alpha
    // test is the trigger, the post-fx quads do not use it) but the AMD path is deliberately not
    // alpha-test-gated, so an 'A2M1' could stand across it -- exactly the single-sample dither
    // AlphaCoverageTargetIsMultisampled exists to prevent. CLOSED: Device::SetState now calls
    // PCAlphaCoverage_Reconcile() (exported below, declared in ShadowPassPCLeaf.h beside the
    // bracket's own two) as its last statement, so the derivation is complete BY CONSTRUCTION.
    // No recursion: the reconciler reads the target (GetRenderTarget) and never binds one, and
    // the bracket / the blit rebind raw, not through SetState.
    //
    // ⚠ THE ORDER THE BINDER USES MATTERS AND IS WHY THE RECONCILER IS SHARED.
    // shadowingdevice.cpp's Xbox2SetStateLowLevelShadowed issues word[10] (alpha-to-mask)
    // BEFORE word[16] (alpha-test enable) on BOTH paths -- unset path :619 then :625,
    // incremental path :686 then :711. So at the moment the alpha-to-mask leaf runs, the
    // alpha test still reads the PREVIOUS material's value. That transient is harmless (a
    // state bind issues no draws) precisely BECAUSE the alpha-test leaf reconciles again
    // straight after it, and the settled combination is the one that reaches the draw.
    // =========================================================================
    const u32 KU_A2M_OFFSETS_DITHERED = 0x87u;   // the only offsets word the shipped data carries

    // The two vendor pseudo-format words. Both are ordinary FOURCCs smuggled through a
    // render state; neither is a format anything is ever created in.
    const DWORD KU_A2C_NVIDIA_ENABLE  = MAKEFOURCC('A', 'T', 'O', 'C');
    const DWORD KU_A2C_NVIDIA_DISABLE = 0u;                             // D3DFMT_UNKNOWN
    const DWORD KU_A2C_AMD_ENABLE     = MAKEFOURCC('A', '2', 'M', '1');
    const DWORD KU_A2C_AMD_DISABLE    = MAKEFOURCC('A', '2', 'M', '0');

    enum EAlphaCoveragePath
    {
        E_A2C_PATH_UNTESTED = -1,
        E_A2C_PATH_NONE     = 0,
        E_A2C_PATH_NVIDIA   = 1,   // D3DRS_ADAPTIVETESS_Y = 'ATOC' / D3DFMT_UNKNOWN
        E_A2C_PATH_AMD      = 2    // D3DRS_POINTSIZE      = 'A2M1' / 'A2M0'
    };

    int  siA2cPath      = E_A2C_PATH_UNTESTED;
    bool sbA2cRequested = false;                     // the console's blend word[10]
    u32  suA2cOffsets   = KU_A2M_OFFSETS_DITHERED;   // the console's blend word[8]
    bool sbA2cApplied   = false;                     // what this shim last put on the device

    // Which hook this adapter exposes, asked ONCE: the probe is a driver round trip and the
    // answer cannot change for the life of the device (the same reasoning, and the same
    // shape, as TilingReszSupported at the bottom of this file).
    //
    // ⚠ THE TWO VENDORS ARE NOT DETECTED THE SAME WAY, AND THAT ASYMMETRY IS REAL.
    //   * NVIDIA publishes a capability probe and it is used verbatim: CheckDeviceFormat with
    //     USAGE 0 and D3DRTYPE_SURFACE against the 'ATOC' pseudo-format -- NVIDIA technical
    //     report "Antialiasing with Transparency" (2004/2005), Sample Code. (Intel's D3D9
    //     driver also answers this probe; nothing here needs to care which one did.)
    //   * AMD publishes NO probe for alpha-to-coverage. "Advanced DX9 Capabilities for ATI
    //     Radeon Cards" (rev. 2009-09-09) gives every OTHER hook it documents -- INTZ, NULL,
    //     RESZ, DF16/DF24, Fetch-4, ATI1N/ATI2N -- an explicit CheckDeviceFormat snippet, and
    //     for alpha-to-coverage states instead that every ATI card exposing the D3D9 feature
    //     set supports it. A2M1/A2M0 are RENDER-STATE values behind the FourCC interface, not
    //     surface formats, so CheckDeviceFormat(..., 'A2M1') has no documented meaning at all
    //     and asking it would be an invented convention. The honest test is therefore the
    //     ADAPTER VENDOR, read from the same D3D9 object.
    const DWORD KU_PCI_VENDOR_AMD = 0x1002u;   // ATI/AMD's PCI vendor id

    int AlphaCoveragePath()
    {
        if (siA2cPath != E_A2C_PATH_UNTESTED)
            return siA2cPath;

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return E_A2C_PATH_NONE;      // not latched: ask again once the device exists
        siA2cPath = E_A2C_PATH_NONE;

        D3DDEVICE_CREATION_PARAMETERS lCreation;
        std::memset(&lCreation, 0, sizeof(lCreation));
        lCreation.AdapterOrdinal = D3DADAPTER_DEFAULT;
        lCreation.DeviceType     = D3DDEVTYPE_HAL;
        lpDevice->GetCreationParameters(&lCreation);

        IDirect3D9* lpD3D = nullptr;
        if (SUCCEEDED(lpDevice->GetDirect3D(&lpD3D)) && lpD3D != nullptr)
        {
            // The published snippet passes D3DFMT_X8R8G8B8 literally; the live display mode is
            // used instead for the same reason TilingReszSupported does -- it is the format the
            // device was actually created against.
            D3DFORMAT      leAdapterFormat = D3DFMT_X8R8G8B8;
            D3DDISPLAYMODE lMode;
            std::memset(&lMode, 0, sizeof(lMode));
            if (SUCCEEDED(lpD3D->GetAdapterDisplayMode(lCreation.AdapterOrdinal, &lMode)))
                leAdapterFormat = lMode.Format;

            if (SUCCEEDED(lpD3D->CheckDeviceFormat(
                    lCreation.AdapterOrdinal, lCreation.DeviceType, leAdapterFormat,
                    0u, D3DRTYPE_SURFACE, static_cast<D3DFORMAT>(KU_A2C_NVIDIA_ENABLE))))
            {
                siA2cPath = E_A2C_PATH_NVIDIA;
            }
            else
            {
                D3DADAPTER_IDENTIFIER9 lIdentifier;
                std::memset(&lIdentifier, 0, sizeof(lIdentifier));
                if (SUCCEEDED(lpD3D->GetAdapterIdentifier(lCreation.AdapterOrdinal, 0u,
                                                          &lIdentifier))
                    && lIdentifier.VendorId == KU_PCI_VENDOR_AMD)
                {
                    siA2cPath = E_A2C_PATH_AMD;
                }
            }
            lpD3D->Release();            // GetDirect3D AddRefs
        }

        if (siA2cPath == E_A2C_PATH_NVIDIA)
        {
            LogOnce("a2c-path",
                    "[a2c] alpha-to-mask -> NVIDIA ATOC (D3DRS_ADAPTIVETESS_Y): the shader's"
                    " output alpha becomes an MSAA coverage mask on alpha-tested draws.\n");
        }
        else if (siA2cPath == E_A2C_PATH_AMD)
        {
            LogOnce("a2c-path",
                    "[a2c] alpha-to-mask -> AMD A2M (D3DRS_POINTSIZE = 'A2M1'/'A2M0'): the"
                    " shader's output alpha becomes an MSAA coverage mask.\n");
        }
        else
        {
            LogOnce("a2c-path",
                    "[a2c] no D3D9 alpha-to-coverage on this adapter; alpha-tested cut-outs"
                    " keep hard edges.\n");
        }
        return siA2cPath;
    }

    // Does this path need the alpha test switched on before the hook does anything?
    //   * NVIDIA: YES -- but this is NOT in any NVIDIA document. The "Antialiasing with
    //     Transparency" report never names D3DRS_ALPHATESTENABLE; the requirement is attested
    //     by practitioners and, more usefully, by DXVK, whose D3D9 layer gates ATOC on
    //     `renderStates[D3DRS_ADAPTIVETESS_Y] == ATOC && renderStates[D3DRS_ALPHATESTENABLE]`
    //     (d3d9_device.cpp, UpdateAlphaToCoverageAndAlphaTest). Marked UNSOURCED-BY-VENDOR
    //     rather than presented as documented.
    //   * AMD: NO. The AMD document says nothing about the alpha test, and DXVK's AMD branch
    //     checks only the POINTSIZE value. That matches the CONSOLE, where alpha-to-mask and
    //     the alpha test are separate bits of RB_COLORCONTROL, so the AMD path is left
    //     faithful and is NOT gated here. (Moot on the shipped data -- every alpha-to-mask
    //     request carries the alpha test on -- but a gate that is not needed is a deviation
    //     that is not taken.)
    inline bool AlphaCoveragePathNeedsAlphaTest(int liPath)
    {
        return liPath == E_A2C_PATH_NVIDIA;
    }

    // Is the bound COLOUR target multisampled? THIS CHECK IS LOAD-BEARING, NOT HYGIENE.
    // Neither vendor documents what its hook does on a single-sampled target: DXVK makes the
    // NVIDIA one inert (it ANDs alpha-to-coverage with "RT0 sample count > 1"), but the AMD
    // side is undetermined and the practitioner reading is that a single-sample configuration
    // shows a visible DITHER -- an artefact, not a no-op. So the gate is applied on every
    // path rather than argued away on one vendor's behalf.
    //
    // The D3D9 runtime is the only thing that knows what was actually created -- the same
    // argument TilingSurfaceIsMultisampled makes further down; it is not reused because it
    // lives in a different anonymous namespace 1,500 lines below. Asked only when everything
    // cheaper has already said yes -- which is NOT "a handful of times a frame": with a request
    // live it runs on every alpha-test change (hundreds a second) and now on every render-target
    // bind. GetRenderTarget is a runtime AddRef and GetDesc a struct copy; the measured cost of
    // the whole wave was 58.8 -> 57.5 fps. For scale, the probe boot counted
    // 19,899 alpha-to-mask leaf calls over ~110 s (~3 a frame) and the blend-state shadow
    // means only the ones that CHANGE ever get here.
    bool AlphaCoverageTargetIsMultisampled(IDirect3DDevice9* lpDevice)
    {
        IDirect3DSurface9* lpTarget = nullptr;
        if (FAILED(lpDevice->GetRenderTarget(0u, &lpTarget)) || lpTarget == nullptr)
            return false;
        D3DSURFACE_DESC lDesc;
        std::memset(&lDesc, 0, sizeof(lDesc));
        const bool lbMultisampled = SUCCEEDED(lpTarget->GetDesc(&lDesc))
                                 && lDesc.MultiSampleType != D3DMULTISAMPLE_NONE;
        lpTarget->Release();             // GetRenderTarget AddRefs
        return lbMultisampled;
    }

    // Bring the vendor state in line with the five inputs. Idempotent and cheap: it issues a
    // SetRenderState only when the answer actually changes.
    void AlphaCoverage_Reconcile()
    {
        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice == nullptr)
            return;

        // Probed on the FIRST reconcile of the run (the first blend-state bind) so the log
        // names the path taken early, whether or not anything asks for coverage.
        const int liPath = AlphaCoveragePath();

        bool lbWant = (liPath != E_A2C_PATH_NONE)
                   && sbA2cRequested
                   && (renderengine::gAlphaToCoverage != 0)
                   // The shadow-map pass draws the SAME alpha-tested foliage into a
                   // single-sampled depth atlas. No vendor DOCUMENT says what its hook does
                   // there, so "A2C cannot touch the shadow map" is made true by construction
                   // for one bool instead of by appeal to one vendor's behaviour.
                   && !sbShadowPassActive
                   // ...and the motion-blur mask stamp, whose quads exist to write one exact
                   // alpha value (see that flag's own note above).
                   && !sbMaskStampActive;

        if (lbWant && AlphaCoveragePathNeedsAlphaTest(liPath))
        {
            // ⚠ ON NVIDIA THE ALPHA TEST IS THE TRIGGER (see AlphaCoveragePathNeedsAlphaTest
            // for exactly how well sourced that is). READ BACK rather than shadowed -- see the
            // banner above; a third copy of a value shadowingdevice.cpp and the D3D9 runtime
            // both already hold is the split-brain rule 3 forbids.
            DWORD luAlphaTestEnable = FALSE;
            lpDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &luAlphaTestEnable);
            lbWant = (luAlphaTestEnable != FALSE);
        }
        if (lbWant)
            lbWant = AlphaCoverageTargetIsMultisampled(lpDevice);

        if (lbWant == sbA2cApplied)
            return;

        if (liPath == E_A2C_PATH_NVIDIA)
        {
            lpDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y,
                                     lbWant ? KU_A2C_NVIDIA_ENABLE : KU_A2C_NVIDIA_DISABLE);
        }
        else if (liPath == E_A2C_PATH_AMD)
        {
            // ⚠ SHARES D3DRS_POINTSIZE WITH THE RESZ DEPTH RESOLVE. AMD overloads that one
            // render state with at least three unrelated back doors -- 'A2M1'/'A2M0'
            // (alpha-to-coverage), 0x7FA05000 (the RESZ depth resolve this file already uses)
            // and 'INST' (SM2.0 instancing) -- so the two users in this tree have to agree.
            // They do: TilingResolveDepthToTexture is the ONLY other writer of D3DRS_POINTSIZE
            // in the whole tree (grep pasted in the report), and it GETs the state, writes the
            // RESZ magic, then puts back exactly what it read -- so an 'A2M1' in flight
            // survives the resolve and sbA2cApplied stays true across it. Its own trigger draw
            // is a degenerate point with colour writes off, so coverage is irrelevant for the
            // window in which the magic is in there. Nothing in this build sets a real point
            // size, which is what makes the overload safe in the first direction too.
            lpDevice->SetRenderState(D3DRS_POINTSIZE,
                                     lbWant ? KU_A2C_AMD_ENABLE : KU_A2C_AMD_DISABLE);
        }
        else
        {
            return;                      // no hook: nothing was ever applied, nothing to undo
        }
        sbA2cApplied = lbWant;
    }
}

namespace renderengine
{
    // The exported face of AlphaCoverage_Reconcile for the ONE input writer that lives in
    // another TU: renderengine::Device::SetState (PostFxRenderTargetPCLeaf.cpp), the single
    // binder of colour slot 0. Declared in ShadowPassPCLeaf.h beside PCSurfaceBracket_Save/
    // Restore. See the derivation banner above for why every input writer must reconcile.
    void PCAlphaCoverage_Reconcile()
    {
        AlphaCoverage_Reconcile();
    }

    // [PC bring-up] renderengine::Device::SetWorldPassDefaultStates -- the state a world
    // pass starts from (declared in device.h). Every state a MATERIAL owns is now bound per
    // technique from its own MaterialState triple (shadow::Device::SetMaterialRenderStatesPC),
    // so this only has to leave the device in a sane state for anything drawn before the
    // first technique bind, and to switch off the fixed-function lighting D3D9 defaults on.
    void Device::SetWorldPassDefaultStates(bool lbTransparentPass)
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr)
            return;

        lpDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, lbTransparentPass ? FALSE : TRUE);
        lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, lbTransparentPass ? TRUE : FALSE);
        lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        // The alpha test just went off, and alpha-to-coverage is derived from it, so the
        // vendor hook has to follow. NOTE that sbA2cRequested is NOT cleared: the console's
        // word[10] is still whatever the last material asked for, and the blend-state shadow
        // (mauLowLevelStateShadow[10]) will NOT re-issue an unchanged word -- clearing the
        // request here would strand alpha-to-coverage off for the rest of the pass. Deriving
        // instead of clearing is what makes the pass boundary safe AND recoverable.
        AlphaCoverage_Reconcile();
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        // Colour writes ON. A world pass' own techniques override this from their
        // MaterialState -- and the Z pre-pass deliberately drives it to 0 -- so the
        // pass-boundary reset has to put it back, otherwise a frame whose last world
        // work was the depth-only pass would swallow the whole 2D/GUI tail.
        lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                                 D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
                                 | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        // The rest of the material-owned state a technique can drive but nothing after the
        // pass puts back. Same reasoning as the colour-write reset above: SetMaterialRenderStatesPC
        // binds all of these off the technique's MaterialState triple, while the 2D/GUI tail's
        // fixed-function prologues only re-set lighting/depth/cull/blend-factor/scissor. A world
        // pass that ended on a stencil-write technique, a wireframe raster state, a subtract
        // blend op or a separate-alpha blend would otherwise carry it straight into the overlay.
        lpDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        lpDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        lpDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        lpDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        // And the PROGRAMS. The walk leaves the last technique's vertex/pixel pair bound
        // (WorldPrograms_Bind / WorldFallbackShader_Bind above are the only SetVertex/PixelShader
        // callers in the build), and D3D9 keeps a bound shader until it is explicitly cleared --
        // it does not fall back to fixed function just because a later draw sets an FVF. The
        // 2D tail draws through the fixed-function pipeline, so the pass boundary hands the
        // device back with no programs bound, exactly as it found it.
        lpDevice->SetVertexShader(nullptr);
        lpDevice->SetPixelShader(nullptr);
    }

    // ---- world-pass leaf hooks (declared in shadowingdevice.cpp) -----------

    bool WorldFallbackShader_Bind()
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr || !CompileFallbackShaders(lpDevice))
            return false;
        lpDevice->SetVertexShader(spFallbackVs);
        lpDevice->SetPixelShader(spFallbackPs);
        return true;
    }

    void WorldFallbackShader_SetWvp(const f32* lpWvpRows16)
    {
        std::memcpy(safLastWvp, lpWvpRows16, sizeof(safLastWvp));
        sbHaveLastWvp = true;
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice != nullptr)
            lpDevice->SetVertexShaderConstantF(KU_FALLBACK_WVP_REGISTER, lpWvpRows16, 4);
    }

    // ---- the REAL per-technique program path (PC leaf) ----------------------
    // The X360 binds the technique's two microcode programs through
    // shadow::Device::SetVertexProgram / SetPixelProgram (which push the Xenos shader
    // headers straight into the command buffer). On PC the same two payloads --
    // ProgramBufferData + 0x14, which the converter fills with plain D3D9 SM3 bytecode
    // (FORMAT_MAP.md section 5) -- become IDirect3DVertex/PixelShader9 objects created
    // once per image and cached by payload address.
    //
    // Returns false (leaving nothing bound) when either payload is not D3D9 bytecode or
    // the object cannot be created; the caller then keeps the flagged fallback pair.
    bool WorldPrograms_Bind(const void* lpVertexPayload, const void* lpPixelPayload,
                            const char* lpcTechniqueName)
    {
        IDirect3DDevice9* lpDevice = Dev();
        sbRealProgramsBound = false;
        sbRealVsHasWorld    = false;
        // The technique this bind is for, kept for the fallback survey and the wheel diag.
        spCurrentTechniqueName = lpcTechniqueName;
        // Per-technique sampler record: see the two pointers' banner. Reset HERE, before
        // SetMeshTechniquePC's own BindTechniqueSamplers tail fills it in.
        spTechniqueUnit0Texture = nullptr;
        spTechniqueFirstTexture = nullptr;
        if (lpDevice == nullptr || lpVertexPayload == nullptr || lpPixelPayload == nullptr)
        {
            WorldShader_ReportFallback(lpcTechniqueName, E_WORLD_FALLBACK_NO_PROGRAM);
            return false;
        }
        if (!LooksLikeD3D9Bytecode(lpVertexPayload, false) || !LooksLikeD3D9Bytecode(lpPixelPayload, true))
        {
            LogOnce("realbc", "[WorldShader] technique program is not D3D9 bytecode - fallback kept\n");
            WorldShader_ReportFallback(lpcTechniqueName, E_WORLD_FALLBACK_NOT_D3D9_BYTECODE);
            return false;
        }

        IDirect3DVertexShader9*& lrpVs = sVsCache[lpVertexPayload];
        if (lrpVs == nullptr)
            lpDevice->CreateVertexShader(static_cast<const DWORD*>(lpVertexPayload), &lrpVs);
        IDirect3DPixelShader9*& lrpPs = sPsCache[lpPixelPayload];
        if (lrpPs == nullptr)
            lpDevice->CreatePixelShader(static_cast<const DWORD*>(lpPixelPayload), &lrpPs);
        if (lrpVs == nullptr || lrpPs == nullptr)
        {
            LogOnce("realcr", "[WorldShader] Create{Vertex,Pixel}Shader FAILED for a technique - fallback kept\n");
            WorldShader_ReportFallback(lpcTechniqueName, E_WORLD_FALLBACK_CREATE_FAILED);
            return false;
        }

        lpDevice->SetVertexShader(lrpVs);
        lpDevice->SetPixelShader(lrpPs);
        spRealVs            = lrpVs;
        spRealPs            = lrpPs;
        suRealVsInputMask   = VertexShaderInputMask(lpVertexPayload);
        sbRealVsHasWorld    = VertexProgramHasWorldMatrix(lpVertexPayload);
        sbRealProgramsBound = true;
        LogOnce("realok", "[WorldShader] REAL per-technique programs bound (SHADERS.BNDL)\n");
        return true;
    }

    // The exported face of the fallback survey, for the ONE caller that knows a technique has
    // no program pair at all before the bind is even attempted (shadow::Device::
    // SetMeshTechniquePC's null-import arm).
    void WorldShader_ReportTechniqueHasNoPrograms(const char* lpcTechniqueName)
    {
        WorldShader_ReportFallback(lpcTechniqueName, E_WORLD_FALLBACK_NO_PROGRAM);
    }

    void WorldShader_ClearRealPrograms()
    {
        sbRealProgramsBound = false;
        sbRealVsHasWorld    = false;
        spRealVs            = nullptr;
        spRealPs            = nullptr;
        suRealVsInputMask   = 0;
        // Per-technique sampler record: see the two pointers' banner.
        spTechniqueUnit0Texture = nullptr;
        spTechniqueFirstTexture = nullptr;
    }

    bool WorldShader_RealProgramsBound()
    {
        return sbRealProgramsBound;
    }

    // FLAG PC bring-up: the alpha-test REFERENCE and comparison belong to the technique's
    // blend-state object, whose packed Xenos register fields are not decoded yet
    // (renderengine::BlendStateParameters muState4..muState17). The 1-bit-alpha world
    // techniques all cut out at the texture's mid-point, so >= 128 stands in until then.
    // One shader-constant upload: luNumRegisters float4s from lpData at register luRegister.
    // (X360: a direct write into the Xenos ALU-constant window through the command buffer;
    // the D3D9 equivalent is Set{Vertex,Pixel}ShaderConstantF.)
    void WorldShaderConstants_Set(bool lbPixel, u32 luRegister, const void* lpData, u32 luNumRegisters)
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr || lpData == nullptr || luNumRegisters == 0)
            return;
        // D3D9 SM3 exposes 224 float constants to the vertex stage and 224 to the pixel
        // stage; a request past the end is rejected wholesale, so clamp and report once.
        const u32 luLimit = lbPixel ? 224u : 256u;
        if (luRegister >= luLimit)
        {
            LogOnce("creghi", "[WorldShader] constant register out of range for D3D9 - skipped\n");
            return;
        }
        if (luRegister + luNumRegisters > luLimit)
            luNumRegisters = luLimit - luRegister;

        if (lbPixel)
            lpDevice->SetPixelShaderConstantF(luRegister, static_cast<const float*>(lpData), luNumRegisters);
        else
            lpDevice->SetVertexShaderConstantF(luRegister, static_cast<const float*>(lpData), luNumRegisters);
    }

    // Bind one texture + the world's standard sampler set at a D3D sampler unit.
    bool WorldShader_BindTextureUnit(u32 luUnit, const void* lpRaster)
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr || lpRaster == nullptr || luUnit >= 16u)
            return false;
        const Texture* const lpTexture = static_cast<const Texture*>(lpRaster);
        IDirect3DBaseTexture9* const lpD3DTexture = lpTexture->mpD3DTexture;
        if (lpD3DTexture == nullptr)
            return false;

        lpDevice->SetTexture(luUnit, lpD3DTexture);
        // FLAG PC-platform leaf: the real sampler descriptor comes from the TextureState's
        // 32-byte sampler block (renderengine::SamplerState); until that unpack lands, the
        // world's standard trilinear/wrap set stands.
        //
        // ONE EXCEPTION, ADDED WITH THE CUBE CREATE PATH (reflections step 1): a CUBE raster is
        // addressed by a DIRECTION VECTOR whose face is chosen by the hardware, and WRAP is the
        // wrong request for it -- the console's own reflection TextureStates ask for CLAMP on U and V
        // (addrU = addrV = 2, measured over all five cube-backed shipped TextureStates, reflections
        // step 2); W is 0/WRAP in the data and unused by a cube fetch (addressed by a direction
        // vector), so clamping it too is inert. This matters now because the 234 shipped 64x64x6 rasters that feed
        // the 7 WORLD techniques declaring `ReflectionTextureSampler` as a samplerCUBE (s2 x6,
        // s0 x1 -- CTAB scan of build/game/SHADERS.BNDL) reach D3D9 through THIS path, not
        // through D3DDevice_SetTexture, and until this wave they arrived as 2D textures carrying
        // face 0 only.
        //
        // THE TEST IS A HEADER BYTE, NOT A COM CALL: renderengine::Texture::GetType reads the
        // raster's own create-time dimension (texture.cpp TextureDimension), so this costs a load
        // and a compare on a path that has already issued six device calls -- not an
        // IDirect3DBaseTexture9::GetType() per bind.
        const bool lbCubeRaster = (Texture::GetType(lpTexture) == Texture::E_TYPE_CUBE);
        const DWORD luAddressMode =
            lbCubeRaster ? (DWORD)D3DTADDRESS_CLAMP : (DWORD)D3DTADDRESS_WRAP;
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU, luAddressMode);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV, luAddressMode);
        if (lbCubeRaster)
            lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        // Record it for the fallback selector (see the two pointers' banner). Costs two
        // predictable stores on a path that has just done five device calls.
        if (luUnit == 0u)
            spTechniqueUnit0Texture = lpD3DTexture;
        if (spTechniqueFirstTexture == nullptr)
            spTechniqueFirstTexture = lpD3DTexture;
        return true;
    }

    // [PC bring-up shim] Bind a material's own sampler textures.
    //
    // This is the DATA half of the X360 technique bind (DispatchAllMeshes @0x827F2718, the
    // `lpaInternalSamplers` loop at CgsDispatcherCommands.cpp:2332-2336). Offsets are
    // asm-attested and cross-checked against the converted world data:
    //
    //   MaterialAssembly (the serialised Material blob, console 32-bit layout)
    //     +0x09  s8   mi8NumSamplers            (MaterialResourceType::FixUp reads this byte)
    //     +0x0C  u32  mpaInternalSamplers       (rebased by that FixUp)
    //   InternalSampler (20-byte stride; the FixUp rebases +0x00 and writes +0x0C)
    //     +0x00  u32  name char*
    //     +0x04  u32  name hash
    //     +0x08  u16  sampler unit              (X360 `sub_8227D158(state, *(v295+8))`)
    //     +0x0A  u16  scope                     (assert == E_TEXTURE_SCOPE_MATERIAL, i.e. 0)
    //     +0x0C  s32  sampler type index
    //     +0x10  u32  TextureState*             (bundle IMPORT)
    //   TextureState  +0x20  u32 mpRaster       (bundle IMPORT; renderengine::Texture*)
    //   Texture       +0x00  IDirect3DBaseTexture9* mpD3DTexture
    //
    // DIVERGENCE (flagged): the X360 binds only the samplers the TECHNIQUE names, through its
    // byte index list at MaterialTechnique+0x24 (count at +0x22). In the converted data those
    // index bytes sit in the same serialiser-fill region as the per-stage binding lists (both
    // are runtime-populated scratch, 0x44 filler on disc), so an index read there is not
    // trustworthy yet. Until MaterialResourceType::PostFixUpShaderConstants' companion sampler
    // pass is reconstructed, bind EVERY material-scope sampler at its own unit -- a superset of
    // the technique's selection, correct for the single-diffuse world materials and harmless
    // for the rest. Returns true when at least one texture was bound.
    //
    // The TextureState raster slot is read as a u32 (not through the host-width
    // renderengine::TextureState::mpRaster): this is the STREAMED blob, whose +0x24 word is
    // serialiser filler, so an 8-byte read there splices. Bug class (b).
    bool WorldMaterialSamplers_Bind(const void* lpMaterialAssembly)
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr || lpMaterialAssembly == nullptr)
        {
            sbMaterialTextureBound = false;
            return false;
        }

        sbMaterialTextureBound = false;
        const u8* const lpBlob = static_cast<const u8*>(lpMaterialAssembly);
        const s32 liNumSamplers = static_cast<s32>(*reinterpret_cast<const s8*>(lpBlob + 0x09));
        u32 luSamplersSlot;
        std::memcpy(&luSamplersSlot, lpBlob + 0x0C, 4);
        if (liNumSamplers <= 0 || luSamplersSlot == 0)
            return false;

        const u8* const lpSamplers = reinterpret_cast<const u8*>(static_cast<uintptr_t>(luSamplersSlot));
        bool lbBoundAny = false;
        for (s32 liIndex = 0; liIndex < liNumSamplers; ++liIndex)
        {
            const u8* const lpSampler = lpSamplers + 20 * liIndex;
            u16 lu16Unit, lu16Scope;
            u32 luStateSlot;
            std::memcpy(&lu16Unit,    lpSampler + 0x08, 2);
            std::memcpy(&lu16Scope,   lpSampler + 0x0A, 2);
            std::memcpy(&luStateSlot, lpSampler + 0x10, 4);
            if (lu16Scope != 0 || luStateSlot == 0 || lu16Unit >= 16u)
                continue;

            // [DIAG] unit 15 is the SHADOW MAP's, bound once a frame by
            // BrnRendererModule::Render (X360 Render:536-542) outside any material. A
            // material-scope sampler landing there would silently replace the shadow map for
            // the rest of the frame -- the exact failure that looks like "shadows stopped
            // working half way down the pass list". Report it if it ever happens; do NOT
            // suppress the bind (that would be inventing a rule the console does not have).
            if (lu16Unit == 15u)
            {
                LogOnce("mats15", "[WorldSamplers] a MATERIAL-scope sampler claims unit 15 -"
                                  " it overwrites the shadow map bind\n");
            }

            const u8* const lpState = reinterpret_cast<const u8*>(static_cast<uintptr_t>(luStateSlot));
            u32 luRasterSlot;
            std::memcpy(&luRasterSlot, lpState + 0x20, 4);
            if (luRasterSlot == 0 || luRasterSlot == 0xFFFFFFFFu)
                continue;

            const Texture* const lpTexture =
                reinterpret_cast<const Texture*>(static_cast<uintptr_t>(luRasterSlot));
            IDirect3DBaseTexture9* const lpD3DTexture = lpTexture->mpD3DTexture;
            if (lpD3DTexture == nullptr)
                continue;

            {
                // [DIAG one-shot] first successful world sampler bind.
                static bool sbDiag = false;
                if (!sbDiag)
                {
                    sbDiag = true;
                    char lacMsg[256];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                                  "[WorldSamplers] n=%d unit=%u state=%08x raster=%08x d3d=%p decl_uv=%d\n",
                                  (int)liNumSamplers, (unsigned)lu16Unit, luStateSlot, luRasterSlot,
                                  (void*)lpD3DTexture, (int)sbLastDeclHasTexcoord0);
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
            lpDevice->SetTexture(lu16Unit, lpD3DTexture);
            // FLAG PC-platform leaf: the real sampler descriptor comes from the TextureState's
            // 32-byte sampler block (renderengine::SamplerState); until that unpack lands, the
            // world's standard trilinear/wrap set stands.
            lpDevice->SetSamplerState(lu16Unit, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            lpDevice->SetSamplerState(lu16Unit, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            lpDevice->SetSamplerState(lu16Unit, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            lpDevice->SetSamplerState(lu16Unit, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            lpDevice->SetSamplerState(lu16Unit, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

            // [PC bring-up shim] The fallback pixel shader has a single sampler (s0), so the
            // FIRST material-scope texture is also mirrored onto unit 0 -- otherwise a material
            // whose diffuse sits on a higher unit would sample whatever was left bound there.
            // Skipped once the real per-technique programs are bound: those use the true unit
            // numbers, and clobbering s0 would break a technique whose s0 is something else.
            if (!lbBoundAny && lu16Unit != 0 && !sbRealProgramsBound)
            {
                lpDevice->SetTexture(0, lpD3DTexture);
                lpDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                lpDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
                lpDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                lpDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                lpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
            }
            lbBoundAny = true;
        }

        sbMaterialTextureBound = lbBoundAny;
        return lbBoundAny;
    }

    void* WorldVd32_GetDeclaration(const void* lpVdImage, u32* lpuStride)
    {
        *lpuStride = 0;
        sbLastDeclHasTexcoord0 = false;
        suLastDeclUsageMask    = 0;
        suLastDeclSourceStride = 0;
        suLastDeclDec3nCount   = 0;
        if (lpVdImage == nullptr)
            return nullptr;

        std::unordered_map<const void*, Vd32Cached>::iterator lIt = sVdCache.find(lpVdImage);
        if (lIt != sVdCache.end())
        {
            *lpuStride = lIt->second.muStride;
            sbLastDeclHasTexcoord0 = lIt->second.mbHasTexcoord0;
            suLastDeclUsageMask    = lIt->second.muUsageMask;
            suLastDeclSourceStride = lIt->second.muSourceStride;
            suLastDeclDec3nCount   = lIt->second.muDec3nCount;
            std::memcpy(sau16LastDeclDec3nOffsets, lIt->second.mau16Dec3nOffsets,
                        sizeof(sau16LastDeclDec3nOffsets));
            su8LastDeclPositionType          = lIt->second.mu8PositionType;
            suLastDeclPositionSourceOffset   = lIt->second.mu16PositionSourceOffset;
            suLastDeclPositionExpandedOffset = lIt->second.mu16PositionExpandedOffset;
            spLastDeclElements               = lIt->second.macElements;
            return lIt->second.mpDeclaration;
        }

        Vd32Cached lEntry = {};
        // Zero would read as D3DDECLTYPE_FLOAT1; "no POSITION0 resolved" is UNUSED.
        lEntry.mu8PositionType = D3DDECLTYPE_UNUSED;
        const u8* lpImage = static_cast<const u8*>(lpVdImage);
        u16 lu16NumElements;
        std::memcpy(&lu16NumElements, lpImage + 0x08, 2);

        D3DVERTEXELEMENT9 laElements[17];
        u16 lau16OriginalOffsets[16] = {};
        bool lbOk = (lu16NumElements > 0 && lu16NumElements <= 16);
        IDirect3DDevice9* lpDevice = Dev();
        bool lbSupportsDec3n = true;
        if (lpDevice != nullptr)
        {
            D3DCAPS9 lCaps = {};
            if (SUCCEEDED(lpDevice->GetDeviceCaps(&lCaps)))
                lbSupportsDec3n = (lCaps.DeclTypes & D3DDTCAPS_DEC3N) != 0;
        }
        for (u32 lu = 0; lbOk && lu < lu16NumElements; ++lu)
        {
            // Element record (16 bytes): +0 u16 stream, +2 u16 offset, +4 u32 Xenon type,
            // then THREE BYTES +8 method / +9 usage / +10 usageIndex (+11 pad), +0xC u32 flag.
            // The byte triple is attested twice: tools/assets/bundles/world_type_transcode.py
            // plan_vertexdescriptor ("+8 u8[4] method/usage/usageIndex bytes") and this TU's own
            // D3DDevice_CreateVertexDeclaration, which reads the 12-byte Xenon form's
            // lpIn[6]/[7]/[8] as method/usage/usageIndex right after the 4-byte type.
            //
            // This used to read a u16 usage at +8 and a u16 usageIndex at +10. On a
            // little-endian image the u16 at +8 is {method, usage} and the cast to BYTE kept
            // the METHOD -- i.e. every element came out as usage 0 (D3DDECLUSAGE_POSITION)
            // with the usage index in the wrong lane. The world drew (POSITION0 was correct by
            // accident) but no TEXCOORD/NORMAL/COLOR element ever reached the declaration, so
            // no shader could sample a texture. Bug class (c).
            const u8* lpElem = lpImage + 0x10 + 16 * lu;
            u16 lu16Stream, lu16Offset;
            u32 luXenonType;
            std::memcpy(&lu16Stream,  lpElem + 0, 2);
            std::memcpy(&lu16Offset,  lpElem + 2, 2);
            std::memcpy(&luXenonType, lpElem + 4, 4);
            const u8 lu8Usage      = lpElem[9];
            const u8 lu8UsageIndex = lpElem[10];

            u8 lu8Type;
            if (lu16Stream != 0 || !MapXenonDeclType(luXenonType, &lu8Type))
            {
                lbOk = false;
                break;
            }
            laElements[lu].Stream     = 0;
            laElements[lu].Offset     = lu16Offset;
            laElements[lu].Type       = lu8Type;
            lau16OriginalOffsets[lu]  = lu16Offset;
            // The +8 byte is the record's METHOD lane, but every world element observed uses
            // the plain (DEFAULT) method and D3D9 rejects a whole declaration on an
            // out-of-range method; keep DEFAULT and take only usage/usageIndex from the image.
            laElements[lu].Method     = D3DDECLMETHOD_DEFAULT;
            laElements[lu].Usage      = lu8Usage;
            laElements[lu].UsageIndex = lu8UsageIndex;
            if (lu8Usage == D3DDECLUSAGE_TEXCOORD && lu8UsageIndex == 0)
                lEntry.mbHasTexcoord0 = true;
            lEntry.muUsageMask |= DeclUsageBit(lu8Usage, lu8UsageIndex);
            if (lu8Type == D3DDECLTYPE_DEC3N && !lbSupportsDec3n)
                lEntry.mau16Dec3nOffsets[lEntry.muDec3nCount++] = lu16Offset;
        }

        if (lbOk)
        {
            // The descriptor stores the source stride immediately after its element
            // table. Keep that stride for reading the bundle, while the D3D declaration
            // and UP draw use a stride enlarged by eight bytes per DEC3N -> FLOAT3.
            lEntry.muSourceStride = lpImage[0x10 + 16 * lu16NumElements];

            // Keep the expansion offsets ordered even if a descriptor's elements are
            // not serialized by ascending byte offset.
            for (u32 lu = 1; lu < lEntry.muDec3nCount; ++lu)
            {
                const u16 lu16Offset = lEntry.mau16Dec3nOffsets[lu];
                u32 luInsert = lu;
                while (luInsert != 0
                       && lEntry.mau16Dec3nOffsets[luInsert - 1] > lu16Offset)
                {
                    lEntry.mau16Dec3nOffsets[luInsert] =
                        lEntry.mau16Dec3nOffsets[luInsert - 1];
                    --luInsert;
                }
                lEntry.mau16Dec3nOffsets[luInsert] = lu16Offset;
            }

            for (u32 lu = 0; lu < lEntry.muDec3nCount; ++lu)
            {
                if (static_cast<u32>(lEntry.mau16Dec3nOffsets[lu]) + 4u
                        > lEntry.muSourceStride
                    || (lu != 0
                        && lEntry.mau16Dec3nOffsets[lu]
                           < lEntry.mau16Dec3nOffsets[lu - 1] + 4u))
                {
                    lbOk = false;
                    break;
                }
            }

            for (u32 lu = 0; lbOk && lu < lu16NumElements; ++lu)
            {
                u32 luExpansionBefore = 0;
                for (u32 luPacked = 0; luPacked < lEntry.muDec3nCount; ++luPacked)
                {
                    if (lEntry.mau16Dec3nOffsets[luPacked] < lau16OriginalOffsets[lu])
                        ++luExpansionBefore;
                }
                laElements[lu].Offset = static_cast<WORD>(
                    static_cast<u32>(lau16OriginalOffsets[lu]) + 8u * luExpansionBefore);
                if (laElements[lu].Type == D3DDECLTYPE_DEC3N && !lbSupportsDec3n)
                    laElements[lu].Type = D3DDECLTYPE_FLOAT3;
            }

            lEntry.muStride = lEntry.muSourceStride + 8u * lEntry.muDec3nCount;
            const D3DVERTEXELEMENT9 lEnd = D3DDECL_END();
            laElements[lu16NumElements] = lEnd;

            // [DIAG wheels] the resolved-declaration record (see Vd32Cached's banner). This
            // is the only point where the SOURCE offsets (lau16OriginalOffsets, the bundle's
            // own byte offsets) and the EXPANDED ones (laElements, after the DEC3N widening)
            // are both in hand, so both are captured here. Nothing below reads them.
            if (lbOk)
            {
                int liWritten = 0;
                for (u32 lu = 0; lu < lu16NumElements; ++lu)
                {
                    if (laElements[lu].Usage == D3DDECLUSAGE_POSITION
                        && laElements[lu].UsageIndex == 0)
                    {
                        lEntry.mu8PositionType            = laElements[lu].Type;
                        lEntry.mu16PositionSourceOffset   = lau16OriginalOffsets[lu];
                        lEntry.mu16PositionExpandedOffset = laElements[lu].Offset;
                    }
                    if (liWritten >= 0
                        && static_cast<size_t>(liWritten) < sizeof(lEntry.macElements))
                    {
                        const int liOne = std::snprintf(
                            lEntry.macElements + liWritten,
                            sizeof(lEntry.macElements) - static_cast<size_t>(liWritten),
                            "%s%s%u:%s@%u->%u",
                            (lu != 0) ? " " : "",
                            DeclUsageName(laElements[lu].Usage),
                            static_cast<unsigned>(laElements[lu].UsageIndex),
                            DeclTypeName(laElements[lu].Type),
                            static_cast<unsigned>(lau16OriginalOffsets[lu]),
                            static_cast<unsigned>(laElements[lu].Offset));
                        if (liOne < 0)
                        {
                            break;      // encoding error: keep what is already there
                        }
                        liWritten += liOne;   // may exceed the buffer; the guard above stops
                    }
                }
            }

            if (lbOk && lpDevice != nullptr)
            {
                const HRESULT lhr =
                    lpDevice->CreateVertexDeclaration(laElements, &lEntry.mpDeclaration);
                if (FAILED(lhr) || lEntry.mpDeclaration == nullptr)
                {
                    lEntry.mpDeclaration  = nullptr;
                    lEntry.mbHasTexcoord0 = false;
                    lEntry.muUsageMask    = 0;
                    static bool sbLoggedFailure = false;
                    if (!sbLoggedFailure)
                    {
                        sbLoggedFailure = true;
                        D3DCAPS9 lCaps = {};
                        const HRESULT lhrCaps = lpDevice->GetDeviceCaps(&lCaps);
                        char lacMsg[1024];
                        int liWritten = std::snprintf(
                            lacMsg, sizeof(lacMsg),
                            "[WorldVd32] CreateVertexDeclaration FAILED hr=0x%08X"
                            " capsHr=0x%08X declTypes=0x%08X elems=%u:",
                            static_cast<unsigned>(lhr), static_cast<unsigned>(lhrCaps),
                            static_cast<unsigned>(lCaps.DeclTypes),
                            static_cast<unsigned>(lu16NumElements));
                        for (u32 lu = 0; lu < lu16NumElements
                                       && liWritten > 0
                                       && static_cast<size_t>(liWritten) < sizeof(lacMsg); ++lu)
                        {
                            const D3DVERTEXELEMENT9& lrElement = laElements[lu];
                            liWritten += std::snprintf(
                                lacMsg + liWritten, sizeof(lacMsg) - liWritten,
                                " [s%u o%u t%u m%u u%u i%u]",
                                static_cast<unsigned>(lrElement.Stream),
                                static_cast<unsigned>(lrElement.Offset),
                                static_cast<unsigned>(lrElement.Type),
                                static_cast<unsigned>(lrElement.Method),
                                static_cast<unsigned>(lrElement.Usage),
                                static_cast<unsigned>(lrElement.UsageIndex));
                        }
                        const size_t luEnd = (liWritten > 0
                                             && static_cast<size_t>(liWritten) < sizeof(lacMsg) - 1)
                            ? static_cast<size_t>(liWritten) : sizeof(lacMsg) - 2;
                        lacMsg[luEnd] = '\n';
                        lacMsg[luEnd + 1] = '\0';
                        CgsDev::Log::WriteToLog(lacMsg);
                    }
                }
            }
        }
        else
        {
            LogOnce("vd32", "[WorldVd32] unsupported vertex-descriptor image (multi-stream/unknown type) - decl skipped\n");
        }

        if (!lbOk)
        {
            lEntry.mbHasTexcoord0 = false;
            lEntry.muUsageMask    = 0;
        }
        {
            // [DIAG one-shot] first vertex declaration built for a world mesh.
            static bool sbDiagVd = false;
            if (!sbDiagVd)
            {
                sbDiagVd = true;
                char lacMsg[256];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[WorldVd32] ok=%d elems=%u stride=%u uv0=%d\n",
                              (int)lbOk, (unsigned)lu16NumElements, lEntry.muStride,
                              (int)lEntry.mbHasTexcoord0);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        // Bind to the STORED entry, not the local: spLastDeclElements below has to point at
        // the cache's own buffer (the cache-hit path hands out the same pointer).
        const Vd32Cached& lrStored = (sVdCache[lpVdImage] = lEntry);
        *lpuStride = lrStored.muStride;
        sbLastDeclHasTexcoord0 = lrStored.mbHasTexcoord0;
        suLastDeclUsageMask    = lrStored.muUsageMask;
        suLastDeclSourceStride = lrStored.muSourceStride;
        suLastDeclDec3nCount   = lrStored.muDec3nCount;
        std::memcpy(sau16LastDeclDec3nOffsets, lrStored.mau16Dec3nOffsets,
                    sizeof(sau16LastDeclDec3nOffsets));
        su8LastDeclPositionType          = lrStored.mu8PositionType;
        suLastDeclPositionSourceOffset   = lrStored.mu16PositionSourceOffset;
        suLastDeclPositionExpandedOffset = lrStored.mu16PositionExpandedOffset;
        spLastDeclElements               = lrStored.macElements;
        if (lEntry.muDec3nCount != 0 && lEntry.mpDeclaration != nullptr)
            LogOnce("vd32dec3n", "[WorldVd32] driver lacks DEC3N; packed normals expanded to FLOAT3\n");
        return lEntry.mpDeclaration;
    }

    // [PC bring-up shim] Choose the fallback pair for the mesh just bound: the
    // TEXTURED variant only when the material bound a texture AND this mesh's
    // declaration actually supplies TEXCOORD0 (see Vd32Cached::mbHasTexcoord0).
    void WorldFallbackShader_SelectForMesh()
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr)
        {
            sbForceFallbackNextMesh = false;
            return;
        }

        // ⭐ THE DECISION MOVED HERE (2026-08-17, wheels). sbForceFallbackNextMesh no longer
        // means "force" -- it means "this mesh is CONSOLE-INSTANCED", and the leaf decides,
        // because only the leaf knows what the technique's programs turned out to be. See the
        // banner on WorldFallbackShader_MarkInstancedMesh for why the old unconditional force
        // was wrong for the wheels.
        const bool lbInstancedMesh = sbForceFallbackNextMesh;
        sbForceFallbackNextMesh = false;
        const bool lbForceFallback = lbInstancedMesh && !sbRealVsHasWorld;
        if (lbForceFallback)
        {
            WorldShader_ReportFallback(spCurrentTechniqueName,
                                       E_WORLD_FALLBACK_INSTANCED_NO_WORLD);
        }

        // The technique's REAL programs are bound and this mesh's declaration supplies every
        // input the vertex program declares -> nothing to choose, keep them.
        // (A vs_3_0 input the declaration lacks makes the D3D9 draw fail silently, so a mesh
        // that cannot feed the real shader has to drop back to the flagged fallback pair --
        // which then needs its own c0..c3 WVP restored, because the technique's constants
        // have just been uploaded over those registers.)
        const bool lbRealUsable = !lbForceFallback
                               && sbRealProgramsBound
                               && (suRealVsInputMask & ~suLastDeclUsageMask) == 0;
        {
            // [DIAG] one-shot tally of what the world meshes drew with. Printed after the
            // first 4096 selections so a single line covers a whole frame's worth of the list.
            static u32 suSeen = 0u, suReal = 0u, suRealBadDecl = 0u,
                       suTex = 0u, suNoTexture = 0u, suNoUv = 0u;
            if (suSeen < 4096u)
            {
                ++suSeen;
                if (lbRealUsable)                 ++suReal;
                else if (sbRealProgramsBound)     ++suRealBadDecl;
                else if (sbMaterialTextureBound && sbLastDeclHasTexcoord0) ++suTex;
                else if (!sbMaterialTextureBound) ++suNoTexture;
                else                              ++suNoUv;
                if (suSeen == 4096u)
                {
                    char lac[256];
                    std::snprintf(lac, sizeof(lac),
                        "[WorldShader] mesh shader tally: REAL=%u real-but-decl-short=%u "
                        "fallback textured=%u fallback flat(no material texture)=%u "
                        "fallback flat(no TEXCOORD0)=%u of %u\n",
                        suReal, suRealBadDecl, suTex, suNoTexture, suNoUv, suSeen);
                    CgsDev::Log::WriteToLog(lac);
                }
            }
        }
        if (lbRealUsable)
        {
            // A local latch, NOT LogOnce: this is the common case, so every world mesh of
            // every pass used to hash a const char* through LogOnce's unordered_map on its
            // way to a bind that already happened. (LogOnce itself is unchanged -- its other
            // sites are all genuinely rare.)
            static bool sbLoggedRealDraw = false;
            if (!sbLoggedRealDraw)
            {
                sbLoggedRealDraw = true;
                CgsDev::Log::WriteToLog("[WorldShader] REAL technique shaders are drawing the world\n");
            }
            // Re-assert: an earlier mesh of this same technique may have swapped in the
            // fallback pair.
            lpDevice->SetVertexShader(spRealVs);
            lpDevice->SetPixelShader(spRealPs);

            // ---- [DIAG wheels] a CONSOLE-INSTANCED mesh drawing with its OWN programs.
            // Latched on the outcome BITS, never on a "printed once" bool: the wheel renderable
            // has seven meshes with seven materials, and a run where one of them is untextured
            // and the rest are must not be reported as "the wheels are textured".
            // ⛔ DELETE-WHEN the wheels are confirmed textured on a booted run.
            if (lbInstancedMesh)
            {
                const u32 luOutcome = (sbLastDeclHasTexcoord0 ? 1u : 0u)
                                    | (spTechniqueUnit0Texture != nullptr ? 2u : 0u)
                                    | (spTechniqueFirstTexture != nullptr ? 4u : 0u);
                static u32 suSeenRealOutcomes = 0u;
                if ((suSeenRealOutcomes & (1u << luOutcome)) == 0u)
                {
                    suSeenRealOutcomes |= (1u << luOutcome);
                    char lacMsg[288];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[wheel-shade] realProgram=1 technique=%s declUv0=%d techUnit0=%d"
                        " techFirst=%d stride=%u declMask=0x%llX vsInputs=0x%llX\n",
                        (spCurrentTechniqueName != nullptr) ? spCurrentTechniqueName
                                                            : "<unnamed technique>",
                        (int)sbLastDeclHasTexcoord0,
                        spTechniqueUnit0Texture != nullptr, spTechniqueFirstTexture != nullptr,
                        (unsigned)suLastDeclSourceStride,
                        (unsigned long long)suLastDeclUsageMask,
                        (unsigned long long)suRealVsInputMask);
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
            return;
        }
        if (sbRealProgramsBound)
        {
            LogOnce("realdecl",
                    "[WorldShader] mesh declaration lacks an input the technique's vertex program"
                    " declares - flagged fallback used for it\n");
            WorldShader_ReportFallback(spCurrentTechniqueName,
                                       E_WORLD_FALLBACK_DECLARATION_SHORT);
        }

        if (!CompileFallbackShaders(lpDevice))
            return;

        // ⭐ The diffuse for THIS mesh, from whichever of the two sampler paths actually ran.
        // spTechniqueUnit0Texture / spTechniqueFirstTexture come from the REAL path
        // (Device::BindTechniqueSamplers -> WorldShader_BindTextureUnit); sbMaterialTextureBound
        // comes from the bring-up path, which already mirrors its first texture onto s0 itself.
        // See the two pointers' banner for why asking only the second was the wheels defect.
        IDirect3DBaseTexture9* const lpTechniqueTexture =
            (spTechniqueUnit0Texture != nullptr) ? spTechniqueUnit0Texture
                                                 : spTechniqueFirstTexture;
        const bool lbHaveDiffuse = sbMaterialTextureBound || lpTechniqueTexture != nullptr;

        const bool lbTextured = lbHaveDiffuse && sbLastDeclHasTexcoord0
                             && spFallbackTexVs != nullptr && spFallbackTexPs != nullptr;
        if (lbTextured)
        {
            LogOnce("wtex", "[WorldFallback] TEXTURED world draws live (material samplers bound)\n");
            // The fallback pixel shader has a single sampler, s0. When the technique's own
            // sampler list DID name unit 0 the right texture is already there and nothing is
            // touched; when it did not, mirror the technique's first texture onto s0.
            // ⭐ Why that mirror cannot break a real draw: BindTechniqueSamplers binds only the
            // units the TECHNIQUE names, so "no unit 0 bound" means this technique's programs do
            // not read s0 -- there is nothing there to clobber. A different technique re-binds
            // its own samplers on the next technique change.
            if (spTechniqueUnit0Texture == nullptr && lpTechniqueTexture != nullptr)
            {
                lpDevice->SetTexture(0, lpTechniqueTexture);
                lpDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                lpDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
                lpDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                lpDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                lpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
            }
            lpDevice->SetVertexShader(spFallbackTexVs);
            static const bool sbUvDebug = (::GetEnvironmentVariableA("BRN_WORLD_UVDEBUG", nullptr, 0) != 0);
            lpDevice->SetPixelShader((sbUvDebug && spFallbackUvDebugPs != nullptr)
                                     ? spFallbackUvDebugPs : spFallbackTexPs);
        }
        else
        {
            lpDevice->SetVertexShader(spFallbackVs);
            lpDevice->SetPixelShader(spFallbackPs);
        }

        // ---- [DIAG wheels leg 1] the FORCED (console-instanced) selection, one line per
        // DISTINCT outcome. ⛔ DELETE-WHEN the wheels are confirmed textured on a booted run.
        // Latched on the outcome BITS, never on a "printed once" bool: the wheel renderable has
        // seven meshes with seven materials, and a run where one of them has no UVs and the rest
        // do must not be reported as "the wheels are textured".
        if (lbInstancedMesh)
        {
            const u32 luOutcome = (lbTextured ? 1u : 0u)
                                | (sbLastDeclHasTexcoord0 ? 2u : 0u)
                                | (lbHaveDiffuse ? 4u : 0u)
                                | (spTechniqueUnit0Texture != nullptr ? 8u : 0u)
                                | (sbMaterialTextureBound ? 16u : 0u);
            static u32 suSeenForcedOutcomes = 0u;
            if ((suSeenForcedOutcomes & (1u << luOutcome)) == 0u)
            {
                suSeenForcedOutcomes |= (1u << luOutcome);
                char lacMsg[320];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[wheel-shade] realProgram=0 technique=%s vsHasWorld=%d textured=%d"
                    " declUv0=%d haveDiffuse=%d techUnit0=%d techFirst=%d bringUpFlag=%d"
                    " stride=%u declMask=0x%llX vsInputs=0x%llX\n",
                    (spCurrentTechniqueName != nullptr) ? spCurrentTechniqueName
                                                        : "<unnamed technique>",
                    (int)sbRealVsHasWorld,
                    (int)lbTextured, (int)sbLastDeclHasTexcoord0, (int)lbHaveDiffuse,
                    spTechniqueUnit0Texture != nullptr, spTechniqueFirstTexture != nullptr,
                    (int)sbMaterialTextureBound, (unsigned)suLastDeclSourceStride,
                    (unsigned long long)suLastDeclUsageMask,
                    (unsigned long long)suRealVsInputMask);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
    }

    // [DIAG] Set by the immediate-mode leaf's constant flush, which is the last thing that
    // happens before an immediate-mode draw; consumed (and cleared) by the next draw. Lets a
    // probe distinguish the sky-dome draw from a world mesh that happens to share its
    // 20-byte stride.
    bool sbNextDrawIsImmediateMode = false;
    void WorldDraw_MarkImmediateMode()
    {
        sbNextDrawIsImmediateMode = true;
    }

    // [DIAG wheels] Set by Device::DrawIndexedMeshPC for a mesh whose mu8InstanceCount > 1
    // (i.e. the console pre-replicated instanced geometry -- on this build that is the wheel
    // renderable and nothing else). Consumed and cleared by the next WorldDraw_IndexedUP, which
    // reports everything that decides whether the draw produces pixels. DELETE once the wheels
    // are confirmed drawing correctly.
    bool sbNextDrawIsInstanced = false;
    void WorldDraw_MarkInstanced()
    {
        sbNextDrawIsInstanced = true;
    }

    // ========================================================================
    // CONSOLE-INSTANCED MESHES: WHY THIS IS A MARK AND NOT A FORCE.
    //
    // A mesh with mu8InstanceCount > 1 carries geometry the Xenos instancing shader decodes
    // by hand: one copy of the vertices, an index buffer of N slices, and the instance number
    // in the HIGH BITS of every index (index = (instance << 12) | vertexIndex). DECODED from
    // the console wheel vertex program (X360 SHADERS.BNDL resource AB4DE44C, disassembled with
    // tools/assets/shaders/xenos.py), the shader does, per vertex:
    //     instance = trunc((index + 0.5) * 1/4096)          [slots 7..9, c254 = {1/4096, .5, ..}]
    //     a0       = instance                               [slot 10  maxas]
    //     row      = trunc(InstancingIndexArray[a0].w) * 4  [slots 11..12, c255.x = 4]
    //     vertex   = index - instance * 4096                [slot 13, c255.y = 4096]
    //     a0       = row                                    [slot 18  maxas]
    //     worldPos = pos.x*c[a0+4] + pos.y*c[a0+5] + pos.z*c[a0+6] + c[a0+7]   [slots 19..21]
    // i.e. InstancingMatrixArray (c4, 5 x 4 registers) IS the per-draw object matrix and the
    // .w lane of InstancingIndexArray (c24) is which entry to take (0,1,2,3,4 -- CgsModel.cpp
    // gaTheFirstFewIntegersAsVectors, so the indirection is the identity). Slot 28 also
    // exports InstancingIndexArray[instance].x on interpolator 2 lane z -- the per-wheel value
    // RenderRaceCar puts there, its SPIN BLUR FACTOR.
    //
    // ⛔ WHAT THE PREVIOUS VERSION OF THIS BANNER GOT WRONG. It said the PC programs "do not
    // implement it" and forced EVERY console-instanced mesh onto the flagged fallback pair.
    // The premise does not hold for the four VEHICLE WHEEL techniques (nor for the 15 other
    // instancing consumers): their recompiled PC vertex programs are the NON-INSTANCED twins,
    // which take exactly the same two data through their own names --
    //     `world` (c20, 4 registers)  and  `g_wheelConstants` (c31, 1 register, read as
    //     `mov o3.z, c31.x`, the same interpolator lane the console exports).
    // Nothing published them, which is why binding the real program produced zero pixels
    // (task #133) -- not a missing feature, a missing PUBLISH. CgsShaderConstants.cpp's
    // instancing substitute table now binds the technique's own two constants to those two
    // registers, and DrawRenderable::Interpret points them at the expanded draw's own entry.
    //
    // So this function only MARKS the mesh; WorldFallbackShader_SelectForMesh keeps the real
    // programs whenever the bound vertex program declares `world`, and drops to the fallback
    // pair only when it does not -- in which case nothing can transform the geometry and the
    // fallback's own c240..c243 WVP (published by Device::SetObjectTransformPC, which the
    // expansion already fills per instance) is the only way to draw it at all.
    //
    // RESIDUAL (verify F3, car+lights step 1): the "keep the real programs" branch assumes the
    // per-instance `world` WAS published for this mesh, i.e. that its object command carried
    // constant 6 (InstancingMatrixArray) and Interpret pointed the expanded draw at entry i. A
    // console-instanced mesh whose object has NO constant 6 would keep its real programs and
    // draw with whatever c20..c23 the previous draw left. Not reachable today --
    // Model::SetupShaderConstantsForInstancing always publishes 6 and 7 before an instanced
    // submit -- so the flag is not plumbed; if a new instanced producer ever skips the
    // publish, carry a `lbPerInstanceWorldPublished` bool from Interpret and force the
    // fallback when it is false.
    //
    // DELETE the mark when a real instanced draw path exists on the D3D9 back end.
    // (the flag itself lives with the other per-mesh selection state, above)
    // ========================================================================
    void WorldFallbackShader_MarkInstancedMesh()
    {
        sbForceFallbackNextMesh = true;
    }


    void WorldDraw_SetIndexSource(const void* lpIndexBufferHeader)
    {
        spIndexSource = static_cast<const IndexBufferHeader32*>(lpIndexBufferHeader);
    }

    // The DISPATCH path's publisher: `luStride` is the EXPANDED stride WorldVd32_GetDeclaration
    // just returned, and the three DEC3N fields it left in the suLastDecl* trio describe the very
    // same declaration. The two halves must be published together -- see the raw variant below.
    void WorldDraw_SetVertexSource(const void* lpVertexBufferHeader, u32 luStride)
    {
        spVertexSource = static_cast<const VertexBufferHeader32*>(lpVertexBufferHeader);
        sbVertexSourceFastSet = false;                 // -> the RETAINED draw path
        suVertexStride = luStride;
        suVertexSourceStride = suLastDeclSourceStride;
        suVertexDec3nCount   = suLastDeclDec3nCount;
        std::memcpy(sau16VertexDec3nOffsets, sau16LastDeclDec3nOffsets,
                    sizeof(sau16VertexDec3nOffsets));
    }

    // ⭐ FIXED 2026-08-12 (exploding-geometry wave). The FAST-SET path's publisher.
    //
    // THE BUG: D3DDevice_SetStreamSource -- the Xenon fast-set binder the sky dome, MeshHelper,
    // the Lion particle renderer, XCam and shadow::Device's own stream-array walk all go through
    // -- used to call WorldDraw_SetVertexSource above. That takes the stride from its CALLER but
    // reads the source stride and the whole DEC3N expansion plan out of the suLastDecl* trio,
    // which ONLY WorldVd32_GetDeclaration ever writes. Nothing on the fast-set path goes through
    // that function (its declarations are built by D3DDevice_CreateVertexDeclaration, a different
    // function that maps DEC3N straight through and expands nothing), so every fast-set draw
    // silently inherited the expansion plan of whatever DISPATCH mesh was bound last.
    //
    // MEASURED, on the frame the car is on screen ([carverts] probe, since removed): stride 20 published by the
    // caller against a stale sourceStride of 24 and a stale dec3nCount of 1 -- a triple that
    // cannot come from one declaration (the expanded stride is always sourceStride + 8 per DEC3N,
    // i.e. 32, never 20). Two things then went wrong at once:
    //   * the vertex COUNT is muSize / sourceStride, so a 990-vertex buffer was reported as 825
    //     and 388 of the run's indices fell "outside" it; and
    //   * dec3nCount != 0 sent the buffer through the DEC3N expansion loop, which re-packs each
    //     vertex reading at a 24-byte source stride and writing ~32 bytes into 20-byte
    //     destination records -- every vertex stomping the next, and the scratch overrun at the
    //     end. That is the stretched-ribbon geometry.
    //
    // The fast-set path needs a plan that describes ITS OWN buffer: the stride the caller gave,
    // no expansion. A stride of 0 (the console's "clear the previous binding" call) stays 0 and
    // the draw path's own early-out catches it.
    //
    // ⚠ KNOWN RESIDUAL, deliberately not papered over: because this path performs no expansion,
    // a fast-set declaration that really does carry a DEC3N element cannot be created on a driver
    // without D3DDTCAPS_DEC3N -- CreateVertexDeclaration fails, sbWorldDeclarationValid goes
    // false and the draw is SKIPPED. Skipping is honest; drawing it through another mesh's plan
    // was not. Give this path its own descriptor-driven expansion when one of those meshes needs
    // it on such a driver.
    void WorldDraw_SetVertexSourceRaw(const void* lpVertexBufferHeader, u32 luStride)
    {
        spVertexSource        = static_cast<const VertexBufferHeader32*>(lpVertexBufferHeader);
        sbVertexSourceFastSet = true;                  // -> the DrawIndexedPrimitiveUP path
        suVertexStride        = luStride;
        suVertexSourceStride  = luStride;
        suVertexDec3nCount   = 0;
        std::memset(sau16VertexDec3nOffsets, 0, sizeof(sau16VertexDec3nOffsets));
    }

    // The technique's primitive-reset render state, republished on every technique change.
    void WorldDraw_SetPrimitiveReset(bool lbEnabled, u32 luResetIndex)
    {
        spResetEnabled = lbEnabled;
        suResetIndex   = luResetIndex;
    }

    void WorldDraw_IndexedUP(u32 luPrimTypeXenon, u32 luBaseVertexIndex,
                             u32 luStartIndex, u32 luIndexCount)
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr || spIndexSource == nullptr || spVertexSource == nullptr
            || suVertexStride == 0 || suVertexSourceStride == 0
            || !sbWorldDeclarationValid)
        {
            LogOnce("updraw", "[WorldDraw] draw skipped: no device/geometry/declaration stash\n");
            return;
        }

        const void* lpIndexData  = ResolveGuestPointer(spIndexSource->muBaseAddress);
        const void* lpVertexData = ResolveGuestPointer(spVertexSource->muBaseAddress);
        if (lpIndexData == nullptr || lpVertexData == nullptr)
        {
            LogOnce("upnull", "[WorldDraw] draw skipped: buffer base unresolved (x64 widening seam?)\n");
            return;
        }

        // Index width from the console header Common tag (0x20000000 = 16-bit,
        // 0xC0000000 = 32-bit; the high bit selects the width).
        const bool lb32Bit = (spIndexSource->muCommon & 0x80000000u) != 0;
        const u32  luIndexSize = lb32Bit ? 4u : 2u;

        D3DPRIMITIVETYPE lePrim;
        UINT luPrimCount;
        if (!MapPrimitive(luPrimTypeXenon, luIndexCount, &lePrim, &luPrimCount))
            return;


        const u8* lpIndices  = static_cast<const u8*>(lpIndexData) + luStartIndex * luIndexSize;
        const UINT luNumVertices = spVertexSource->muSize / suVertexSourceStride;
        if (luBaseVertexIndex >= luNumVertices)
            return;

        // ---- THE SUBMISSION ROUTE -------------------------------------------------------
        // FLAG PC-platform leaf: the DISPATCH publisher binds the bundle's own STATIC
        // serialised buffers -- on the console they are GPU memory and the draw fetches
        // straight out of them, so nothing is ever reworked per draw. D3D9 cannot bind host
        // memory, so those buffers are mirrored once into device buffers (including the
        // DEC3N expansion and the primitive-reset strip re-cut, neither of which D3D9 can do
        // at fetch time) and drawn with SetStreamSource/SetIndices/DrawIndexedPrimitive.
        // See WorldGeometryPCLeaf.cpp. The FAST-SET publisher's runs are rebuilt every frame
        // and stay on DrawIndexedPrimitiveUP below, which is exactly what UP is for.
        WorldGeometryDraw lRetained;
        std::memset(&lRetained, 0, sizeof(lRetained));
        bool lbRetained = false;
        if (!sbVertexSourceFastSet)
        {
            WorldGeometryVertexPlan lVertexPlan;
            std::memset(&lVertexPlan, 0, sizeof(lVertexPlan));
            lVertexPlan.mpHeader         = spVertexSource;
            lVertexPlan.mpData           = lpVertexData;
            lVertexPlan.muNumVertices    = luNumVertices;
            lVertexPlan.muSourceStride   = suVertexSourceStride;
            lVertexPlan.muExpandedStride = suVertexStride;
            // Clamped to the 16 offsets the plan (and sau16VertexDec3nOffsets) can carry, so
            // the cache key and the baked bytes describe the same plan.
            lVertexPlan.muDec3nCount     = suVertexDec3nCount < 16u ? suVertexDec3nCount : 16u;
            std::memcpy(lVertexPlan.mau16Dec3nOffsets, sau16VertexDec3nOffsets,
                        sizeof(lVertexPlan.mau16Dec3nOffsets));

            WorldGeometryIndexPlan lIndexPlan;
            std::memset(&lIndexPlan, 0, sizeof(lIndexPlan));
            lIndexPlan.mpHeader               = spIndexSource;
            lIndexPlan.mpRun                  = lpIndices;
            lIndexPlan.muStartIndex           = luStartIndex;
            lIndexPlan.muIndexCount           = luIndexCount;
            lIndexPlan.muResetIndex           = suResetIndex;
            lIndexPlan.miMappedPrimitiveType  = static_cast<s32>(lePrim);
            lIndexPlan.muMappedPrimitiveCount = luPrimCount;
            lIndexPlan.mb32Bit                = lb32Bit;
            lIndexPlan.mbResetEnabled         = spResetEnabled;

            const EWorldGeometryPrepare lePrepare =
                WorldGeometry_Prepare(lVertexPlan, lIndexPlan, &lRetained);
            if (lePrepare == E_WORLDGEOMETRY_SKIP)
            {
                // The strip run expanded to no triangles at all -- the same case the UP
                // path's `luTriangles == 0` early-out covers. Consume the per-draw marks
                // the skipped draw would have consumed.
                LogOnce("retainskip", "[WorldDraw] retained strip expansion produced ZERO"
                                      " triangles - draw skipped\n");
                sbNextDrawIsInstanced     = false;
                sbNextDrawIsImmediateMode = false;
                return;
            }
            lbRetained = (lePrepare == E_WORLDGEOMETRY_READY);
            if (!lbRetained)
            {
                // A plain latch, NOT LogOnce: this sits on the per-draw path and LogOnce is
                // a hash-map probe.
                static bool sbRetainOffLogged = false;
                if (!sbRetainOffLogged)
                {
                    sbRetainOffLogged = true;
                    CgsDev::Log::WriteToLog("[WorldDraw] no retained mirror for a dispatch"
                                            " mesh - DrawIndexedPrimitiveUP kept for it\n");
                }
            }
        }

        const u8* lpVertices = nullptr;
        if (!lbRetained && suVertexDec3nCount != 0)
        {
            // D3DDECLTYPE_DEC3N is a signed-normalized 10:10:10 value. Expand
            // each component exactly as the fixed-function vertex fetch would:
            // sign-extend ten bits and normalize by 511, clamping -512 to -1.
            sVertexFormatScratch.resize(
                static_cast<size_t>(luNumVertices) * suVertexStride);
            const u8* lpSource = static_cast<const u8*>(lpVertexData);
            u8* lpDestination = &sVertexFormatScratch[0];
            for (u32 luVertex = 0; luVertex < luNumVertices; ++luVertex)
            {
                const u8* lpSourceVertex = lpSource + luVertex * suVertexSourceStride;
                u8* lpDestinationVertex = lpDestination + luVertex * suVertexStride;
                u32 luSourceCursor = 0;
                u32 luDestinationCursor = 0;
                for (u32 luPacked = 0; luPacked < suVertexDec3nCount; ++luPacked)
                {
                    const u32 luOffset = sau16VertexDec3nOffsets[luPacked];
                    const u32 luPrefixBytes = luOffset - luSourceCursor;
                    std::memcpy(lpDestinationVertex + luDestinationCursor,
                                lpSourceVertex + luSourceCursor, luPrefixBytes);
                    luDestinationCursor += luPrefixBytes;

                    u32 luValue;
                    std::memcpy(&luValue, lpSourceVertex + luOffset, sizeof(luValue));
                    f32 lafNormal[3];
                    for (u32 luComponent = 0; luComponent < 3; ++luComponent)
                    {
                        int liComponent = static_cast<int>(
                            (luValue >> (10u * luComponent)) & 0x3FFu);
                        if ((liComponent & 0x200) != 0)
                            liComponent -= 0x400;
                        lafNormal[luComponent] = liComponent <= -512
                            ? -1.0f : static_cast<f32>(liComponent) / 511.0f;
                    }
                    std::memcpy(lpDestinationVertex + luDestinationCursor,
                                lafNormal, sizeof(lafNormal));
                    luSourceCursor = luOffset + 4u;
                    luDestinationCursor += sizeof(lafNormal);
                }
                const u32 luTailBytes = suVertexSourceStride - luSourceCursor;
                std::memcpy(lpDestinationVertex + luDestinationCursor,
                            lpSourceVertex + luSourceCursor, luTailBytes);
            }
            lpVertices = &sVertexFormatScratch[0]
                       + static_cast<size_t>(luBaseVertexIndex) * suVertexStride;
        }
        else if (!lbRetained)
        {
            lpVertices = static_cast<const u8*>(lpVertexData)
                       + static_cast<size_t>(luBaseVertexIndex) * suVertexSourceStride;
        }

        // [DIAG wheels] scan the run this draw will actually submit, BEFORE the strip
        // expansion, so an out-of-range index value is visible as a number.
        const bool lbInstancedDiag = sbNextDrawIsInstanced;
        sbNextDrawIsInstanced = false;
        u32 luDiagMin = 0xFFFFFFFFu, luDiagMax = 0u, luDiagResets = 0u;
        if (lbInstancedDiag)
        {
            for (u32 luI = 0; luI < luIndexCount; ++luI)
            {
                const u32 luV = lb32Bit ? reinterpret_cast<const u32*>(lpIndices)[luI]
                                        : reinterpret_cast<const u16*>(lpIndices)[luI];
                if (luV == suResetIndex) { ++luDiagResets; continue; }
                if (luV < luDiagMin) luDiagMin = luV;
                if (luV > luDiagMax) luDiagMax = luV;
            }
        }

        // FLAG PC-platform leaf: honour the rasteriser state's primitive reset by
        // expanding the strip runs into a triangle list (see ExpandStripRunsToList).
        if (!lbRetained && spResetEnabled && lePrim == D3DPT_TRIANGLESTRIP)
        {
            const UINT luTriangles =
                lb32Bit
                    ? ExpandStripRunsToList<u32>(reinterpret_cast<const u32*>(lpIndices),
                                                 luIndexCount, suResetIndex, sResetScratch)
                    : ExpandStripRunsToList<u16>(reinterpret_cast<const u16*>(lpIndices),
                                                 luIndexCount, static_cast<u16>(suResetIndex),
                                                 sResetScratch);
            {
                // [DIAG one-shot] the first mesh whose strips were re-cut.
                static bool sbDiag = false;
                if (!sbDiag)
                {
                    sbDiag = true;
                    char lacMsg[192];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                                  "[WorldDraw] primitive reset honoured: %u strip indices"
                                  " -> %u list triangles (reset index 0x%X)\n",
                                  (unsigned)luIndexCount, (unsigned)luTriangles,
                                  (unsigned)suResetIndex);
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
            if (luTriangles == 0)
            {
                if (lbInstancedDiag)
                    LogOnce("wheelexp", "[wheel-draw] strip expansion produced ZERO triangles\n");
                return;
            }
            lePrim      = D3DPT_TRIANGLELIST;
            luPrimCount = luTriangles;
            lpIndices   = &sResetScratch[0];
        }

        // ---- what the probes below sample ------------------------------------------------
        // The clip-space tally and the wheel NDC witness both read POSITION for the first
        // three indices of the SUBMITTED run. On the UP path those are the scratch bytes
        // this function just built; on the retained path the run lives in a device buffer,
        // so the mirror hands back its first three index values and POSITION is read out of
        // the HOST source bytes at the SOURCE stride -- valid because POSITION is element 0
        // at byte 0, which is the same assumption both probes already made.
        const u8* lpProbeVertices;
        u32       luProbeStride;
        u32       lau32ProbeIndices[3] = { 0u, 0u, 0u };
        u32       luProbeIndexCount;
        if (lbRetained)
        {
            lePrim          = static_cast<D3DPRIMITIVETYPE>(lRetained.miPrimitiveType);
            luPrimCount     = lRetained.muPrimitiveCount;
            lpProbeVertices = static_cast<const u8*>(lpVertexData)
                            + static_cast<size_t>(luBaseVertexIndex) * suVertexSourceStride;
            luProbeStride   = suVertexSourceStride;
            luProbeIndexCount = lRetained.muFirstIndexCount;
            for (u32 luK = 0; luK < luProbeIndexCount && luK < 3u; ++luK)
                lau32ProbeIndices[luK] = lRetained.mau32FirstIndices[luK];
        }
        else
        {
            lpProbeVertices   = lpVertices;
            luProbeStride     = suVertexStride;
            luProbeIndexCount = (luIndexCount < 3u) ? luIndexCount : 3u;
            for (u32 luK = 0; luK < luProbeIndexCount; ++luK)
            {
                lau32ProbeIndices[luK] = lb32Bit ? reinterpret_cast<const u32*>(lpIndices)[luK]
                                                 : reinterpret_cast<const u16*>(lpIndices)[luK];
            }
        }

        // [DIAG wheels, env-gated] BRN_WHEEL_ZALWAYS=1 draws the console-instanced meshes with
        // the depth test defeated. It separates "the wheels do not draw" from "the wheels draw
        // and something in front of them wins the depth test". Off unless the variable is set.
        DWORD luSavedZFunc = 0;
        bool  lbZDefeated  = false;
        if (lbInstancedDiag)
        {
            static const int siWheelZAlways =
                (std::getenv("BRN_WHEEL_ZALWAYS") != nullptr) ? 1 : 0;
            if (siWheelZAlways != 0)
            {
                lpDevice->GetRenderState(D3DRS_ZFUNC, &luSavedZFunc);
                lpDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
                lbZDefeated = true;
            }
        }

        // ---- [DIAG, env-gated] the shadow-caster cull / depth-bias experiment -------------
        // OFF unless one of the three variables is set; with none set this block does nothing
        // and the draw is byte-for-byte what it was.
        //
        // WHY IT EXISTS. Once sampler 15 really compares (see ShadowSampler_ApplyState), the
        // next thing between "no shadows" and "correct shadows" is SHADOW ACNE, and this build
        // has nothing to prevent it: the shipped vertex shaders apply NO depth bias to the
        // shadow coordinate (verified across the world VS set -- o3.xyz is the raw
        // worldPos * ShadowMap_WorldToLight[0] with nothing added), so on the console the bias
        // must come from the pass's own rasteriser state. That is exactly the bracket
        // BrnRendererModule::RenderShadowMapPasses parks: sub_82276B38(dword_83010A3C) /
        // (dword_83010A38), whose two RasterizerState globals are DATA and are not in any IDA
        // export -- so their cull mode and depth bias are unattested and cannot be written
        // down honestly. What IS attested is the intent: ShadowMap::Construct sets
        // maTsmBBInfo[i].mbInvertCullMode = true for all three cascades
        // (BrnShadowMap.cpp:223, stb r11, 0x23C(r10)) -- the console renders casters with the
        // cull mode inverted, the classic depth-acne remedy.
        // These knobs let that be MEASURED without fabricating the constants:
        //   BRN_SHADOW_CULL=none|cw|ccw    force a cull mode for caster draws
        //   BRN_SHADOW_BIAS=<float>        D3DRS_DEPTHBIAS for caster draws
        //   BRN_SHADOW_SLOPEBIAS=<float>   D3DRS_SLOPESCALEDEPTHBIAS for caster draws
        // Whatever value turns out to be right is still a PARK until the two globals' bytes
        // are recovered -- a knob that produces a good picture is evidence, not attestation.
        // ---- [PROBE] the clip-space tally + the slot's first-draw device state -----------
        // Scoped by suShadowClipSlot alone, NOT by sbShadowPassActive: the world-opaque
        // CONTROL (slot 3) is measured by the same classifier on purpose, and a control that
        // only exercised half the instrument would not be a control. suShadowClipSlot is set
        // by ShadowProbe_Begin and cleared by ShadowProbe_End, so the scoping is exact.
        if (suShadowClipSlot < KU_SHADOW_TALLY_SLOTS)
        {
            ShadowClipTally& lrTally = saShadowClip[suShadowClipSlot];

            if (!lrTally.mbStateCaptured)
            {
                lrTally.mbStateCaptured = true;

                D3DVIEWPORT9 lViewport;
                std::memset(&lViewport, 0, sizeof(lViewport));
                lpDevice->GetViewport(&lViewport);
                lrTally.muVpX    = lViewport.X;
                lrTally.muVpY    = lViewport.Y;
                lrTally.muVpW    = lViewport.Width;
                lrTally.muVpH    = lViewport.Height;
                lrTally.mfVpMinZ = lViewport.MinZ;
                lrTally.mfVpMaxZ = lViewport.MaxZ;

                RECT lScissor;
                std::memset(&lScissor, 0, sizeof(lScissor));
                lpDevice->GetScissorRect(&lScissor);
                lrTally.miScissorL = static_cast<s32>(lScissor.left);
                lrTally.miScissorT = static_cast<s32>(lScissor.top);
                lrTally.miScissorR = static_cast<s32>(lScissor.right);
                lrTally.miScissorB = static_cast<s32>(lScissor.bottom);

                DWORD luState = 0;
                lpDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &luState); lrTally.muScissorEnable = luState;
                lpDevice->GetRenderState(D3DRS_ZENABLE,           &luState); lrTally.muZEnable       = luState;
                lpDevice->GetRenderState(D3DRS_ZWRITEENABLE,      &luState); lrTally.muZWrite        = luState;
                lpDevice->GetRenderState(D3DRS_ZFUNC,             &luState); lrTally.muZFunc         = luState;
                lpDevice->GetRenderState(D3DRS_CULLMODE,          &luState); lrTally.muCull          = luState;
                lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE,  &luState); lrTally.muColourWrite   = luState;
            }

            if (!sbHaveLastWvp)
            {
                ++lrTally.muNoWvp;
            }
            else
            {
                // The fitted extent, from the first record's matrix columns (see the banner).
                if (lrTally.muSampled == 0u)
                {
                    const f32 lfGradX = std::sqrt(safLastWvp[0] * safLastWvp[0]
                                                + safLastWvp[4] * safLastWvp[4]
                                                + safLastWvp[8] * safLastWvp[8]);
                    const f32 lfGradY = std::sqrt(safLastWvp[1] * safLastWvp[1]
                                                + safLastWvp[5] * safLastWvp[5]
                                                + safLastWvp[9] * safLastWvp[9]);
                    const f32 lfGradZ = std::sqrt(safLastWvp[2] * safLastWvp[2]
                                                + safLastWvp[6] * safLastWvp[6]
                                                + safLastWvp[10] * safLastWvp[10]);
                    lrTally.mfHalfWidthMetres  = (lfGradX > 0.0f) ? (1.0f / lfGradX) : -1.0f;
                    lrTally.mfHalfHeightMetres = (lfGradY > 0.0f) ? (1.0f / lfGradY) : -1.0f;
                    lrTally.mfDepthSpanMetres  = (lfGradZ > 0.0f) ? (1.0f / lfGradZ) : -1.0f;
                }

                f32  lafTriX[3] = { 0.0f, 0.0f, 0.0f };
                f32  lafTriY[3] = { 0.0f, 0.0f, 0.0f };
                u32  luTriVerts = 0;

                for (u32 luK = 0; luK < luProbeIndexCount; ++luK)
                {
                    const u32 luIdx = lau32ProbeIndices[luK];
                    if (luIdx >= luNumVertices)
                        continue;

                    f32 lafPosition[3];
                    std::memcpy(lafPosition, lpProbeVertices + luIdx * luProbeStride, sizeof(lafPosition));

                    f32 lafClip[4];
                    for (u32 luLane = 0; luLane < 4u; ++luLane)
                    {
                        lafClip[luLane] = lafPosition[0] * safLastWvp[0 + luLane]
                                        + lafPosition[1] * safLastWvp[4 + luLane]
                                        + lafPosition[2] * safLastWvp[8 + luLane]
                                        +                  safLastWvp[12 + luLane];
                    }

                    if (lrTally.muSampled == 0u)
                    {
                        std::memcpy(lrTally.mafFirstObject, lafPosition, sizeof(lafPosition));
                        std::memcpy(lrTally.mafFirstClip, lafClip, sizeof(lafClip));
                    }

                    ++lrTally.muSampled;
                    const f32 lfW = lafClip[3];
                    if (!(lfW > 0.0f))                              ++lrTally.muBehindW;
                    else if (lafClip[2] < 0.0f)                     ++lrTally.muOutZNear;
                    else if (lafClip[2] > lfW)                      ++lrTally.muOutZFar;
                    else if (lafClip[0] < -lfW || lafClip[0] > lfW
                          || lafClip[1] < -lfW || lafClip[1] > lfW) ++lrTally.muOutXY;
                    else                                            ++lrTally.muInside;

                    // The clip AABB + the triangle's screen footprint, both after the divide.
                    if (lfW != 0.0f)
                    {
                        const f32 lafNdc[3] = { lafClip[0] / lfW, lafClip[1] / lfW, lafClip[2] / lfW };
                        for (u32 luAxis = 0; luAxis < 3u; ++luAxis)
                        {
                            if (lrTally.muSampled == 1u || lafNdc[luAxis] < lrTally.mafClipMin[luAxis])
                                lrTally.mafClipMin[luAxis] = lafNdc[luAxis];
                            if (lrTally.muSampled == 1u || lafNdc[luAxis] > lrTally.mafClipMax[luAxis])
                                lrTally.mafClipMax[luAxis] = lafNdc[luAxis];
                        }
                        lafTriX[luTriVerts] = lafNdc[0] * 0.5f * static_cast<f32>(lrTally.muVpW);
                        lafTriY[luTriVerts] = lafNdc[1] * 0.5f * static_cast<f32>(lrTally.muVpH);
                        ++luTriVerts;
                    }
                }

                // Sub-pixel census. A triangle whose screen area is under one pixel can pass
                // every clip and depth test and still light no fragment -- which is precisely
                // the "100% inside, px=0" shape.
                if (luTriVerts == 3u)
                {
                    const f32 lfArea = 0.5f * std::fabs(
                          (lafTriX[1] - lafTriX[0]) * (lafTriY[2] - lafTriY[0])
                        - (lafTriX[2] - lafTriX[0]) * (lafTriY[1] - lafTriY[0]));
                    ++lrTally.muTrisSampled;
                    if (lfArea < 1.0f)
                        ++lrTally.muTrisSubPixel;
                    if (lfArea > lrTally.mfMaxTriPixelArea)
                        lrTally.mfMaxTriPixelArea = lfArea;
                }
            }
        }

        // ---- [DIAG, env-gated] force the FALLBACK program pair for caster draws ----------
        // The real per-technique vertex shader does NOT consume the WVP the tally above uses:
        // it computes o0 from `world` x `ViewProjectionModified` (two separate constant
        // blocks). The FALLBACK pair does use it, at c240. So binding the fallback here is a
        // clean A/B: if the fallback rasterises pixels where the real programs rasterise none,
        // the record's baked WVP is right and the constants the real programs were handed are
        // not the cascade's. Diagnostic only; the next technique bind restores the real pair.
        if (sbShadowPassActive)
        {
            static const char* const spcFallbackVs = std::getenv("BRN_SHADOW_FALLBACKVS");
            if (spcFallbackVs != nullptr && spcFallbackVs[0] != '0')
                renderengine::WorldFallbackShader_Bind();
        }

        DWORD luSavedCull = 0, luSavedBias = 0, luSavedSlopeBias = 0;
        DWORD luShadowSavedZFunc = 0, luShadowSavedZEnable = 0, luShadowSavedZWrite = 0;
        bool  lbShadowStateOverridden = false;
        bool  lbShadowZDefeated       = false;
        if (sbShadowPassActive)
        {
            // BRN_SHADOW_ZALWAYS=1 -- defeat the depth test (and force the test/write on) for
            // caster draws. This is the single knob that splits the remaining search in two:
            // if the pixel counts jump, the geometry WAS reaching the band and the depth test
            // (or an uncleared / wrongly cleared depth buffer) was rejecting it; if they stay
            // at zero, nothing is rasterising and the cause is upstream of the depth test.
            static const char* const spcZAlways = std::getenv("BRN_SHADOW_ZALWAYS");
            if (spcZAlways != nullptr && spcZAlways[0] != '0')
            {
                lbShadowZDefeated = true;
                lpDevice->GetRenderState(D3DRS_ZFUNC,        &luShadowSavedZFunc);
                lpDevice->GetRenderState(D3DRS_ZENABLE,      &luShadowSavedZEnable);
                lpDevice->GetRenderState(D3DRS_ZWRITEENABLE, &luShadowSavedZWrite);
                lpDevice->SetRenderState(D3DRS_ZFUNC,        D3DCMP_ALWAYS);
                lpDevice->SetRenderState(D3DRS_ZENABLE,      D3DZB_TRUE);
                lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            }

            static const char* const spcCull  = std::getenv("BRN_SHADOW_CULL");
            static const char* const spcBias  = std::getenv("BRN_SHADOW_BIAS");
            static const char* const spcSlope = std::getenv("BRN_SHADOW_SLOPEBIAS");
            if (spcCull != nullptr || spcBias != nullptr || spcSlope != nullptr)
            {
                lbShadowStateOverridden = true;
                lpDevice->GetRenderState(D3DRS_CULLMODE, &luSavedCull);
                lpDevice->GetRenderState(D3DRS_DEPTHBIAS, &luSavedBias);
                lpDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, &luSavedSlopeBias);

                if (spcCull != nullptr)
                {
                    DWORD luCullMode = D3DCULL_NONE;
                    if (spcCull[0] == 'c' && spcCull[1] == 'w')       luCullMode = D3DCULL_CW;
                    else if (spcCull[0] == 'c' && spcCull[1] == 'c')  luCullMode = D3DCULL_CCW;
                    lpDevice->SetRenderState(D3DRS_CULLMODE, luCullMode);
                }
                if (spcBias != nullptr)
                {
                    const f32 lfBias = static_cast<f32>(std::atof(spcBias));
                    DWORD luBiasBits = 0;
                    std::memcpy(&luBiasBits, &lfBias, sizeof(luBiasBits));
                    lpDevice->SetRenderState(D3DRS_DEPTHBIAS, luBiasBits);
                }
                if (spcSlope != nullptr)
                {
                    const f32 lfSlope = static_cast<f32>(std::atof(spcSlope));
                    DWORD luSlopeBits = 0;
                    std::memcpy(&luSlopeBits, &lfSlope, sizeof(luSlopeBits));
                    lpDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, luSlopeBits);
                }
            }
        }

        // ---- [DIAG, env-gated] force colour writes on for caster draws -------------------
        // BRN_SHADOW_FORCECWE=1. The Z-only walk forces COLORWRITEENABLE to 0 (the leaf in
        // SetMaterialRenderStatesPC), which is correct for a depth-only pass but leaves one
        // exclusion open: whether this driver still counts occlusion samples with every
        // colour channel masked off. Turning the mask back on for one boot closes it.
        DWORD luSavedColourWrite = 0;
        bool  lbColourWriteForced = false;
        if (sbShadowPassActive)
        {
            static const char* const spcForceCwe = std::getenv("BRN_SHADOW_FORCECWE");
            if (spcForceCwe != nullptr && spcForceCwe[0] != '0')
            {
                lbColourWriteForced = true;
                lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &luSavedColourWrite);
                lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                                         D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
                                         | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
            }
        }

        // ---- [PROBE] the EFFECTIVE depth/cull state, read AFTER every override -----------
        // The state block captured at the top of the tally is read BEFORE the env knobs are
        // applied, so it reports what the material walk left -- which is what you want as the
        // baseline, but it is NOT what the draw runs under when a knob is set. Reading it
        // again here removes the ambiguity: `zfuncEff` is the value the GPU actually used.
        if (suShadowClipSlot < KU_SHADOW_TALLY_SLOTS && !saShadowClip[suShadowClipSlot].mbEffectiveCaptured)
        {
            ShadowClipTally& lrTally = saShadowClip[suShadowClipSlot];
            lrTally.mbEffectiveCaptured = true;
            DWORD luState = 0;
            lpDevice->GetRenderState(D3DRS_ZFUNC,    &luState); lrTally.muZFuncEffective = luState;
            lpDevice->GetRenderState(D3DRS_CULLMODE, &luState); lrTally.muCullEffective  = luState;
        }

        // The retained submit is the exact equivalent of the UP call beside it: the UP form
        // offsets the vertex POINTER by baseVertex * stride and passes base 0, the retained
        // form passes BaseVertexIndex = baseVertex against the whole mirrored buffer, so the
        // run's index values address the same vertices either way.
        const HRESULT lhrDraw =
            lbRetained
                ? static_cast<HRESULT>(WorldGeometry_Submit(lRetained, luBaseVertexIndex))
                : lpDevice->DrawIndexedPrimitiveUP(lePrim, 0, luNumVertices, luPrimCount,
                                                   lpIndices,
                                                   lb32Bit ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
                                                   lpVertices, suVertexStride);

        if (lbShadowStateOverridden)
        {
            lpDevice->SetRenderState(D3DRS_CULLMODE, luSavedCull);
            lpDevice->SetRenderState(D3DRS_DEPTHBIAS, luSavedBias);
            lpDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, luSavedSlopeBias);
        }
        if (lbShadowZDefeated)
        {
            lpDevice->SetRenderState(D3DRS_ZFUNC,        luShadowSavedZFunc);
            lpDevice->SetRenderState(D3DRS_ZENABLE,      luShadowSavedZEnable);
            lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, luShadowSavedZWrite);
        }
        if (lbColourWriteForced)
        {
            lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE, luSavedColourWrite);
        }
        // [FLAG PC bring-up probe] every submitted world draw, counted. Read by the shadow
        // pass's [shadow-fetch] line to tell "no draws reached the shadow target" apart from
        // "draws reached it and rasterised nothing" -- the two have identical symptoms in the
        // frame and completely different causes. DELETE with the shadow bring-up probes.
        if (SUCCEEDED(lhrDraw))
            ++guWorldDrawCalls;
        else if (lbRetained)
        {
            // A retained submit the runtime rejected (D3DERR_INVALIDCALL: a run whose index
            // values reach past NumVertices - BaseVertexIndex would be the classic cause; the
            // UP form silently drew such a run out of range). Witnessed once, latched.
            static bool sbRetainedFailLogged = false;
            if (!sbRetainedFailLogged)
            {
                sbRetainedFailLogged = true;
                char lacMsg[160];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[WorldDraw] retained DrawIndexedPrimitive FAILED hr=0x%08X"
                              " (prim %d count %u base %u verts %u)\n",
                              static_cast<unsigned>(lhrDraw), lRetained.miPrimitiveType,
                              lRetained.muPrimitiveCount, luBaseVertexIndex,
                              lRetained.muNumVertices);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }

        if (lbZDefeated)
            lpDevice->SetRenderState(D3DRS_ZFUNC, luSavedZFunc);

        // ====================================================================================
        // [DIAG wheels] WHICH GEOMETRY IS THIS, AND HOW WAS ITS STREAM DECODED?
        //
        // One line per DISTINCT console-instanced mesh signature -- NOT sampled -- so a single
        // boot log enumerates every wheel mesh that reached the device, in both the colour and
        // the z-only/shadow pass (cw distinguishes them).
        //
        // WHY IT EXISTS. "The tyre draws as a box" has exactly two possible causes, and the
        // frame cannot tell them apart: either the leaf decodes the POSITION stream through
        // the wrong D3DDECLTYPE, or the mesh IS a box in the shipped data. This line answers
        // both at once -- `pos=` and `decl=` are the resolved declaration (a mis-mapped
        // position type is visible as a type name that is not FLOAT3, or a source offset that
        // is not 0), and `bbox=`/`corners=`/`boxProxy=` are a census of the SOURCE vertex
        // bytes, which is independent of every decode decision except POSITION's own type.
        //
        // For the record, MEASURED OFFLINE from the shipped bundles (X360 retail
        // WHEELS/WHE_51916650_GR.BNDL and our converted build/game/Wheels/WHE_51916650_GR.bndl
        // -- 24 of 24 meshes byte-identical in positions and indices): the wheel model carries
        // five LOD states, and state 4 -- the coarsest -- ships a 24-vertex axis-aligned CUBE
        // as its tyre mesh (renderable 04F87E17 mesh 1, material 68B809B4 =
        // Vehicle_1Bit_Tyre_Textured_*, six quads at (+-0.4906, +-0.5, +-0.5) with per-face
        // normals). So `boxProxy=1` on a 24-vertex mesh is the DATA saying "coarsest LOD", not
        // a decode fault -- and the fix for a box on screen is then the LOD choice, not this
        // leaf. See the wheels2 report.
        //
        // COST: the bbox/corner scan runs once per distinct signature, at most
        // KU_WHEEL_DECODE_SLOTS times in a whole run and all at boot; every later draw of an
        // already-reported mesh costs one linear scan of that small table.
        // DELETE with the rest of the [wheel-*] diagnostics.
        // ====================================================================================
        if (lbInstancedDiag)
        {
            struct WheelDecodeSignature
            {
                u32 muVbSize;
                u32 muNumVertices;
                u32 muSourceStride;
                u32 muExpandedStride;
                u32 muIndexCount;
                u32 muStartIndex;
                u32 muColourWrite;
            };
            const u32 KU_WHEEL_DECODE_SLOTS = 32u;
            static WheelDecodeSignature saWheelDecodeSeen[KU_WHEEL_DECODE_SLOTS];
            static u32                  suWheelDecodeCount = 0u;

            DWORD luColourWriteState = 0;
            lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &luColourWriteState);

            WheelDecodeSignature lSignature;
            std::memset(&lSignature, 0, sizeof(lSignature));
            lSignature.muVbSize         = spVertexSource->muSize;
            lSignature.muNumVertices    = luNumVertices;
            lSignature.muSourceStride   = suVertexSourceStride;
            lSignature.muExpandedStride = suVertexStride;
            lSignature.muIndexCount     = luIndexCount;
            lSignature.muStartIndex     = luStartIndex;
            lSignature.muColourWrite    = static_cast<u32>(luColourWriteState);

            bool lbAlreadyReported = false;
            for (u32 luSlot = 0; luSlot < suWheelDecodeCount; ++luSlot)
            {
                if (std::memcmp(&saWheelDecodeSeen[luSlot], &lSignature,
                                sizeof(lSignature)) == 0)
                {
                    lbAlreadyReported = true;
                    break;
                }
            }

            if (!lbAlreadyReported && suWheelDecodeCount < KU_WHEEL_DECODE_SLOTS)
            {
                saWheelDecodeSeen[suWheelDecodeCount++] = lSignature;

                // The source-byte census. POSITION is read ONLY when the resolved declaration
                // really says FLOAT3 at a source offset the stride contains -- otherwise the
                // line reports the type and prints no numbers rather than inventing them.
                char lacShape[224];
                lacShape[0] = '\0';
                if (su8LastDeclPositionType == D3DDECLTYPE_FLOAT3
                    && suLastDeclPositionSourceOffset + 12u <= suVertexSourceStride
                    && luNumVertices != 0u)
                {
                    const u8* const lpPositions = static_cast<const u8*>(lpVertexData)
                                                + suLastDeclPositionSourceOffset;
                    f32 lafMin[3];
                    f32 lafMax[3];
                    std::memcpy(lafMin, lpPositions, sizeof(lafMin));
                    std::memcpy(lafMax, lpPositions, sizeof(lafMax));
                    for (u32 luVertex = 1u; luVertex < luNumVertices; ++luVertex)
                    {
                        f32 lafPosition[3];
                        std::memcpy(lafPosition, lpPositions + luVertex * suVertexSourceStride,
                                    sizeof(lafPosition));
                        for (u32 luComponent = 0; luComponent < 3u; ++luComponent)
                        {
                            if (lafPosition[luComponent] < lafMin[luComponent])
                                lafMin[luComponent] = lafPosition[luComponent];
                            if (lafPosition[luComponent] > lafMax[luComponent])
                                lafMax[luComponent] = lafPosition[luComponent];
                        }
                    }
                    // A vertex is on a CORNER of that box when all three components sit at an
                    // extreme. An axis-aligned box proxy scores numVertices/numVertices; a
                    // tyre, a rim, or any curved surface scores 0.
                    u32 luCorners = 0u;
                    for (u32 luVertex = 0u; luVertex < luNumVertices; ++luVertex)
                    {
                        f32 lafPosition[3];
                        std::memcpy(lafPosition, lpPositions + luVertex * suVertexSourceStride,
                                    sizeof(lafPosition));
                        u32 luExtremes = 0u;
                        for (u32 luComponent = 0; luComponent < 3u; ++luComponent)
                        {
                            if (lafPosition[luComponent] == lafMin[luComponent]
                                || lafPosition[luComponent] == lafMax[luComponent])
                                ++luExtremes;
                        }
                        if (luExtremes == 3u)
                            ++luCorners;
                    }
                    std::snprintf(lacShape, sizeof(lacShape),
                                  " bbox=(%.4f,%.4f,%.4f)..(%.4f,%.4f,%.4f) corners=%u/%u"
                                  " boxProxy=%d",
                                  lafMin[0], lafMin[1], lafMin[2],
                                  lafMax[0], lafMax[1], lafMax[2],
                                  (unsigned)luCorners, (unsigned)luNumVertices,
                                  (luCorners == luNumVertices) ? 1 : 0);
                }

                char lacDecodeMsg[640];
                std::snprintf(lacDecodeMsg, sizeof(lacDecodeMsg),
                              "[wheel-decode] verts=%u vbSize=%u srcStride=%u expStride=%u"
                              " dec3n=%u pos=%s@%u->%u decl=[%s] start=%u count=%u prim=%u"
                              " cw=%u%s\n",
                              (unsigned)luNumVertices, (unsigned)spVertexSource->muSize,
                              (unsigned)suVertexSourceStride, (unsigned)suVertexStride,
                              (unsigned)suVertexDec3nCount,
                              DeclTypeName(su8LastDeclPositionType),
                              (unsigned)suLastDeclPositionSourceOffset,
                              (unsigned)suLastDeclPositionExpandedOffset,
                              (spLastDeclElements != nullptr) ? spLastDeclElements : "?",
                              (unsigned)luStartIndex, (unsigned)luIndexCount,
                              (unsigned)luPrimTypeXenon, (unsigned)luColourWriteState,
                              lacShape);
                CgsDev::Log::WriteToLog(lacDecodeMsg);
            }
        }

        // [DIAG wheels] one line per instanced draw, SAMPLED across the run (the first draws
        // of a run are from the boot cameras, not the chase camera). DELETE with the wheels.
        if (lbInstancedDiag)
        {
            static u32 suWheelDiagSeen = 0, suWheelDiagLogged = 0;
            const u32 luWheelSeen = suWheelDiagSeen++;
            if ((luWheelSeen % 32768u) == 0u && suWheelDiagLogged++ < 12u)
            {
                // Transform the first three REFERENCED vertices by the WVP the fallback pair
                // was just given, and report their NDC. This is the ground truth for "does
                // this geometry land on screen": it uses the real vertex bytes, the real
                // declaration offset (POSITION is element 0 at byte 0) and the real matrix.
                char lacNdc[192];
                lacNdc[0] = '\0';
                if (sbHaveLastWvp && luNumVertices > 0)
                {
                    char* lpcAt = lacNdc;
                    int   liLeft = (int)sizeof(lacNdc);
                    for (u32 luK = 0; luK < luProbeIndexCount; ++luK)
                    {
                        const u32 luIdx = lau32ProbeIndices[luK];
                        if (luIdx >= luNumVertices)
                            continue;
                        f32 lafP[3];
                        std::memcpy(lafP, lpProbeVertices + luIdx * luProbeStride, sizeof(lafP));
                        f32 lafC[4];
                        for (int liC = 0; liC < 4; ++liC)
                        {
                            lafC[liC] = lafP[0] * safLastWvp[0 + liC]
                                      + lafP[1] * safLastWvp[4 + liC]
                                      + lafP[2] * safLastWvp[8 + liC]
                                      +           safLastWvp[12 + liC];
                        }
                        const int liN = std::snprintf(lpcAt, (size_t)liLeft,
                            " v%u obj(%.3f,%.3f,%.3f) ndc(%.3f,%.3f,%.3f)/w%.3f",
                            (unsigned)luIdx, lafP[0], lafP[1], lafP[2],
                            lafC[3] != 0.0f ? lafC[0] / lafC[3] : 0.0f,
                            lafC[3] != 0.0f ? lafC[1] / lafC[3] : 0.0f,
                            lafC[3] != 0.0f ? lafC[2] / lafC[3] : 0.0f,
                            lafC[3]);
                        if (liN <= 0 || liN >= liLeft) break;
                        lpcAt += liN; liLeft -= liN;
                    }
                }
                IDirect3DVertexShader9*      lpVs   = nullptr;
                IDirect3DPixelShader9*       lpPs   = nullptr;
                IDirect3DVertexDeclaration9* lpDecl = nullptr;
                DWORD luZEnable = 0, luZFunc = 0, luCull = 0, luColourWrite = 0, luAlphaTest = 0;
                lpDevice->GetVertexShader(&lpVs);
                lpDevice->GetPixelShader(&lpPs);
                lpDevice->GetVertexDeclaration(&lpDecl);
                lpDevice->GetRenderState(D3DRS_ZENABLE, &luZEnable);
                lpDevice->GetRenderState(D3DRS_ZFUNC, &luZFunc);
                lpDevice->GetRenderState(D3DRS_CULLMODE, &luCull);
                lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &luColourWrite);
                lpDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &luAlphaTest);
                char lacMsg[704];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[wheel-draw] @%u prim=%u start=%u count=%u idx[%u..%u] resets=%u"
                              " | VBsize=%u stride=%u numVerts=%u | outPrim=%d primCount=%u"
                              " | vs=%d ps=%d decl=%d z=%u zf=%u cull=%u cw=%u at=%u | hr=0x%08X"
                              " |%s\n",
                              (unsigned)luWheelSeen, (unsigned)luPrimTypeXenon,
                              (unsigned)luStartIndex, (unsigned)luIndexCount,
                              (unsigned)luDiagMin, (unsigned)luDiagMax, (unsigned)luDiagResets,
                              (unsigned)spVertexSource->muSize, (unsigned)suVertexStride,
                              (unsigned)luNumVertices, (int)lePrim, (unsigned)luPrimCount,
                              lpVs != nullptr, lpPs != nullptr, lpDecl != nullptr,
                              (unsigned)luZEnable, (unsigned)luZFunc, (unsigned)luCull,
                              (unsigned)luColourWrite, (unsigned)luAlphaTest,
                              (unsigned)lhrDraw, lacNdc);
                CgsDev::Log::WriteToLog(lacMsg);
                if (lpVs)   lpVs->Release();
                if (lpPs)   lpPs->Release();
                if (lpDecl) lpDecl->Release();
            }
        }
        {
            // [DIAG one-shot] The first IMMEDIATE-MODE draw of the run -- i.e. the sky dome,
            // the only thing on this build that reaches here through
            // shadow::Device::FlushVertexProgramState. Reports everything that decides
            // whether that pass produces pixels, so a black sky can be diagnosed from a log
            // instead of a capture: the draw result, the expanded primitive counts, whether a
            // vertex/pixel program and a declaration are actually bound, and the depth/cull/
            // colour-write state it inherited. DELETE with the sky bring-up.
            static bool sbDiagSky = false;
            const bool lbImmediateMode = sbNextDrawIsImmediateMode;
            sbNextDrawIsImmediateMode = false;
            if (!sbDiagSky && lbImmediateMode)
            {
                sbDiagSky = true;
                IDirect3DVertexShader9*      lpVs   = nullptr;
                IDirect3DPixelShader9*       lpPs   = nullptr;
                IDirect3DVertexDeclaration9* lpDecl = nullptr;
                DWORD luZEnable = 0, luZFunc = 0, luCull = 0, luColourWrite = 0, luDepthBias = 0;
                lpDevice->GetVertexShader(&lpVs);
                lpDevice->GetPixelShader(&lpPs);
                lpDevice->GetVertexDeclaration(&lpDecl);
                lpDevice->GetRenderState(D3DRS_ZENABLE, &luZEnable);
                lpDevice->GetRenderState(D3DRS_ZFUNC, &luZFunc);
                lpDevice->GetRenderState(D3DRS_CULLMODE, &luCull);
                lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &luColourWrite);
                lpDevice->GetRenderState(D3DRS_DEPTHBIAS, &luDepthBias);
                float lfDepthBias = 0.0f;
                std::memcpy(&lfDepthBias, &luDepthBias, 4);
                char lacMsg[288];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[SkyDraw] hr=0x%08X prim=%d prims=%u verts=%u idx=%u reset=%d"
                              " vs=%d ps=%d decl=%d z=%u zfunc=%u cull=%u cw=%u bias=%.6f\n",
                              (unsigned)lhrDraw, (int)lePrim, (unsigned)luPrimCount,
                              (unsigned)luNumVertices, (unsigned)luIndexCount,
                              (int)spResetEnabled, (int)(lpVs != nullptr), (int)(lpPs != nullptr),
                              (int)(lpDecl != nullptr), (unsigned)luZEnable, (unsigned)luZFunc,
                              (unsigned)luCull, (unsigned)luColourWrite, lfDepthBias);
                CgsDev::Log::WriteToLog(lacMsg);
                if (lpVs) lpVs->Release();
                if (lpPs) lpPs->Release();
                if (lpDecl) lpDecl->Release();
            }
        }
    }

    // renderengine::D3DDevice_CreateVertexDeclaration (VertexDescriptor.cpp
    // CreateD3DObject): the input array uses the XENON 12-byte element records
    // {u16 stream, u16 offset, u32 Xenon type, u8 method, u8 usage, u8 usageIdx,
    // pad} terminated by {0xFF, 0, -1, 0,0,0}. Translate to PC records.
    D3DVertexDeclaration* D3DDevice_CreateVertexDeclaration(const void* lpVertexElements)
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr)
            return nullptr;

        const u8* lpIn = static_cast<const u8*>(lpVertexElements);
        D3DVERTEXELEMENT9 laElements[65];
        u32 luCount = 0;
        for (; luCount < 64; ++luCount, lpIn += 12)
        {
            u16 lu16Stream;
            std::memcpy(&lu16Stream, lpIn, 2);
            if (lu16Stream == 0x00FFu)
                break;
            u16 lu16Offset;
            u32 luXenonType;
            std::memcpy(&lu16Offset, lpIn + 2, 2);
            std::memcpy(&luXenonType, lpIn + 4, 4);
            u8 lu8Type;
            if (!MapXenonDeclType(luXenonType, &lu8Type))
                return nullptr;
            laElements[luCount].Stream     = lu16Stream;
            laElements[luCount].Offset     = lu16Offset;
            laElements[luCount].Type       = lu8Type;
            laElements[luCount].Method     = lpIn[6];
            laElements[luCount].Usage      = lpIn[7];
            laElements[luCount].UsageIndex = lpIn[8];
        }
        const D3DVERTEXELEMENT9 lEnd = D3DDECL_END();
        laElements[luCount] = lEnd;

        IDirect3DVertexDeclaration9* lpDecl = nullptr;
        lpDevice->CreateVertexDeclaration(laElements, &lpDecl);
        return reinterpret_cast<D3DVertexDeclaration*>(lpDecl);
    }
}

// =============================================================================
// The Xenon immediate-vertex ring (D3DDevice_BeginVertices / _EndVertices):
// the pending-run state the two ends share.
//
// FLAG PC-platform leaf: the pair is an Xbox 360 XDK D3D immediate-mode
// extension. On the console BeginVertices reserves luVertexCount*luStride bytes
// directly in the GPU command buffer and returns a write cursor into it; the
// caller fills the cursor; EndVertices closes the packet and the run is drawn
// against the CURRENTLY BOUND vertex declaration, shaders and render state.
// PC Direct3D 9 has no writable command buffer, but DrawPrimitiveUP carries
// exactly the same contract at the other end ("here is a block of vertices,
// draw it with what is bound"), so the cursor becomes a host scratch block and
// the submit becomes one DrawPrimitiveUP.
//
// WHY THIS IS NOT PUBLISHED INTO THE WORLD STASH. spVertexSource /
// suVertexStride / the DEC3N plan above also describe "the vertices of the next
// draw", and folding an immediate run into them would look like reuse. It is
// the opposite: the stash describes a SERIALISED CONSOLE vertex-buffer header
// for the indexed world path, and an immediate run overwriting it would leave
// the next DispatchAllMeshes draw reading this quad's stride and expansion
// plan. That is precisely the split-brain that produced the 2026-08-12
// exploding geometry (see the WorldDraw_SetVertexSourceRaw note above). The two
// paths keep separate state on purpose.
// =============================================================================
namespace
{
    // Scratch ring size. Both ImRenderer<V>::Render callers fence the GPU when a
    // batch exceeds KU_FENCE_BYTE_THRESHOLD == 0x80000 bytes
    // (CgsIm2dUntex.cpp:74, BrnSkidVertex.cpp:78), so by the console's own
    // reckoning one run can reach half a megabyte; this is twice that. At the
    // three known strides it holds 87,381 Im2d vertices (12 B),
    // 52,428 post-fx vertices (20 B) or 37,449 skid vertices (28 B) -- against
    // known in-tree batches of FOUR (every 2D quad and the post-fx composite).
    // BSS, allocated once: the console path never heap-allocates per draw and
    // neither does this one.
    const u32 KU_IMVERTS_SCRATCH_BYTES = 1u << 20;

    // 16-byte aligned: the callers write through u32*/f32* casts of the returned
    // cursor, and a vertex block handed to D3D9 should be at least as aligned as
    // the widest element it can carry.
    alignas(16) u8 sauImVertsScratch[KU_IMVERTS_SCRATCH_BYTES];

    bool             sbImVertsPending    = false;  // a BeginVertices is open
    bool             sbImVertsDrawable   = false;  // ...and EndVertices may submit it
    D3DPRIMITIVETYPE seImVertsPrimitive  = D3DPT_TRIANGLESTRIP;
    UINT             suImVertsPrimCount  = 0;
    u32              suImVertsStride     = 0;

    // One flag per CAUSE, not per frame: the composite runs at 60 Hz, so a
    // per-call log line would be 3,600 lines a minute. (LogOnce above keys on a
    // string-literal ADDRESS, which is fine for its fixed messages but cannot
    // carry the formatted numbers -- the requested byte count, the offending
    // type -- that make these particular failures actionable.)
    bool sbImVertsReportedOverflow = false;
    bool sbImVertsReportedNesting  = false;
    bool sbImVertsReportedOrphan   = false;
    bool sbImVertsReportedPartial  = false;
    bool sbImVertsReportedType     = false;
}

// =============================================================================
// The extern "C" Xenon fast-set surface.
// =============================================================================
extern "C"
{

void D3DDevice_SetVertexShader(IDirect3DDevice9* /*lpDeviceArg*/, void* lpShader)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice == nullptr) return;

    if (lpShader == nullptr)
    {
        lpDevice->SetVertexShader(nullptr);
        return;
    }
    // The pointer is the program image at ProgramBufferData+0x14. When the
    // converted PC shader container is live it holds D3D9 bytecode; create-once
    // and bind. Anything else (Xenos microcode) falls back to the world shim.
    if (LooksLikeD3D9Bytecode(lpShader, false))
    {
        IDirect3DVertexShader9*& lrpVs = sVsCache[lpShader];
        if (lrpVs == nullptr)
            lpDevice->CreateVertexShader(static_cast<const DWORD*>(lpShader), &lrpVs);
        if (lrpVs != nullptr)
        {
            lpDevice->SetVertexShader(lrpVs);
            return;
        }
    }
    LogOnce("vsraw", "[XenonD3D9] SetVertexShader: not D3D9 bytecode - fallback world shader\n");
    renderengine::WorldFallbackShader_Bind();
}

void D3DDevice_SetPixelShader(IDirect3DDevice9* /*lpDeviceArg*/, void* lpShader)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice == nullptr) return;

    if (lpShader == nullptr)
    {
        lpDevice->SetPixelShader(nullptr);
        return;
    }
    if (LooksLikeD3D9Bytecode(lpShader, true))
    {
        IDirect3DPixelShader9*& lrpPs = sPsCache[lpShader];
        if (lrpPs == nullptr)
            lpDevice->CreatePixelShader(static_cast<const DWORD*>(lpShader), &lrpPs);
        if (lrpPs != nullptr)
        {
            lpDevice->SetPixelShader(lrpPs);
            return;
        }
    }
    LogOnce("psraw", "[XenonD3D9] SetPixelShader: not D3D9 bytecode - fallback world shader\n");
    renderengine::WorldFallbackShader_Bind();
}

void D3DDevice_SetVertexDeclaration(IDirect3DDevice9* /*lpDeviceArg*/, void* lpDecl)
{
    IDirect3DDevice9* lpDevice = Dev();
    sbWorldDeclarationValid = lpDecl != nullptr;
    if (lpDevice != nullptr && lpDecl != nullptr)
        lpDevice->SetVertexDeclaration(static_cast<IDirect3DVertexDeclaration9*>(lpDecl));
}

void D3DDevice_SetIndices(IDirect3DDevice9* /*lpDeviceArg*/, void* lpIndexData)
{
    // The Xenon fast path binds serialised index-buffer headers; on PC the UP
    // draw path consumes the stash instead.
    renderengine::WorldDraw_SetIndexSource(lpIndexData);
}

void D3DDevice_SetStreamSource(IDirect3DDevice9* /*lpDeviceArg*/, u32 luStreamNumber,
                               const void* lpStreamData, u32 /*luOffsetInBytes*/,
                               u32 luStride, u32 /*luFlags*/)
{
    if (luStreamNumber == 0)
        renderengine::WorldDraw_SetVertexSourceRaw(lpStreamData, luStride);
}

// =============================================================================
// FLAG PC-platform leaf: THE RAW-DEPTH SAMPLER SEAM (2026-08-15, the post-fx step-5 wave).
//
// THE PROBLEM. A TextureState carries its own sampler words, and on this backend they are
// NEVER APPLIED: shadow::Device::SetState(const TextureState*, u32) hands them to
// SetSamplerStateLowLevel, which is the documented no-op a few hundred lines below (the Xenos
// sampler descriptor has no D3D9 translation yet). So a unit keeps whatever sampler state the
// last user left on it -- and D3D9's own default for an untouched unit is POINT/POINT/NONE with
// ADDRESS = WRAP.
//
// THAT MATTERS THE MOMENT A DEPTH TEXTURE IS BOUND. PostFxRenderTargetPCLeaf.cpp now gives every
// sampled post-fx target a RAW-depth format (INTZ / DF24 / DF16) so the composite's DoF and
// motion-blur permutations, and rwgpfxdof, can read depth VALUES. Those fetches must be POINT
// (averaging two depths yields a depth that is at neither surface -- haloes on every silhouette)
// and CLAMP (a wrapped fetch at the screen edge reads the opposite edge's depth). The console
// says the same thing in its own words: RenderTarget::CreateStates @0x82403A18 builds that
// target's depth TextureState with mag = min = 0, i.e. POINT.
//
// WHY IT HOOKS THE **BIND** RATHER THAN A UNIT NUMBER. Keying on "unit 4 is SamplerDepth" would
// be a guess that has to be re-guessed for rwgpfxdof and for PfxHelper; keying on the RESOURCE
// cannot be wrong -- a raw-depth texture is a raw-depth texture at whatever unit it lands on.
// The format is read from the D3D object itself (GetLevelDesc), not from renderengine::Texture::
// miFormat, so no assumption about that field's encoding is made.
//
// IT DOES NOT TOUCH THE SHADOW PATH. The shadow map's D24X8 / D16 are hardware-COMPARE formats
// and are not on the list below, so unit 15 keeps the LINEAR (= 2x2 PCF) filter
// ShadowSampler_ApplyState installs. If a driver ever demotes the shadow map to INTZ, this
// applies POINT there -- which is exactly what ShadowSampler_ApplyState itself does in that case
// (see its `RAW (INTZ/DF24/DF16) POINT` note), so the two agree rather than fight.
//
// KNOWN, BOUNDED SIDE EFFECT: a unit that has held a raw-depth texture keeps POINT/CLAMP after a
// later ordinary texture is bound to it (nothing here restores the previous words, because
// nothing on this backend knows what they were). Today the only units that can receive one are
// the composite's SamplerDepth (unit 4, bound by BrnPostFxShader::Render) and unit 15 on a
// raw-depth shadow fallback; neither is used by anything else.
// DELETE WHEN SetSamplerStateLowLevel really applies a TextureState's sampler block.
//
// extern "C++" BECAUSE THIS SITS INSIDE THE FILE'S extern "C" BLOCK (opened above
// D3DDevice_SetViewportF, closed at the end of the fast-set surface). Without it the anonymous
// namespace is powerless: C linkage flattens every name in it, so `guRawDepthSamplerUnits` and
// the three helpers would be emitted as EXTERNAL, unmangled symbols -- three new globals in the
// link's C namespace and a name collision waiting for any other TU to define `IsRawDepthFormat`.
// Caught by dumpbin, not by the compiler.
// =============================================================================
extern "C++" {
namespace
{
    const u32 KU_RAW_DEPTH_MAX_SAMPLER_UNITS = 16u;

    // Which sampler units are currently holding a raw-depth texture. Read by
    // renderengine::PostFxDepthSampler_BoundUnitMask so a target that is about to bind its depth
    // texture as the DEPTH-STENCIL SURFACE can unbind it from the samplers first (D3D9 does not
    // do that for you -- see renderengine::Device::SetState in PostFxRenderTargetPCLeaf.cpp).
    u32 guRawDepthSamplerUnits = 0u;

    bool IsRawDepthFormat(D3DFORMAT leFormat)
    {
        return leFormat == static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'))
            || leFormat == static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '2', '4'))
            || leFormat == static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '1', '6'))
            || leFormat == static_cast<D3DFORMAT>(MAKEFOURCC('R', 'A', 'W', 'Z'));
    }

    // Ask the runtime what was actually bound. Only 2D textures can be depth textures here, and
    // the call happens ONLY on a real bind change (shadow::Device dedups every SetTexture), so
    // this is a handful of CPU reads a frame, not a per-draw cost.
    bool BoundTextureIsRawDepth(IDirect3DBaseTexture9* lpTexture, D3DFORMAT* lpeFormatOut)
    {
        if (lpTexture == nullptr || lpTexture->GetType() != D3DRTYPE_TEXTURE)
            return false;

        D3DSURFACE_DESC lDesc;
        std::memset(&lDesc, 0, sizeof(lDesc));
        if (FAILED(static_cast<IDirect3DTexture9*>(lpTexture)->GetLevelDesc(0u, &lDesc)))
            return false;

        if (lpeFormatOut != nullptr)
            *lpeFormatOut = lDesc.Format;
        return IsRawDepthFormat(lDesc.Format);
    }

    // =========================================================================================
    // [FLAG PC-platform leaf] THE COLOUR-CUBE (VOLUME) SAMPLER SEAM -- post-fx step 10.
    //
    // Same disease as the raw-depth seam above, different patient, and it is fixed the same way
    // for the same reason: a TextureState's sampler words are never applied on this backend
    // (SetSamplerStateLowLevel is the documented no-op below), so the unit keeps whatever the
    // last user left on it, and D3D9's own default for an untouched unit is POINT/POINT/NONE
    // with ADDRESS = WRAP.
    //
    // THAT IS TWO VISIBLE BUGS ON A 32^3 COLOUR-GRADING LUT, not one:
    //   * POINT on a 32-step lookup posterises the whole frame -- the grade snaps between 32
    //     values per channel instead of interpolating, which reads as heavy banding in skies
    //     and headlight falloff;
    //   * WRAP is worse than wrong: the composite feeds the COMPOSED COLOUR in as the
    //     coordinate (`tex3D(Sampler3dTint, lComposite)`), so a channel that reaches 1.0 wraps
    //     to 0.0 and the brightest pixels in the frame come back BLACK.
    //
    // The console says LINEAR/CLAMP in its own words: rw::graphics::postfx::Tint::Initialize
    // @0x82403B48 builds the lookup TextureState with addressU = addressV = addressW = 2 and
    // magFilter = minFilter = 1, mipFilter = 0 (asm 0x82403CC8-0x82403CEC). This applies exactly
    // that set.
    //
    // WHY IT HOOKS THE BIND RATHER THAN A UNIT NUMBER: identical argument to the raw-depth seam.
    // "Unit 3 is Sampler3dTint" is a guess that has to be re-guessed for every future consumer;
    // "a volume texture is a volume texture" cannot be wrong, and the type is read from the D3D
    // object itself (GetType()), not from renderengine::Texture::miFormat.
    //
    // ONLY THE TINT LUT CAN REACH THIS PATH, and that is now measured rather than asserted. The
    // test is on the D3D object's own GetType() == D3DRTYPE_VOLUMETEXTURE, so it fires exactly for
    // the textures renderengine::Texture::Create made with CreateVolumeTexture -- and Create takes
    // that branch only for Parameters::meType == E_TYPE_VOLUME, which nothing but
    // rw::graphics::postfx::Tint::Initialize asks for (two buffers of one 32^3 cube). CENSUS of the
    // shipped data behind that claim: of 26,614 RwRaster resources under build/game, 0 carry the
    // create-time volume flag -- while 234 (every track unit's DXT1 64x64x6 CUBE MAP, plus
    // GLOBALBACKDROPS / GLOBALPROPS) have a serialised depth byte above 1 and WOULD have landed
    // here under the depth-derived test the first cut of this wave used
    // (scratch/postfx_step10_finish/tintfix/work/raster_dimension_census.py). So there is no world
    // or prop unit whose previous filter this can disturb, beyond the same bounded side effect the
    // raw-depth seam already documents.
    // DELETE WHEN SetSamplerStateLowLevel really applies a TextureState's sampler block.
    // =========================================================================================
    void ApplyVolumeSamplerState(IDirect3DDevice9* lpDevice, u32 luUnit)
    {
        if (lpDevice == nullptr || luUnit >= KU_RAW_DEPTH_MAX_SAMPLER_UNITS)
            return;

        lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER,     D3DTEXF_LINEAR);  // X360 minFilter = 1
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER,     D3DTEXF_LINEAR);  // X360 magFilter = 1
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER,     D3DTEXF_NONE);    // X360 mipFilter = 0
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU,      D3DTADDRESS_CLAMP);  // X360 addressU = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV,      D3DTADDRESS_CLAMP);  // X360 addressV = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSW,      D3DTADDRESS_CLAMP);  // X360 addressW = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXMIPLEVEL,   0u);
        // ⚠ 1, NOT the console's 13, AND THAT IS DELIBERATE. Tint::Initialize @0x82403B48 does put
        // maxAnisotropy = 0xD in the lookup TextureState (`li r10, 0xD` @0x82403C54 -> `stw r10,
        // 0x1A0+var_120(r1)` @0x82403C9C, which rwgpfxtint.cpp carries as muMaxAnisotropy = 13), but
        // D3D9 consults MAXANISOTROPY only while MINFILTER or MAGFILTER is D3DTEXF_ANISOTROPIC --
        // and both are LINEAR two lines above, exactly as the console sets them. Writing 13 here
        // would change nothing and would read as an anisotropic filter that is not being asked for.
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXANISOTROPY, 1u);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_SRGBTEXTURE,   FALSE);
    }

    // =========================================================================================
    // [FLAG PC-platform leaf] THE CUBE SAMPLER SEAM -- reflections step 1. The third case of the
    // shape the two seams above already established, and it exists for the same reason: a
    // TextureState's sampler words never reach D3D9 on this backend (SetSamplerStateLowLevel is
    // the documented no-op below), so a unit keeps whatever the last user left on it, and D3D9's
    // own default for an untouched unit is POINT/POINT/NONE with ADDRESS = WRAP.
    //
    // WHAT THE CONSOLE ASKS FOR, and it is the tree's own decode, not a new one:
    // BrnRendererModule::Construct @0x8240A778 builds the env map's TextureState with
    // addressU = addressV = addressW = 2 and magFilter = minFilter = mipFilter = 1 (lines 556-561
    // of its pseudocode; the same 2 == CLAMP / 1 == LINEAR vocabulary the volume seam above was
    // built from). So: trilinear, clamped -- which is what a 128x128 reflection cube needs and
    // what the 22 shipped vehicle pixel shaders sample at s13.
    //
    // WHY IT HOOKS THE BIND RATHER THAN A UNIT NUMBER: identical argument to the two seams above.
    // "Unit 13 is the reflection sampler" is a guess that has to be re-guessed for every future
    // consumer (and would be wrong for the 7 WORLD techniques, which declare the same sampler on
    // s0 / s2); "a cube texture is a cube texture" cannot be wrong, and the type is read from the
    // D3D object itself.
    // DELETE WHEN SetSamplerStateLowLevel really applies a TextureState's sampler block.
    // =========================================================================================
    void ApplyCubeSamplerState(IDirect3DDevice9* lpDevice, u32 luUnit)
    {
        if (lpDevice == nullptr || luUnit >= KU_RAW_DEPTH_MAX_SAMPLER_UNITS)
            return;

        lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER,     D3DTEXF_LINEAR);  // X360 minFilter = 1
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER,     D3DTEXF_LINEAR);  // X360 magFilter = 1
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER,     D3DTEXF_LINEAR);  // X360 mipFilter = 1
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU,      D3DTADDRESS_CLAMP);  // X360 addressU = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV,      D3DTADDRESS_CLAMP);  // X360 addressV = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSW,      D3DTADDRESS_CLAMP);  // X360 addressW = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXMIPLEVEL,   0u);
        // 1, not the console's 13 -- same reasoning as ApplyVolumeSamplerState above: D3D9 consults
        // MAXANISOTROPY only while a filter is D3DTEXF_ANISOTROPIC, and neither is.
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXANISOTROPY, 1u);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_SRGBTEXTURE,   FALSE);
    }

    void ApplyRawDepthSamplerState(IDirect3DDevice9* lpDevice, u32 luUnit)
    {
        if (lpDevice == nullptr || luUnit >= KU_RAW_DEPTH_MAX_SAMPLER_UNITS)
            return;

        lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER,     D3DTEXF_POINT);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER,     D3DTEXF_POINT);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER,     D3DTEXF_NONE);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU,      D3DTADDRESS_CLAMP);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV,      D3DTADDRESS_CLAMP);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSW,      D3DTADDRESS_CLAMP);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXMIPLEVEL,   0u);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXANISOTROPY, 1u);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_SRGBTEXTURE,   FALSE);
    }

    // =========================================================================================
    // [FLAG PC-platform leaf] THE POST-FX COMPOSITE'S **SOURCE** SAMPLER SEAM -- the fourth case
    // of the shape the three seams above establish, and the one with the largest visible payload.
    //
    // WHAT THE CONSOLE ASKS FOR. BrnPostFxShader::Construct @0x823FDD18 builds FOUR samplers for
    // this one unit and Render picks between three of them per frame
    // (BrnPostFxShader.cpp:1786-1799):
    //     motion blur on   -> mapSamplerState_MotionBlur[meQuality]: ANISOTROPIC min+mag
    //                         (muBias = muMaxLevel = KU_SAMPLER_FILTER_ANISO, BrnPostFxShader.cpp
    //                         :1341-1342) with muConvolution -- the block's MAX ANISOTROPY field,
    //                         see that file's field-naming banner at :404-412 -- set to the loop's
    //                         `luMaxAnisotropy = 4` then `+= 12` (the X360 `addi r27, r27, 0xC`),
    //                         i.e. 4x for E_QUALITY_CHEAP and 16x for E_QUALITY_EXPENSIVE;
    //     bilinear source  -> mpSamplerState_Linear: LINEAR min+mag (:1313-1314);
    //     otherwise        -> mpSamplerState_Point:  POINT  min+mag (:1293-1294), because a 1:1
    //                         blit must not be filtered ("a linear fetch there would soften every
    //                         pixel", :1283-1284).
    // All four are CLAMP on U/V/W (:1290-1292; Linear and MotionBlur are copies of the Point
    // block) and all four carry the same mip word, so the ARMS DIFFER ONLY IN MIN/MAG AND IN THE
    // ANISOTROPY COUNT -- which is exactly this function's two parameters.
    //
    // WHY THE COUNT IS THE EFFECT. The console's camera motion blur is not a tap loop: it sets the
    // fetch unit's two gradient latches by hand (setGradientH = the screen velocity, setGradientV
    // = its perpendicular * 1/256) and issues ONE tfetch, so the sampler's ANISOTROPIC filter
    // integrates the streak in hardware. The PC programs spell that as one tex2Dgrad
    // (tools/assets/shaders/brn_postfx_composite.fx). BLUR and BLURHQ differ by a single constant
    // -- the sub-tap jitter 0.25 vs 0.0625 -- and both are 1/MaxAnisotropy for the sampler this
    // function installs. Left at MAXANISOTROPY = 1 with a LINEAR filter, texldd's gradients only
    // select a mip level, MIPFILTER is NONE, and the whole effect degrades to a bilinear tap at
    // the jittered uv: visibly SHARP.
    //
    // WHY IT IS APPLIED HERE AND NOT THROUGH THE SAMPLER OBJECT. Same reason as the three seams
    // above: SetSamplerStateLowLevel is the documented no-op at the bottom of this file, so a
    // SamplerState's words never reach D3D9 -- including the POINT and LINEAR arms. Unlike those
    // three this one CANNOT be keyed on the bound resource (the source is an ordinary 2D colour
    // target on every arm), so it is keyed on the CALLER's own bind instead.
    //
    // IT HAS AN OFF ARM, AND THAT IS THE POINT. BrnPostFxShader::Render calls this on EVERY
    // composite draw with the words of the sampler it just bound, so the unit always carries that
    // sampler and a non-blur frame puts POINT/LINEAR and MAXANISOTROPY = 1 back. Applying a
    // single arm would leave unit 0 ANISOTROPIC for every later consumer (the Im2d GUI draws at
    // CgsIm2d.cpp:118/149 re-assert MIN/MAG but never ADDRESS), which is a state leak, not a fix.
    // The apply discipline is the file's own: re-issue the words at every bind, keep no cache --
    // shadow::Device::SetState already owns the only cache there is (shadowingdevice.cpp:374-386).
    //
    // CAPS, not assumption: D3D9 ignores MAXANISOTROPY unless a filter is D3DTEXF_ANISOTROPIC and
    // clamps the count to D3DCAPS9::MaxAnisotropy. Both are read from the device and reported
    // once. On a device without anisotropic min+mag the aniso arm falls back to LINEAR with
    // anisotropy 1 -- which is not an invention: it is exactly what Construct writes into the
    // motion-blur block BEFORE the anisotropic override (BrnPostFxShader.cpp:1335-1337).
    // DELETE WHEN SetSamplerStateLowLevel really applies a TextureState's sampler block.
    // =========================================================================================

    // The console's own min/mag filter word, as stored in SamplerStateParameters::muBias /
    // muMaxLevel by BrnPostFxShader.cpp:380-382 (`li r26, 1` / `li r22, 4`). Passed in rather
    // than re-derived so the caller can hand over the word of the sampler object it just bound.
    const u32 KU_POSTFX_SOURCE_FILTER_POINT  = 0u;
    const u32 KU_POSTFX_SOURCE_FILTER_LINEAR = 1u;
    const u32 KU_POSTFX_SOURCE_FILTER_ANISO  = 4u;

    // [DIAG, mask-alpha] Raised by ApplyPostFxSourceSamplerState and by nothing else, so it is
    // true on exactly the composite's own full-screen quad and false on the bloom down-sample and
    // the two Blur9 passes -- the discriminator the [mask-alpha] source read-back needs (see the
    // probe in the immediate-mode draw path). Consumed there, on every immediate draw.
    bool gbPostFxSourceSamplerApplied = false;

    void ApplyPostFxSourceSamplerState(IDirect3DDevice9* lpDevice, u32 luUnit,
                                       u32 luMinMagFilterWord, u32 luMaxAnisotropy)
    {
        gbPostFxSourceSamplerApplied = true;

        if (lpDevice == nullptr || luUnit >= KU_RAW_DEPTH_MAX_SAMPLER_UNITS)
            return;

        static bool sbCapsRead       = false;
        static bool sbAnisoSupported = false;
        static u32  suDeviceMaxAniso = 1u;
        if (!sbCapsRead)
        {
            sbCapsRead = true;
            D3DCAPS9 lCaps;
            std::memset(&lCaps, 0, sizeof(lCaps));
            if (SUCCEEDED(lpDevice->GetDeviceCaps(&lCaps)))
            {
                sbAnisoSupported =
                    ((lCaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0u) &&
                    ((lCaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0u);
                suDeviceMaxAniso = (lCaps.MaxAnisotropy > 1u)
                                       ? static_cast<u32>(lCaps.MaxAnisotropy) : 1u;
            }
            char lacMsg[280];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[postfx-mb] composite SOURCE sampler seam live: device anisotropic"
                          " min+mag = %d, MaxAnisotropy = %u. The console's motion-blur samplers"
                          " ask for 4x (E_QUALITY_CHEAP) and 16x (E_QUALITY_EXPENSIVE); with"
                          " anisotropy 1 the blur's single tex2Dgrad is a bilinear tap.\n",
                          sbAnisoSupported ? 1 : 0, static_cast<unsigned>(suDeviceMaxAniso));
            CgsDev::Log::WriteToLog(lacMsg);
        }

        DWORD leFilter       = D3DTEXF_POINT;
        DWORD luAppliedAniso = 1u;
        if (luMinMagFilterWord == KU_POSTFX_SOURCE_FILTER_ANISO)
        {
            if (sbAnisoSupported)
            {
                u32 luAniso = (luMaxAnisotropy < 1u) ? 1u : luMaxAnisotropy;
                if (luAniso > suDeviceMaxAniso)
                    luAniso = suDeviceMaxAniso;
                leFilter       = D3DTEXF_ANISOTROPIC;
                luAppliedAniso = static_cast<DWORD>(luAniso);
            }
            else
            {
                // Construct's own pre-override words for this block (BrnPostFxShader.cpp
                // :1335-1337): LINEAR min+mag, muConvolution = 1.
                leFilter = D3DTEXF_LINEAR;
            }
        }
        else if (luMinMagFilterWord == KU_POSTFX_SOURCE_FILTER_LINEAR)
        {
            leFilter = D3DTEXF_LINEAR;
        }

        lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER,     leFilter);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER,     leFilter);
        // NONE, and it is not a choice: the composite's source is a single-level render target,
        // so there is no mip chain to filter. The console's mip word (KU_SAMPLER_MIP_FILTER = 2)
        // is identical across all four of Construct's samplers (BrnPostFxShader.cpp:383), i.e. it
        // is not what tells the arms apart, and the three seams above install NONE for the same
        // single-level reason.
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER,     D3DTEXF_NONE);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU,      D3DTADDRESS_CLAMP);  // X360 addressU = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV,      D3DTADDRESS_CLAMP);  // X360 addressV = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSW,      D3DTADDRESS_CLAMP);  // X360 addressW = 2
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXMIPLEVEL,   0u);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXANISOTROPY, luAppliedAniso);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_SRGBTEXTURE,   FALSE);
    }
}
}  // extern "C++"

unsigned int D3DDevice_SetTexture(IDirect3DDevice9* /*lpDeviceArg*/, u32 luSampler,
                                  void* lpTexture, unsigned int /*luFlags*/)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice == nullptr)
        return 0;

    // The Xenon fast-set takes a GPU texture HEADER, which on the console is the leading
    // part of the renderengine::Texture object itself -- so every caller passes a
    // renderengine::Texture* (shadow::Device::SetResource, this build's only caller, is
    // handed one straight from BrnIm3d.cpp's cloud-texture bind). D3D9 needs the wrapped
    // COM object out of it; binding the wrapper as if it were an IDirect3DBaseTexture9
    // would hand the runtime a bogus vtable. Same field the world path reads
    // (WorldShader_BindTextureUnit).
    IDirect3DBaseTexture9* lpD3DTexture = nullptr;
    if (lpTexture != nullptr)
        lpD3DTexture = static_cast<const renderengine::Texture*>(lpTexture)->mpD3DTexture;
    const HRESULT lhr = lpDevice->SetTexture(luSampler, lpD3DTexture);

    // A VOLUME texture is the post-fx colour-cube LUT and needs LINEAR/CLAMP on its unit, for the
    // same reason and by the same mechanism as the raw-depth case below (see both banners).
    // Keyed on what was bound, not on which unit.
    if (luSampler < KU_RAW_DEPTH_MAX_SAMPLER_UNITS &&
        SUCCEEDED(lhr) && lpD3DTexture != nullptr &&
        lpD3DTexture->GetType() == D3DRTYPE_VOLUMETEXTURE)
    {
        ApplyVolumeSamplerState(lpDevice, luSampler);

        static bool sabVolumeReported[KU_RAW_DEPTH_MAX_SAMPLER_UNITS] = {};
        if (!sabVolumeReported[luSampler])
        {
            sabVolumeReported[luSampler] = true;
            char lacMsg[160];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[postfx-tint] unit %u bound a VOLUME texture"
                          " -- sampler forced LINEAR/CLAMP (the colour-cube LUT)\n",
                          (unsigned)luSampler);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // A CUBE texture is the environment map (unit 13) or a shipped world reflection raster, and
    // needs LINEAR/CLAMP on its unit -- the third case of the same shape (see ApplyCubeSamplerState).
    // This is envface's CROSS-GROUP REQUEST to cubeleaf, answered here.
    if (luSampler < KU_RAW_DEPTH_MAX_SAMPLER_UNITS &&
        SUCCEEDED(lhr) && lpD3DTexture != nullptr &&
        lpD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE)
    {
        ApplyCubeSamplerState(lpDevice, luSampler);

        static bool sabCubeReported[KU_RAW_DEPTH_MAX_SAMPLER_UNITS] = {};
        if (!sabCubeReported[luSampler])
        {
            sabCubeReported[luSampler] = true;
            char lacMsg[176];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[envmap] unit %u bound a CUBE texture"
                          " -- sampler forced LINEAR/CLAMP (the reflection cube)\n",
                          (unsigned)luSampler);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // A RAW-depth texture needs POINT/CLAMP on its unit, and this backend has nowhere else to
    // say so (see the banner above this function). Keyed on what was bound, not on which unit.
    if (luSampler < KU_RAW_DEPTH_MAX_SAMPLER_UNITS)
    {
        D3DFORMAT leBoundFormat = D3DFMT_UNKNOWN;
        if (SUCCEEDED(lhr) && BoundTextureIsRawDepth(lpD3DTexture, &leBoundFormat))
        {
            ApplyRawDepthSamplerState(lpDevice, luSampler);
            guRawDepthSamplerUnits |= (1u << luSampler);

            static bool sabRawDepthReported[KU_RAW_DEPTH_MAX_SAMPLER_UNITS] = {};
            if (!sabRawDepthReported[luSampler])
            {
                sabRawDepthReported[luSampler] = true;
                const u32 luFourCC = static_cast<u32>(leBoundFormat);
                char lacMsg[192];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[postfx-depth] unit %u bound a RAW depth texture (%c%c%c%c)"
                              " -- sampler forced POINT/CLAMP\n",
                              (unsigned)luSampler,
                              (char)(luFourCC & 0xFFu), (char)((luFourCC >> 8) & 0xFFu),
                              (char)((luFourCC >> 16) & 0xFFu), (char)((luFourCC >> 24) & 0xFFu));
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        else
        {
            // Anything else on this unit (including an unbind) retires its raw-depth claim.
            guRawDepthSamplerUnits &= ~(1u << luSampler);
        }
    }
    {
        // [DIAG one-shot per unit] the first bind on each of the two cloud sampler units.
        static bool sabDiag[2] = { false, false };
        if (luSampler < 2u && !sabDiag[luSampler])
        {
            sabDiag[luSampler] = true;
            char lacMsg[160];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[SkyTex] unit %u raster=%p d3d=%p hr=0x%08X\n",
                          (unsigned)luSampler, lpTexture, (void*)lpD3DTexture, (unsigned)lhr);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }
    return 0;
}

void D3DDevice_DrawIndexedVertices(IDirect3DDevice9* /*lpDeviceArg*/,
                                   u32 lePrimitiveType,
                                   u32 luBaseVertexIndex,
                                   u32 luMinVertexIndex,
                                   u32 luNumVertices)
{
    // Xenon signature: (PrimitiveType, BaseVertexIndex, StartIndex, IndexCount).
    renderengine::WorldDraw_IndexedUP(lePrimitiveType, luBaseVertexIndex,
                                      luMinVertexIndex, luNumVertices);
}

// ---------------------------------------------------------------------------------------------
// D3DDevice_BeginVertices -- reserve a run of luVertexCount vertices of luVertexStreamZeroStride
// bytes each and return the write cursor the caller fills.
//
// Declared (with no home until now) by CgsIm2dUntex.cpp:102, BrnSkidVertex.cpp:103 and the
// post-fx composite; CgsIm3d.cpp:23-24 names the same pair as the reason its X360 Render body
// was left unbodied.
//
// The device argument is IGNORED, exactly as in every sibling shim in this block: the callers
// pass ImRendererBase::mgpDevice / renderengine::gpD3DDevice (the X360 off_83271608 global) and
// the live PC device is whatever Dev() resolves at submit time. It is spelled void* here for the
// same reason D3DDevice_SetViewportF is -- the callers' `D3DDevice` is an incomplete console-only
// tag type this TU has no reason to name, and under extern "C" the symbol carries no parameter
// types at all (the D3DDevice_SetStreamSource precedent: XCamVideoOutput.h declares it with
// D3DDevice*, this TU defines it with IDirect3DDevice9*, and it links).
//
// THE PRIMITIVE TYPE ARRIVES AS A XENOS VALUE AND MUST BE TRANSLATED. Xenos numbers TRIANGLEFAN 5
// and TRIANGLESTRIP 6; PC Direct3D 9 numbers them the other way round (D3DPT_TRIANGLESTRIP == 5,
// D3DPT_TRIANGLEFAN == 6). Every immediate quad in this tree is a STRIP passed as 6 --
// CgsFontRenderer.cpp:190-191 names 6 KU_PRIMITIVE_TRIANGLE_STRIP, the six
// `Render(PrimitiveType(6), laVerts, 4)` 2D quads in BrnRendererModule / CgsGuiViewModule /
// BrnLoadingScreenRenderer pass it, and so does the post-fx composite -- so forwarding 6 raw would
// read all of them as FANS. A four-vertex strip and a four-vertex fan cover the same quad, but the
// fan's second triangle has the OPPOSITE winding to its first, so with backface culling on a
// diagonal half of every quad (and of the whole post-fx composite) silently is not drawn.
// MapPrimitive() at the top of this TU is its single translation table and the indexed world path
// already uses it; it is reused here rather than duplicated, so the two draw paths can never
// disagree about what 6 means.
//
// Types MapPrimitive refuses -- Xenos RECTLIST (8) and QUADLIST (13) -- have no DrawPrimitiveUP
// form and no caller in this tree today (nothing passes 8 or 13 anywhere in b5-decomp/src), so
// they are refused honestly rather than expanded on speculation. If a real RECTLIST caller ever
// lands, expand it there (a rect is two triangles) rather than guessing now.
//
// THE RETURN VALUE. Every in-tree caller null-checks the cursor before writing through it
// (CgsIm2dUntex.cpp:436, BrnSkidVertex.cpp:379, the post-fx composite), so nullptr is a usable
// refusal -- but it is returned for exactly ONE cause: a request larger than the scratch block,
// where there is no memory to hand out and returning a short buffer would be a silent overrun of
// KU_IMVERTS_SCRATCH_BYTES. Every other refusal (unsupported topology, empty run) still returns
// the real scratch base, so even a future caller that dereferences unconditionally writes into
// owned memory; only the submit is skipped.
// ---------------------------------------------------------------------------------------------
void* D3DDevice_BeginVertices(void* /*lpDeviceArg*/, u32 luPrimitiveType,
                              u32 luVertexCount, u32 luVertexStreamZeroStride)
{
    if (sbImVertsPending && !sbImVertsReportedNesting)
    {
        sbImVertsReportedNesting = true;
        CgsDev::Log::WriteToLog("[XenonD3D9] BeginVertices called with a run already open - "
                                "the earlier run is dropped\n");
    }
    // Recover by abandoning the open run rather than asserting. The console ring is not re-entrant
    // either, and faulting the sim over a mis-nested immediate draw would cost more than the draw.
    sbImVertsPending   = true;
    sbImVertsDrawable  = false;
    suImVertsPrimCount = 0;
    suImVertsStride    = luVertexStreamZeroStride;

    // 64-bit product: luVertexCount * luVertexStreamZeroStride is two u32s and would WRAP in 32
    // bits, which would turn a huge request into a small one and pass the bounds check.
    const u64 lu64Bytes = static_cast<u64>(luVertexCount)
                        * static_cast<u64>(luVertexStreamZeroStride);
    if (lu64Bytes > static_cast<u64>(KU_IMVERTS_SCRATCH_BYTES))
    {
        if (!sbImVertsReportedOverflow)
        {
            sbImVertsReportedOverflow = true;
            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[XenonD3D9] BeginVertices: %u verts x %u bytes = %llu exceeds the %u-byte "
                          "scratch ring - run REFUSED (raise KU_IMVERTS_SCRATCH_BYTES)\n",
                          static_cast<unsigned>(luVertexCount),
                          static_cast<unsigned>(luVertexStreamZeroStride),
                          static_cast<unsigned long long>(lu64Bytes),
                          static_cast<unsigned>(KU_IMVERTS_SCRATCH_BYTES));
            CgsDev::Log::WriteToLog(lacMsg);
        }
        return nullptr;
    }

    if (luVertexCount == 0u || luVertexStreamZeroStride == 0u)
    {
        // Nothing to draw. Still a valid cursor, so a caller that writes anyway stays in bounds.
        return sauImVertsScratch;
    }

    // Topology and primitive count are resolved HERE, not at submit time, so a type this backend
    // cannot draw is refused before the caller has even filled the run -- the device never sees it.
    D3DPRIMITIVETYPE lePrimitive = D3DPT_TRIANGLESTRIP;
    UINT             luPrimCount = 0;
    if (!MapPrimitive(luPrimitiveType, luVertexCount, &lePrimitive, &luPrimCount))
    {
        // MapPrimitive's own LogOnce covers the unsupported-TYPE case; this adds the numbers it
        // cannot format, and also covers its other refusal (a supported type with too few
        // vertices to make one primitive, e.g. a 2-vertex triangle strip).
        if (!sbImVertsReportedType)
        {
            sbImVertsReportedType = true;
            char lacMsg[208];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[XenonD3D9] BeginVertices: Xenos primitive type %u with %u vertices is not "
                          "drawable as a DrawPrimitiveUP - submit SKIPPED\n",
                          static_cast<unsigned>(luPrimitiveType),
                          static_cast<unsigned>(luVertexCount));
            CgsDev::Log::WriteToLog(lacMsg);
        }
        return sauImVertsScratch;
    }

    // MapPrimitive's counts already FLOOR (integer / 2 and / 3). A run whose vertex count does not
    // divide evenly -- an odd LINELIST, a TRIANGLELIST that is not a multiple of three -- draws its
    // complete primitives and drops the trailing partial one. That matches the hardware (a partial
    // primitive is not a primitive) and is strictly better than dropping the whole batch over a
    // caller's off-by-one, but it is a caller bug either way, so it is reported once.
    bool lbPartial = false;
    if (lePrimitive == D3DPT_LINELIST)          lbPartial = (luVertexCount % 2u) != 0u;
    else if (lePrimitive == D3DPT_TRIANGLELIST) lbPartial = (luVertexCount % 3u) != 0u;
    if (lbPartial && !sbImVertsReportedPartial)
    {
        sbImVertsReportedPartial = true;
        char lacMsg[208];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[XenonD3D9] BeginVertices: %u vertices do not divide into D3D primitive type "
                      "%u - drawing %u complete primitives, trailing partial dropped\n",
                      static_cast<unsigned>(luVertexCount),
                      static_cast<unsigned>(lePrimitive),
                      static_cast<unsigned>(luPrimCount));
        CgsDev::Log::WriteToLog(lacMsg);
    }

    seImVertsPrimitive = lePrimitive;
    suImVertsPrimCount = luPrimCount;
    sbImVertsDrawable  = true;
    return sauImVertsScratch;
}

// ---------------------------------------------------------------------------------------------
// D3DDevice_EndVertices -- close the open run and submit it.
//
// STREAM 0, THE DOCUMENTED SIDE EFFECT. DrawPrimitiveUP resets the stream-0 source binding (and
// its stride) to the user-pointer data it just consumed. When this was written no TU in
// b5-decomp/src called IDirect3DDevice9::SetStreamSource; SINCE THEN the retained world
// vertex/index path does (WorldGeometryPCLeaf.cpp:810-817 SetStreamSource / SetIndices /
// DrawIndexedPrimitive straight on the device) -- and it RE-BINDS stream 0 and the indices on
// every submit, so a reset here still costs nothing (nothing caches a stream-0 binding across
// draws; shadowingdevice.cpp's FlushVertexProgramState re-issues every stream source per flush).
// The rest of the original reasoning still holds: the Xenon
// D3DDevice_SetStreamSource shim thirty lines above publishes into the WORLD STASH
// (WorldDraw_SetVertexSourceRaw) and never touches a device stream, every other named
// SetStreamSource in the tree (shadowingdevice.cpp, MeshHelper.cpp, BrnSkyDomeManager.cpp, the
// Lion particle renderer, XCam) is a call TO that shim, and every draw on this backend is a
// *PrimitiveUP. There is therefore no device-side stream-0 binding to lose and no cache to
// invalidate -- and the world path's own DrawIndexedPrimitiveUP (WorldDraw_IndexedUP) has always
// depended on the same fact. Deliberately NOT re-binding or clearing stream 0 here: that would
// introduce a convention this backend's other draw path does not follow.
//
// The run is drawn against whatever vertex declaration, shaders and render state are currently
// bound, which is the console contract exactly -- the caller's FlushVertexProgramState /
// BeginRendering / SetProgram all ran before BeginVertices.
// ---------------------------------------------------------------------------------------------
void D3DDevice_EndVertices(void* /*lpDeviceArg*/)
{
    if (!sbImVertsPending)
    {
        if (!sbImVertsReportedOrphan)
        {
            sbImVertsReportedOrphan = true;
            CgsDev::Log::WriteToLog("[XenonD3D9] EndVertices with no open run - ignored\n");
        }
        return;
    }
    sbImVertsPending = false;

    if (!sbImVertsDrawable)
    {
        // Refused at Begin (oversize / unsupported topology / empty run); the cause was already
        // reported once there. Nothing reaches the device.
        return;
    }
    sbImVertsDrawable = false;

    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice == nullptr)
    {
        return;   // the same silent early-out every sibling shim in this block takes
    }

    const HRESULT lhrDraw = lpDevice->DrawPrimitiveUP(seImVertsPrimitive, suImVertsPrimCount,
                                                      sauImVertsScratch, suImVertsStride);

    // [DIAG one-shot x4] the first immediate runs, with the device state they drew against --
    // the post-fx composite is this path's first live consumer (2026-08-15) and its first boot
    // presented a black frame with no error anywhere, so the answers to "where did the quad go"
    // are printed once instead of guessed: HRESULT, the bound colour target vs the swap chain's
    // back buffer, whether shaders / a declaration are bound, and the render states a full-screen
    // opaque quad depends on. Delete with the bring-up.
    // [DIAG, sampled] every 500th run up to the 3000th (so it lands on WORLD frames, not only the
    // loading screen): the centre pixel of the bound render target after the draw and of the
    // stage-0 texture -- "is the SOURCE black, or is the OUTPUT black?" is the one question the
    // state dump above cannot answer.
    static u32 suRunTotal = 0u;
    ++suRunTotal;

    // [DIAG, mask-alpha] WHICH stride-20 quad is this? gbPostFxSourceSamplerApplied is raised by
    // ApplyPostFxSourceSamplerState -- the seam BrnPostFxShader::Render calls at its SamplerSource
    // bind -- and by nothing else, so it is true on exactly the composite's own draw and false on
    // the bloom down-sample and the two Blur9 passes. Consumed on EVERY immediate draw, so a
    // composite bind that never reached a draw cannot mislabel the next quad.
    // WHY IT IS NEEDED: the run-count cadence below fires on whichever stride-20 quad happens to
    // be run 2000/4000/..., and BrnGame.log runs 8000 and 10000 read rt=320x180 tex0=320x180 --
    // a down-sample, not the composite -- so the motion-blur MASK the composite actually samples
    // (the ALPHA lane of its SOURCE) has never been measured on a blur frame. A zero mask makes
    // the screen velocity exactly zero, which would make the anisotropic-sampler fix invisible;
    // this settles it in one boot.
    const bool lbCompositeDraw = gbPostFxSourceSamplerApplied;
    gbPostFxSourceSamplerApplied = false;
    static u32 suCompositeDraws = 0u;
    if (lbCompositeDraw)
        ++suCompositeDraws;

    // [DIAG, mask-alpha] DID THE MASK STAMP RUN ON THIS FRAME? Read-and-cleared on every
    // composite draw (not only the sampled ones -- a flag left standing would label every later
    // blur-OFF frame as stamped), and latched so the sampled print below can report it. Without
    // it the "source alpha" line is misreadable: an alpha of 0.000 on a frame with motion blur
    // switched off is the world's leftovers, not a broken carrier.
    static bool sbStampedForThisComposite = false;
    if (lbCompositeDraw)
    {
        sbStampedForThisComposite   = sbMaskStampedSinceComposite;
        sbMaskStampedSinceComposite = false;
    }

    // Rung 5 (bloom lit): FOUR stride-20 quads run per frame now (bloom down-sample, two blur passes,
    // the composite), so the cadence is 2000 runs (= 500 frames) up to run 12000 -- the old 500/3000
    // landed every sample inside the title/menu period and read a black centre.
    // Second cadence: every 500th COMPOSITE draw (= every 500 frames, the same rate), which is what
    // guarantees the mask read below lands on the composite's own source rather than on a
    // down-sample buffer.
    const bool lbSampleThisRun =
        ((suRunTotal % 2000u) == 0u && suRunTotal <= 12000u) ||
        (lbCompositeDraw && (suCompositeDraws % 500u) == 0u && suCompositeDraws <= 6000u);
    if (lbSampleThisRun && suImVertsStride == 20u)
    {
        IDirect3DSurface9* lpRt = nullptr;
        IDirect3DBaseTexture9* lpTex0 = nullptr;
        lpDevice->GetRenderTarget(0, &lpRt);
        lpDevice->GetTexture(0, &lpTex0);
        // [DIAG, mask-alpha] lpuCentre / lpuQuarter (either may be null) hand the two sampled
        // ARGB words back to the caller so the motion-blur MASK can be reported in decoded
        // form. The mask is the ALPHA byte of the composite's SOURCE texture -- the resolved
        // down-sample colour -- and `centre` vs `quarter` are two points that differ on a
        // car-select orbit (frame centre = the car, x = width/4 = the world behind it), which
        // makes them a cars-vs-world read-back with no capture tool involved. lpbRead says
        // whether the two words are real (a multisampled or unlockable surface reads nothing).
        auto ReadCentre = [&](IDirect3DSurface9* lpSrc, const char* lpcTag, char* lpcOut, size_t luOut,
                              u32* lpuCentre, u32* lpuQuarter, bool* lpbRead)
        {
            std::snprintf(lpcOut, luOut, "%s=n/a", lpcTag);
            if (lpSrc == nullptr) return;
            D3DSURFACE_DESC lDesc; if (FAILED(lpSrc->GetDesc(&lDesc))) return;
            // GetRenderTargetData CANNOT read a MULTISAMPLED surface -- D3D9 requires the source to
            // be a non-multisampled render target. Since rung 8 the scene colour target IS one, so
            // this probe would print "readback FAILED hr=0x8876086C" every time it landed on a
            // world frame and read as a fault in the very wave that introduced the surface. Say
            // what it actually is instead. (The composite's own rt is the swap chain and tex0 is
            // the RESOLVED down-sample texture, so the sampled pixels this diag exists for are
            // still readable.)
            if (lDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
            {
                std::snprintf(lpcOut, luOut, "%s=MSAA(no readback) %ux%u fmt=%u samples=%u",
                              lpcTag, (unsigned)lDesc.Width, (unsigned)lDesc.Height,
                              (unsigned)lDesc.Format, (unsigned)lDesc.MultiSampleType);
                return;
            }
            IDirect3DSurface9* lpSys = nullptr;
            if (FAILED(lpDevice->CreateOffscreenPlainSurface(lDesc.Width, lDesc.Height, lDesc.Format,
                                                             D3DPOOL_SYSTEMMEM, &lpSys, nullptr)))
                return;
            const HRESULT lhr = lpDevice->GetRenderTargetData(lpSrc, lpSys);
            D3DLOCKED_RECT lLock;
            if (SUCCEEDED(lhr) && SUCCEEDED(lpSys->LockRect(&lLock, nullptr, D3DLOCK_READONLY)))
            {
                const u32* lpRow = reinterpret_cast<const u32*>(
                    static_cast<const u8*>(lLock.pBits) + (lDesc.Height / 2u) * lLock.Pitch);
                const u32 luC = lpRow[lDesc.Width / 2u];
                const u32 luQ = lpRow[lDesc.Width / 4u];
                if (lpuCentre  != nullptr) *lpuCentre  = luC;
                if (lpuQuarter != nullptr) *lpuQuarter = luQ;
                if (lpbRead    != nullptr) *lpbRead    = true;
                std::snprintf(lpcOut, luOut, "%s=%ux%u fmt=%u centre=%08X quarter=%08X",
                              lpcTag, (unsigned)lDesc.Width, (unsigned)lDesc.Height, (unsigned)lDesc.Format, luC, luQ);
                lpSys->UnlockRect();
            }
            else
            {
                std::snprintf(lpcOut, luOut, "%s=readback FAILED hr=0x%08X", lpcTag, (unsigned)lhr);
            }
            lpSys->Release();
        };
        char lacRt[160], lacTex[160], lacTex1[160];
        u32  luTex0Centre = 0u, luTex0Quarter = 0u;
        bool lbTex0Read = false;
        ReadCentre(lpRt, "rt", lacRt, sizeof(lacRt), nullptr, nullptr, nullptr);
        IDirect3DSurface9* lpTexSurf = nullptr;
        if (lpTex0 != nullptr && lpTex0->GetType() == D3DRTYPE_TEXTURE)
            static_cast<IDirect3DTexture9*>(lpTex0)->GetSurfaceLevel(0, &lpTexSurf);
        ReadCentre(lpTexSurf, "tex0", lacTex, sizeof(lacTex),
                   &luTex0Centre, &luTex0Quarter, &lbTex0Read);
        // [DIAG rung 5, bloom] the stage-1 texture (the composite's SamplerBloom == the bloom pool
        // target after the three bloom passes) and the five pixel constants the composite reads
        // (c0 GlobalParams, c1 BloomColour, c2 VignetteInnerRgbPlusMul, c3 VignetteOuterRgbPlusAdd,
        // c4 Tint2dColour): the first bloom-lit boot presented BLACK with a bright source, and these
        // are the only inputs that can turn a bright source black. Delete with the bring-up.
        IDirect3DBaseTexture9* lpTex1 = nullptr;
        lpDevice->GetTexture(1, &lpTex1);
        IDirect3DSurface9* lpTex1Surf = nullptr;
        if (lpTex1 != nullptr && lpTex1->GetType() == D3DRTYPE_TEXTURE)
            static_cast<IDirect3DTexture9*>(lpTex1)->GetSurfaceLevel(0, &lpTex1Surf);
        ReadCentre(lpTex1Surf, "tex1", lacTex1, sizeof(lacTex1), nullptr, nullptr, nullptr);
        float lafPs[5][4] = {};
        lpDevice->GetPixelShaderConstantF(0, &lafPs[0][0], 5);
        char lacMsg[900];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[ImVerts pixels run %u] %s | %s | %s | ps c0=(%g %g %g %g) c1=(%g %g %g %g) "
                      "c2=(%g %g %g %g) c3=(%g %g %g %g) c4=(%g %g %g %g)\n",
                      static_cast<unsigned>(suRunTotal), lacRt, lacTex, lacTex1,
                      lafPs[0][0], lafPs[0][1], lafPs[0][2], lafPs[0][3],
                      lafPs[1][0], lafPs[1][1], lafPs[1][2], lafPs[1][3],
                      lafPs[2][0], lafPs[2][1], lafPs[2][2], lafPs[2][3],
                      lafPs[3][0], lafPs[3][1], lafPs[3][2], lafPs[3][3],
                      lafPs[4][0], lafPs[4][1], lafPs[4][2], lafPs[4][3]);
        CgsDev::Log::WriteToLog(lacMsg);
        // [DIAG, mask-alpha] THE MASK, DECODED. tex0 of a stride-20 quad on the composite call
        // IS SamplerSource (the resolved down-sample colour), so its alpha byte is exactly what
        // the blur permutations multiply the screen velocity by. Printed as bytes AND as the
        // amounts they stand for, so a boot log alone answers "did the car and the world get
        // DIFFERENT blur?" -- with BRN_POSTFX_MASK_TEST=0,1 on the car-select orbit the two
        // must differ.
        if (lbTex0Read)
        {
            char lacMask[640];
            std::snprintf(lacMask, sizeof(lacMask),
                          "[mask-alpha] source alpha run %u: composite=%d (composite draws %u)"
                          " stamped=%d centre=%u (%.3f) quarter=%u (%.3f)"
                          " -- the motion-blur mask the composite reads at those two pixels."
                          " composite=0 is a bloom/DoF down-sample quad and says nothing about"
                          " the mask. stamped=0 means PCStampMotionBlurMask did NOT run on this"
                          " frame -- motion blur was inactive, the composite picked a NON-blur"
                          " permutation and never sampled the lane, so the alpha shown is the"
                          " world pass's leftovers and says nothing about the mask either.\n",
                          static_cast<unsigned>(suRunTotal),
                          lbCompositeDraw ? 1 : 0,
                          static_cast<unsigned>(suCompositeDraws),
                          sbStampedForThisComposite ? 1 : 0,
                          static_cast<unsigned>((luTex0Centre  >> 24) & 0xFFu),
                          static_cast<double>((luTex0Centre  >> 24) & 0xFFu) / 255.0,
                          static_cast<unsigned>((luTex0Quarter >> 24) & 0xFFu),
                          static_cast<double>((luTex0Quarter >> 24) & 0xFFu) / 255.0);
            CgsDev::Log::WriteToLog(lacMask);
        }
        if (lpTex1Surf) lpTex1Surf->Release();
        if (lpTex1)     lpTex1->Release();
        if (lpTexSurf) lpTexSurf->Release();
        if (lpTex0)    lpTex0->Release();
        if (lpRt)      lpRt->Release();
    }

    static u32 suDiagRuns = 0u;
    if (suDiagRuns < 4u)
    {
        ++suDiagRuns;
        IDirect3DSurface9* lpBoundRt = nullptr;  lpDevice->GetRenderTarget(0, &lpBoundRt);
        IDirect3DSurface9* lpBackBuf = nullptr;  lpDevice->GetBackBuffer(0u, 0u, D3DBACKBUFFER_TYPE_MONO, &lpBackBuf);
        IDirect3DSurface9* lpDepth   = nullptr;  lpDevice->GetDepthStencilSurface(&lpDepth);
        IDirect3DVertexShader9*      lpVs   = nullptr; lpDevice->GetVertexShader(&lpVs);
        IDirect3DPixelShader9*       lpPs   = nullptr; lpDevice->GetPixelShader(&lpPs);
        IDirect3DVertexDeclaration9* lpDecl = nullptr; lpDevice->GetVertexDeclaration(&lpDecl);
        IDirect3DBaseTexture9*       lpTex0 = nullptr; lpDevice->GetTexture(0, &lpTex0);
        DWORD luBlend = 0, luZ = 0, luZW = 0, luCull = 0, luCw = 0, luScis = 0, luFill = 0, luATest = 0, luSrc = 0, luDst = 0;
        lpDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &luBlend);
        lpDevice->GetRenderState(D3DRS_ZENABLE, &luZ);
        lpDevice->GetRenderState(D3DRS_ZWRITEENABLE, &luZW);
        lpDevice->GetRenderState(D3DRS_CULLMODE, &luCull);
        lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &luCw);
        lpDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &luScis);
        lpDevice->GetRenderState(D3DRS_FILLMODE, &luFill);
        lpDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &luATest);
        lpDevice->GetRenderState(D3DRS_SRCBLEND, &luSrc);
        lpDevice->GetRenderState(D3DRS_DESTBLEND, &luDst);
        D3DVIEWPORT9 lVp; std::memset(&lVp, 0, sizeof(lVp)); lpDevice->GetViewport(&lVp);
        RECT lSc; std::memset(&lSc, 0, sizeof(lSc)); lpDevice->GetScissorRect(&lSc);
        char lacMsg[512];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[ImVerts diag %u] prim=%d prims=%u stride=%u hr=0x%08X rt=%p backbuf=%p (%s) depth=%p"
                      " vs=%p ps=%p decl=%p tex0=%p | blend=%lu src=%lu dst=%lu z=%lu zw=%lu cull=%lu cw=0x%lX"
                      " scissor=%lu[%ld,%ld,%ld,%ld] fill=%lu atest=%lu vp=%lux%lu@%lu,%lu z%.2f-%.2f\n",
                      static_cast<unsigned>(suDiagRuns), static_cast<int>(seImVertsPrimitive),
                      static_cast<unsigned>(suImVertsPrimCount), static_cast<unsigned>(suImVertsStride),
                      static_cast<unsigned>(lhrDraw), static_cast<void*>(lpBoundRt), static_cast<void*>(lpBackBuf),
                      (lpBoundRt == lpBackBuf) ? "SWAP CHAIN" : "OFF-SCREEN", static_cast<void*>(lpDepth),
                      static_cast<void*>(lpVs), static_cast<void*>(lpPs), static_cast<void*>(lpDecl), static_cast<void*>(lpTex0),
                      luBlend, luSrc, luDst, luZ, luZW, luCull, luCw, luScis, lSc.left, lSc.top, lSc.right, lSc.bottom,
                      luFill, luATest, lVp.Width, lVp.Height, lVp.X, lVp.Y, (double)lVp.MinZ, (double)lVp.MaxZ);
        CgsDev::Log::WriteToLog(lacMsg);
        // ...and the constant file the draw read: PS c0..c4 / VS c0..c1 (the composite's five + two).
        float lafPs[5 * 4] = { 0 }; lpDevice->GetPixelShaderConstantF(0, lafPs, 5);
        float lafVs[2 * 4] = { 0 }; lpDevice->GetVertexShaderConstantF(0, lafVs, 2);
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[ImVerts diag %u] ps c0={%g %g %g %g} c1={%g %g %g %g} c2={%g %g %g %g} c3={%g %g %g %g}"
                      " c4={%g %g %g %g} | vs c0={%g %g %g %g} c1={%g %g %g %g}\n",
                      static_cast<unsigned>(suDiagRuns),
                      lafPs[0], lafPs[1], lafPs[2], lafPs[3], lafPs[4], lafPs[5], lafPs[6], lafPs[7],
                      lafPs[8], lafPs[9], lafPs[10], lafPs[11], lafPs[12], lafPs[13], lafPs[14], lafPs[15],
                      lafPs[16], lafPs[17], lafPs[18], lafPs[19],
                      lafVs[0], lafVs[1], lafVs[2], lafVs[3], lafVs[4], lafVs[5], lafVs[6], lafVs[7]);
        CgsDev::Log::WriteToLog(lacMsg);
        // ...and the first vertex as written into the scratch (position xyz, uv).
        const float* const lpfV0 = reinterpret_cast<const float*>(sauImVertsScratch);
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[ImVerts diag %u] v0={%g %g %g | %g %g} v1={%g %g %g | %g %g} v3={%g %g %g | %g %g}\n",
                      static_cast<unsigned>(suDiagRuns),
                      lpfV0[0], lpfV0[1], lpfV0[2], lpfV0[3], lpfV0[4],
                      lpfV0[5], lpfV0[6], lpfV0[7], lpfV0[8], lpfV0[9],
                      lpfV0[15], lpfV0[16], lpfV0[17], lpfV0[18], lpfV0[19]);
        CgsDev::Log::WriteToLog(lacMsg);
        if (lpBoundRt) lpBoundRt->Release();
        if (lpBackBuf) lpBackBuf->Release();
        if (lpDepth)   lpDepth->Release();
        if (lpVs)      lpVs->Release();
        if (lpPs)      lpPs->Release();
        if (lpDecl)    lpDecl->Release();
        if (lpTex0)    lpTex0->Release();
    }
}

// The low-level sampler-state setter (X360 sub_827E8950). The Xenon sampler
// state object is a packed GPU register block; translating it waits on the
// TextureState/MaterialState reconciliation. FLAG [PC bring-up]: no-op.
void* SetSamplerStateLowLevel(void* lpState, u32 /*luSamplerId*/, bool /*lbWasUnset*/)
{
    return lpState;
}

// ---- packed render-state fast-sets ------------------------------------------
// Each Xenon fast-set pokes ONE field of a Xenos GPU register shadow, and the
// ARTIST image carries every one of those thunks, so the encodings below are
// read straight off them rather than guessed:
//
//   _ZFunc      @0x82939870   (16*Value) & 0x70      RB_DEPTHCONTROL ZFUNC
//   _AlphaFunc  @0x82939328   Value & 7              RB_COLORCONTROL ALPHA_FUNC
//   _StencilFunc@0x82939A48   (Value<<8) & 0x700     RB_DEPTHCONTROL STENCILFUNC
//   _StencilFail@0x82939AD8   (Value<<11) & 0x3800   RB_DEPTHCONTROL STENCILFAIL
//   _CullMode   @0x82938978   Value & 7              PA_SU_SC_MODE_CNTL [2:0]
//   _FillMode   @0x82938A08   (8*Value) & 0x7F8      PA_SU_SC_MODE_CNTL POLY_MODE
//   _AlphaRef   @0x82939258   Value * (1/255)        RB_ALPHA_REF (0..255 in)
//   _PrimitiveResetEnable @0x8293BB68  (Value<<21) & 0x200000
//
// EVERY comparison/stencil-op field is the GPU's own ZERO-based enumeration
// (NEVER=0..ALWAYS=7, KEEP=0..DECR_WRAP=7) while PC Direct3D 9's D3DCMPFUNC and
// D3DSTENCILOP are ONE-based -- so these leaves add 1. Forwarding the raw value
// INVERTED every comparison (the world's AlphaFunc 4 = GREATER would arrive as
// D3DCMP_LESSEQUAL). Bug class (c).
namespace
{
    // Xenos 0-based comparison function -> D3DCMPFUNC (1-based).
    inline DWORD XenonCompareToD3D9(u32 luValue)
    {
        return static_cast<DWORD>((luValue & 7u) + 1u);
    }

    // Xenos 0-based stencil op -> D3DSTENCILOP. The two enumerations run in the
    // same order (KEEP, ZERO, REPLACE, INCR-clamp, DECR-clamp, INVERT, INCR-wrap,
    // DECR-wrap), so the PC value is again the Xenos value plus one.
    inline DWORD XenonStencilOpToD3D9(u32 luValue)
    {
        return static_cast<DWORD>((luValue & 7u) + 1u);
    }

    // PA_SU_SC_MODE_CNTL [2:0] = { CULL_FRONT (bit 0), CULL_BACK (bit 1),
    // FACE (bit 2) }, straight from the register the X360 setter packs
    // (_CullMode @0x82938978 stores `Value & 7` verbatim), so the Xbox 360
    // D3DCULL enumerators ARE that field: NONE = 0, cull-front = 1, cull-back = 2.
    //
    // FACE selects which winding is the FRONT face, and the hardware definition is
    // FACE = 0 -> COUNTER-CLOCKWISE is front facing (1 -> clockwise). So with the
    // world's FACE = 0: bit 0 (cull front) culls CCW == D3DCULL_CCW, and bit 1
    // (cull back) culls CW == D3DCULL_CW -- the world's CullMode = 2 is D3DCULL_CW.
    //
    // HISTORY (mirror wave, 2026-07-29): this used to read FACE = 0 as "clockwise is
    // front", i.e. the exact opposite, because measuring it that way was the only
    // reading that kept the city solid. That measurement was taken through a MIRRORED
    // camera (the world bring-up producer built its view basis with the opposite
    // handedness to CgsGraphics::Camera::LookAt), and a mirror inverts every
    // triangle's screen-space winding -- so the compensating reading looked correct.
    // With the camera basis fixed in BrnWorldModule the register-accurate reading is
    // the one that leaves the city solid, and the inverted one erases it.
    inline DWORD XenonCullToD3D9(u32 luValue)
    {
        const bool lbCullFront  = (luValue & 1u) != 0;
        const bool lbCullBack   = (luValue & 2u) != 0;
        const bool lbCcwIsFront = (luValue & 4u) == 0;   // FACE
        if (lbCullFront == lbCullBack)
            return D3DCULL_NONE;      // neither, or both (nothing would draw anyway)
        const bool lbCullCounterClockwise = lbCullFront ? lbCcwIsFront : !lbCcwIsFront;
        return lbCullCounterClockwise ? D3DCULL_CCW : D3DCULL_CW;
    }

    // POLY_MODE 0 (the only value the world carries) is the hardware's "no
    // polygon-mode override" == solid fill.
    inline DWORD XenonFillToD3D9(u32 luValue)
    {
        switch (luValue & 3u)
        {
        case 1:  return D3DFILL_WIREFRAME;   // POLY_MODE = dual mode, line
        case 2:  return D3DFILL_POINT;
        default: return D3DFILL_SOLID;
        }
    }

    // RB_BLENDCONTROL blend-factor field -> D3DBLEND.
    DWORD XenonBlendFactorToD3D9(u32 luValue)
    {
        switch (luValue)
        {
        case 0:  return D3DBLEND_ZERO;
        case 1:  return D3DBLEND_ONE;
        case 4:  return D3DBLEND_SRCCOLOR;
        case 5:  return D3DBLEND_INVSRCCOLOR;
        case 6:  return D3DBLEND_SRCALPHA;
        case 7:  return D3DBLEND_INVSRCALPHA;
        case 8:  return D3DBLEND_DESTALPHA;
        case 9:  return D3DBLEND_INVDESTALPHA;
        case 10: return D3DBLEND_DESTCOLOR;
        case 11: return D3DBLEND_INVDESTCOLOR;
        case 12: return D3DBLEND_SRCALPHASAT;
        case 13: return D3DBLEND_BLENDFACTOR;
        case 14: return D3DBLEND_INVBLENDFACTOR;
        default:
            LogOnce("blendfactor",
                    "[XenonD3D9] unmapped Xenos blend factor - treated as ONE\n");
            return D3DBLEND_ONE;
        }
    }

    // RB_BLENDCONTROL COMB_FCN field -> D3DBLENDOP.
    DWORD XenonBlendOpToD3D9(u32 luValue)
    {
        switch (luValue & 7u)
        {
        case 0:  return D3DBLENDOP_ADD;            // DST_PLUS_SRC
        case 1:  return D3DBLENDOP_SUBTRACT;       // SRC_MINUS_DST
        case 2:  return D3DBLENDOP_MIN;
        case 3:  return D3DBLENDOP_MAX;
        case 4:  return D3DBLENDOP_REVSUBTRACT;    // DST_MINUS_SRC
        default: return D3DBLENDOP_ADD;
        }
    }
}

// D3DDevice_SetBlendState @0x8293DA80 stores its argument VERBATIM into the
// device's per-render-target RB_BLENDCONTROL shadow, so the argument IS that
// Xenos register:
//   COLOR_SRCBLEND  [4:0]   COLOR_COMB_FCN  [7:5]   COLOR_DESTBLEND [12:8]
//   ALPHA_SRCBLEND [20:16]  ALPHA_COMB_FCN [23:21]  ALPHA_DESTBLEND [28:24]
// The world's material states carry exactly two values and both corroborate the
// split: 0x00010001 = ONE/ZERO/add on both channels (opaque -- 9 of the 11
// sampled states) and 0x07060706 = SRC_ALPHA/INV_SRC_ALPHA/add (the standard
// alpha blend). Direct3D 9 has no packed form, so the fields become the separate
// D3DRS_SRCBLEND/DESTBLEND/BLENDOP + separate-alpha trio, and
// D3DRS_ALPHABLENDENABLE is DERIVED: a ONE/ZERO/add blend is exactly "no
// blending", which is how the console expresses an opaque material.
//
// FLAG PC-platform leaf: PC D3D9 has ONE blend state for all render targets
// (independent per-RT blending arrives with D3D10), so render targets 1..3 are
// accepted and dropped; the world only ever differs on target 0.
void D3DDevice_SetBlendState(IDirect3DDevice9*, u32 luRenderTargetIndex, u32 luBlendState)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice == nullptr || luRenderTargetIndex != 0)
        return;

    const u32 luColourSrc  =  luBlendState        & 0x1Fu;
    const u32 luColourOp   = (luBlendState >>  5) & 0x07u;
    const u32 luColourDst  = (luBlendState >>  8) & 0x1Fu;
    const u32 luAlphaSrc   = (luBlendState >> 16) & 0x1Fu;
    const u32 luAlphaOp    = (luBlendState >> 21) & 0x07u;
    const u32 luAlphaDst   = (luBlendState >> 24) & 0x1Fu;

    // [FLAG PC bring-up diagnostic] see THE DEST-ALPHA CENSUS above. Counting only; it reads
    // the four decoded factor fields and changes no state.
    BlendCensus_Account(luColourSrc, luColourDst, luAlphaSrc, luAlphaDst);

    const bool lbColourOpaque = (luColourSrc == 1u && luColourDst == 0u && luColourOp == 0u);
    const bool lbAlphaOpaque  = (luAlphaSrc  == 1u && luAlphaDst  == 0u && luAlphaOp  == 0u);
    const bool lbBlend = !(lbColourOpaque && lbAlphaOpaque);

    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, lbBlend ? TRUE : FALSE);
    if (!lbBlend)
        return;

    lpDevice->SetRenderState(D3DRS_SRCBLEND,  XenonBlendFactorToD3D9(luColourSrc));
    lpDevice->SetRenderState(D3DRS_DESTBLEND, XenonBlendFactorToD3D9(luColourDst));
    lpDevice->SetRenderState(D3DRS_BLENDOP,   XenonBlendOpToD3D9(luColourOp));

    const bool lbSeparate = (luAlphaSrc != luColourSrc)
                         || (luAlphaDst != luColourDst)
                         || (luAlphaOp  != luColourOp);
    lpDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, lbSeparate ? TRUE : FALSE);
    if (lbSeparate)
    {
        lpDevice->SetRenderState(D3DRS_SRCBLENDALPHA,  XenonBlendFactorToD3D9(luAlphaSrc));
        lpDevice->SetRenderState(D3DRS_DESTBLENDALPHA, XenonBlendFactorToD3D9(luAlphaDst));
        lpDevice->SetRenderState(D3DRS_BLENDOPALPHA,   XenonBlendOpToD3D9(luAlphaOp));
    }
}

void D3DDevice_SetRenderState_ColorWriteEnable(IDirect3DDevice9*, u32 luValue)
{
    // [FLAG PC bring-up diagnostic] the second half of THE DEST-ALPHA CENSUS above: WHICH
    // colour-write words the shipped material data actually pushes. The ALPHA bit being set on
    // essentially every world material is what makes the alpha lane's contents "whatever the
    // last draw wrote" -- the fact the mask stamp is designed around by running AFTER every
    // world draw, rather than by masking that bit per draw.
    suBlendCensusWordMask |= (luValue & 0x0Fu);

    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE, luValue);
}
// FLAG PC-platform leaf: render targets 1..3. The world pass has a single colour
// target, and D3D9's D3DRS_COLORWRITEENABLE1..3 only apply with multiple render
// targets bound, so these are accepted and dropped rather than mis-applied to
// target 0.
void D3DDevice_SetRenderState_ColorWriteEnable1(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_ColorWriteEnable2(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_ColorWriteEnable3(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_BlendFactor(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_BLENDFACTOR, luValue);
}
// =============================================================================
// ALPHA-TO-MASK -- the Xenos' MSAA coverage feature, and on PC the vendor
// alpha-to-coverage hooks. REAL since rung 9; these were empty leaves before.
//
// WHAT THE XENOS DOES. D3DRS_ALPHATOMASKENABLE drives RB_COLORCONTROL's
// ALPHA_TO_MASK_ENABLE bit and D3DRS_ALPHATOMASKOFFSETS drives that same register's four
// 2-bit ALPHA_TO_MASK_OFFSET0..3 fields. With the bit set the hardware ANDs the sample
// mask with a mask derived from the fragment's ALPHA, so partial alpha becomes partial
// COVERAGE; the four offsets are the DITHER THRESHOLDS for the four pixels of a quad --
// offset0 -> (0,0), offset1 -> (0,1), offset2 -> (1,0), offset3 -> (1,1) -- so that a
// constant alpha does not resolve to the same all-or-nothing answer in every pixel. It is
// a MULTISAMPLE feature: with one sample per pixel there is no mask to build.
//
// ⚠ IT IS INDEPENDENT OF THE ALPHA TEST ON THE CONSOLE, AND THAT IS SOURCED, NOT
// ASSUMED. ALPHA_TEST_ENABLE and ALPHA_TO_MASK_ENABLE are SEPARATE BITS of
// RB_COLORCONTROL (bits 3 and 4; the register layout is public in Xenia's
// gpu/registers.h, and the same fields are documented on same-family AMD silicon in the
// public "Radeon Evergreen 3D Register Reference Guide" rev 1.0, DB_ALPHA_TO_MASK, which
// also gives the per-pixel-in-quad meaning of the four offsets and "set to 2 for
// non-dithered, a unique 0-3 for dithered"). An Xbox 360 SDK sample (Graphics/
// AdvancedXPS) uses alpha-to-mask for billboards with no alpha test at all, which
// confirms the independence behaviourally. The PC hooks below do NOT share that
// independence, which is the one place this leaf has to deviate -- see below.
// [Provenance: the RB_COLORCONTROL vocabulary is also what this file's own fast-set
// translation table already carries, read off the X360 thunks -- _AlphaFunc @0x82939328
// pokes ALPHA_FUNC in the same register.]
//
// THE OFFSETS WORD IS ALWAYS 0x87, AND 0x87 DECODES. Four 2-bit fields at bits [1:0],
// [3:2], [5:4], [7:6]: 0x87 = 0b10'00'01'11 -> 3, 1, 0, 2 -- four UNIQUE values, i.e.
// the fully dithered pattern, which is exactly the constant the Xbox 360 headers name
// D3DALPHATOMASK_DITHERED (0x87); its sibling D3DALPHATOMASK_SOLID (0xAA) decodes to
// 2,2,2,2, the "non-dithered" setting the Evergreen guide describes. The two names
// therefore cross-confirm the decode independently of the header they come from (which
// is a mirrored SDK, not published documentation). Measured, not assumed, that the game
// only ever asks for 0x87: a probe boot of these two leaves counted ~2,000 enable=1
// requests in 19,899 calls with the offsets word 0x87 on every one of them, and the
// tree's own constructed states agree (CgsBlendStateFactory.cpp:154/158, nine slots,
// enable 0 offsets 0x87; BrnPostFxShader.cpp:1225 offsets 0x87).
//
// WHY THIS IS A RECONSTRUCTION AND NOT AN ENHANCEMENT. The requests come from the
// SHIPPED MATERIAL DATA -- the BlendState blob serialised in the bundles (19 words per
// blendstate.h, of which the binder pushes the first 18), bound
// through shadowingdevice.cpp's Xbox2SetStateLowLevelShadowed (word[10] enable,
// word[8] offsets; blendstate.h:28/69-71). ~10% of blend-state binds ask for it. It is
// the console's own answer for foliage, fences, railings and wires, and until rung 9 PC
// dropped it on the floor.
//
// ⚠ THE BANNER THAT USED TO BE HERE WAS FALSE IN BOTH HALVES. It claimed "the world
// pass runs unmultisampled, where alpha-to-mask is a no-op on the console too". The
// console world pass is multisampled (that is what EDRAM tiling is FOR), and since
// rung 8 the PC scene target is too (PostFxRenderTargetPCLeaf.cpp honours
// mn32MultiSampleFormat; BrnRendererModule::mbMultisampledBackbuffer is the console's
// constant TRUE). Nothing in the tree still says "unmultisampled" -- see the report.
//
// WHAT D3D9 DOES INSTEAD. There is no standard alpha-to-coverage in Direct3D 9; each
// vendor smuggles one through an unrelated render state:
//   * NVIDIA -- enable = SetRenderState(D3DRS_ADAPTIVETESS_Y, 'ATOC'), disable =
//     D3DFMT_UNKNOWN; capability = CheckDeviceFormat(usage 0, D3DRTYPE_SURFACE, 'ATOC').
//     All three verbatim from NVIDIA's "Antialiasing with Transparency" technical report
//     (2004/2005). ('SSAA' on the same render state is SUPERSAMPLE transparency and is
//     NOT wanted here; nothing in this tree writes D3DRS_ADAPTIVETESS_Y but this file's
//     AlphaCoverage_Reconcile, which this leaf drives.)
//   * AMD -- enable = SetRenderState(D3DRS_POINTSIZE, 'A2M1'), disable = 'A2M0', and the
//     toggle is GLOBAL. From "Advanced DX9 Capabilities for ATI Radeon Cards"
//     (2009-09-09), the same document as the RESZ / INTZ / DF24 / Fetch4 hooks this file
//     already uses. That document publishes NO capability probe for this one feature --
//     see AlphaCoveragePath for how it is detected instead, and why probing 'A2M1' with
//     CheckDeviceFormat would have been an invented convention.
//   * Anything else: no hook. The leaf says so ONCE and leaves the cut-outs with the hard
//     alpha-tested edge they have today.
//
// THE DEVIATIONS, STATED RATHER THAN HIDDEN. (1) NEITHER hook exposes the per-pixel
// offsets, so the console's 0x87 dither becomes the vendor's own fixed pattern -- the
// cut-out is dithered either way, but not with the console's exact quad pattern.
// (2) THE NVIDIA HOOK NEEDS THE ALPHA TEST SWITCHED ON, where the console's
// alpha-to-mask does not (see above). That precondition turns out to be met by the
// console's OWN state and so costs no deviation at all: a probe boot recorded, for every
// alpha-to-mask request, the AlphaTestEnable word the SAME blend-state bind issues next
// -- on=11500, off=0, across title, intro, junkyard, car select and driving. So this leaf
// does NOT force the alpha test on (which WOULD have been a real deviation: it changes
// which pixels are discarded); it relies on the material's own alpha test, gates only the
// NVIDIA path on it, and D3DDevice_SetRenderState_AlphaTestEnable below logs once if the
// combination that probe never saw ever turns up.
//
// The state, the capability probe and the reconciler are at the top of this file (see
// the ALPHA-TO-COVERAGE banner in the first anonymous namespace) because the world-pass
// boundary, the shadow-map bracket and the scene blit all have to reconcile through
// them too.
// =============================================================================
void D3DDevice_SetRenderState_AlphaToMaskEnable(IDirect3DDevice9*, u32 luValue)
{
    sbA2cRequested = (luValue != 0u);
    AlphaCoverage_Reconcile();
}

// Neither vendor hook exposes the per-pixel-in-quad dither thresholds, so the console's
// offsets word cannot be honoured -- the vendor's own fixed pattern is used instead. The
// word is accepted and RECORDED, and a value other than the console's constant 0x87 says
// so once, because it would mean a material this reconstruction has not accounted for.
// The one other value with a name is 0xAA (D3DALPHATOMASK_SOLID = offsets 2,2,2,2, the
// NON-dithered setting): it does not appear anywhere in the shipped data on the probed
// flow, and if it ever does it is worth knowing that PC cannot express it -- the vendor
// hooks always dither.
void D3DDevice_SetRenderState_AlphaToMaskOffsets(IDirect3DDevice9*, u32 luValue)
{
    if (luValue == suA2cOffsets)
        return;
    suA2cOffsets = luValue;
    if (luValue != KU_A2M_OFFSETS_DITHERED)
    {
        char lacMessage[320];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[a2c] alpha-to-mask OFFSETS 0x%08X requested%s -- the console's constant"
                      " is 0x%08X (D3DALPHATOMASK_DITHERED). D3D9's vendor alpha-to-coverage"
                      " hooks expose no dither-threshold control, so the vendor's own fixed"
                      " pattern is used and this word is recorded only.\n",
                      static_cast<unsigned>(luValue),
                      (luValue == 0xAAu) ? " (D3DALPHATOMASK_SOLID, i.e. NON-dithered)" : "",
                      static_cast<unsigned>(KU_A2M_OFFSETS_DITHERED));
        LogOnce("a2c-offsets", lacMessage);
    }
}
// FLAG PC-platform leaf: high-precision blending selects the Xenos' 10.10.10.2 /
// FP16 blend path per render target. The PC back buffer's format already fixes
// the blend precision, so there is nothing to select.
void D3DDevice_SetRenderState_HighPrecisionBlendEnable(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HighPrecisionBlendEnable1(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HighPrecisionBlendEnable2(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HighPrecisionBlendEnable3(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_AlphaTestEnable(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE, luValue != 0);

    // ⚠ ON THE NVIDIA PATH THIS LEAF IS HALF OF ALPHA-TO-COVERAGE, NOT JUST THE ALPHA TEST.
    // The ATOC hook keys off D3DRS_ALPHATESTENABLE, and the blend-state binder issues
    // word[10] (alpha-to-mask) BEFORE word[16] (this one) on BOTH of its paths
    // (shadowingdevice.cpp: unset path :619 then :625, incremental :686 then :711) -- so THIS
    // is the call at which a material's alpha-to-mask + alpha-test pair is finally settled.
    // Reconciling here
    // is what stops the vendor state being decided against the PREVIOUS material's alpha
    // test. (Harmless on the AMD path, which is not gated on the alpha test at all.)
    AlphaCoverage_Reconcile();

    // The combination the console allows and the NVIDIA hook cannot express. Probed
    // 2026-08-16 over title / intro / junkyard / car select / driving: 11500 alpha-to-mask
    // requests, ALL with the alpha test on, 0 without -- so this line was not expected to
    // appear. KNOWN GAP: it can only fire when the binder actually re-issues word[16], and
    // the incremental path skips an UNCHANGED word, so a material whose word[16] was already
    // 0 would take the silent hard-edge route without a line. That is the residual, and it is
    // the same materials the probe measured as non-existent.
    if (luValue == 0u && sbA2cRequested && siA2cPath == E_A2C_PATH_NVIDIA)
    {
        LogOnce("a2c-no-alpha-test",
                "[a2c] a blend state asked for alpha-to-mask with the alpha test OFF. The"
                " NVIDIA ATOC hook keys off the alpha test, so that material keeps its hard"
                " cut-out edge; the console, where the two are independent register bits,"
                " would have dithered it.\n");
    }
}
void D3DDevice_SetRenderState_AlphaRef(IDirect3DDevice9*, u32 luValue)
{
    // The Xenon thunk converts to a 0..1 float (Value * 1/255), i.e. the incoming
    // word is already the 0..255 reference D3DRS_ALPHAREF wants.
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_ALPHAREF, luValue);
}
void D3DDevice_SetRenderState_AlphaFunc(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_ALPHAFUNC, XenonCompareToD3D9(luValue));
}

// ---- the depth/stencil half (X360 applier @0x827E8150) ----------------------
void D3DDevice_SetRenderState_ZEnable(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_ZENABLE, luValue != 0 ? D3DZB_TRUE : D3DZB_FALSE);
}
void D3DDevice_SetRenderState_ZWriteEnable(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, luValue != 0);
}
void D3DDevice_SetRenderState_ZFunc(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_ZFUNC, XenonCompareToD3D9(luValue));
}
void D3DDevice_SetRenderState_StencilEnable(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILENABLE, luValue != 0);
}
void D3DDevice_SetRenderState_TwoSidedStencilMode(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, luValue != 0);
}
void D3DDevice_SetRenderState_StencilFunc(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILFUNC, XenonCompareToD3D9(luValue));
}
void D3DDevice_SetRenderState_StencilFail(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILFAIL, XenonStencilOpToD3D9(luValue));
}
void D3DDevice_SetRenderState_StencilZFail(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILZFAIL, XenonStencilOpToD3D9(luValue));
}
void D3DDevice_SetRenderState_StencilPass(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILPASS, XenonStencilOpToD3D9(luValue));
}
void D3DDevice_SetRenderState_CCWStencilFunc(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_CCW_STENCILFUNC, XenonCompareToD3D9(luValue));
}
void D3DDevice_SetRenderState_CCWStencilFail(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_CCW_STENCILFAIL, XenonStencilOpToD3D9(luValue));
}
void D3DDevice_SetRenderState_CCWStencilZFail(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL, XenonStencilOpToD3D9(luValue));
}
void D3DDevice_SetRenderState_CCWStencilPass(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_CCW_STENCILPASS, XenonStencilOpToD3D9(luValue));
}
void D3DDevice_SetRenderState_StencilRef(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILREF, luValue);
}
void D3DDevice_SetRenderState_StencilMask(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILMASK, luValue);
}
void D3DDevice_SetRenderState_StencilWriteMask(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_STENCILWRITEMASK, luValue);
}
// FLAG PC-platform leaf: the Xenos keeps a SECOND (counter-clockwise) stencil
// reference/mask/write-mask; PC D3D9 shares one set between both faces, so these
// three are accepted and dropped. The world data carries the same values in both
// sets, so nothing is lost there.
void D3DDevice_SetRenderState_CCWStencilRef(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_CCWStencilMask(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_CCWStencilWriteMask(IDirect3DDevice9*, u32) {}
// FLAG PC-platform leaf: hierarchical stencil is Xenos-only (the coarse
// HiZ/HiStencil unit in EDRAM). It is a pure culling accelerator -- disabling it
// changes speed, never pixels.
void D3DDevice_SetRenderState_HiStencilEnable(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HiStencilWriteEnable(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HiStencilFunc(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HiStencilRef(IDirect3DDevice9*, u32) {}

// ---- the rasteriser half (X360 applier @0x827E8690) -------------------------
void D3DDevice_SetRenderState_CullMode(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_CULLMODE, XenonCullToD3D9(luValue));
}
void D3DDevice_SetRenderState_FillMode(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_FILLMODE, XenonFillToD3D9(luValue));
}
void D3DDevice_SetRenderState_ScissorTestEnable(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, luValue != 0);
}
void D3DDevice_SetRenderState_DepthBias(IDirect3DDevice9*, u32 luFloatAsDword)
{
    // Both sides take the float's bit pattern in a DWORD.
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_DEPTHBIAS, luFloatAsDword);
}
void D3DDevice_SetRenderState_SlopeScaleDepthBias(IDirect3DDevice9*, u32 luFloatAsDword)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, luFloatAsDword);
}
void D3DDevice_SetRenderState_MultiSampleAntiAlias(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, luValue != 0);
}
void D3DDevice_SetRenderState_MultiSampleMask(IDirect3DDevice9*, u32 luValue)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, luValue);
}
// FLAG PC-platform leaf: ViewportEnable selects whether the Xenos applies the
// viewport transform in the vertex pipe (the console can emit pre-transformed
// clip coordinates); the D3D9 runtime always applies it. HalfPixelOffset selects
// the Xenos' pixel-centre convention -- on PC that offset is a shader-side
// concern, and the converted world programs already carry the PC convention.
void D3DDevice_SetRenderState_ViewportEnable(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_HalfPixelOffset(IDirect3DDevice9*, u32) {}
// Primitive reset has no D3D9 equivalent at all; the world draw path re-cuts the
// strip runs itself instead (see WorldDraw_IndexedUP / ExpandStripRunsToList).
// These two exist so the rasteriser applier stays a faithful 1:1 port.
void D3DDevice_SetRenderState_PrimitiveResetEnable(IDirect3DDevice9*, u32) {}
void D3DDevice_SetRenderState_PrimitiveResetIndex(IDirect3DDevice9*, u32) {}

// The Xenos shader GPR split and the ring present-interval fast-set have no PC
// equivalent (the D3D9 runtime owns both); inert by design, not placeholders.
void D3DDevice_SetShaderGPRAllocation(void*, u32, u32, u32) {}
void D3DDevice_SetRenderState_PresentInterval(void*, u32) {}

// ---- the viewport / scissor fast-set ---------------------------------------
// Three reconstructed TUs call these with the same X360 ABI --
// CgsRenderTarget::SetRenderTargetState{,InvertDepth},
// rw::graphics::postfx::RenderTarget's bind, and
// BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap -- and each declares them
// `extern "C" void (void* lpDevice, const void* lpDescriptor)`, so they are defined with
// exactly that signature here. The descriptor pointers are the X360 blocks those bodies
// build on their stacks:
//
//   ViewportF   { f32 X, Y, Width, Height, MinZ, MaxZ; u32 pad }   (0x1C bytes)
//   ScissorRect { s32 Left, Top, Right, Bottom }                   (0x10 bytes)
//
// The Xenos takes the viewport rectangle in FLOATS (it feeds the vertex pipe's viewport
// transform directly); PC Direct3D 9's D3DVIEWPORT9 takes DWORDs for X/Y/Width/Height and
// floats only for the depth range. That conversion is the whole of the leaf. Rounding is
// truncation-with-clamp: every caller passes exact integral pixel values, and a negative
// origin (which D3D9 rejects outright) can only come from a malformed target.
void D3DDevice_SetViewportF(void* /*lpDeviceArg*/, const void* lpViewport)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpViewport == nullptr)
        return;

    const f32* const lpfViewport = static_cast<const f32*>(lpViewport);
    const f32 lfX      = lpfViewport[0];
    const f32 lfY      = lpfViewport[1];
    const f32 lfWidth  = lpfViewport[2];
    const f32 lfHeight = lpfViewport[3];

    D3DVIEWPORT9 lViewport;
    lViewport.X      = static_cast<DWORD>(lfX      > 0.0f ? lfX      : 0.0f);
    lViewport.Y      = static_cast<DWORD>(lfY      > 0.0f ? lfY      : 0.0f);
    lViewport.Width  = static_cast<DWORD>(lfWidth  > 0.0f ? lfWidth  : 0.0f);
    lViewport.Height = static_cast<DWORD>(lfHeight > 0.0f ? lfHeight : 0.0f);
    lViewport.MinZ   = lpfViewport[4];
    lViewport.MaxZ   = lpfViewport[5];
    lpDevice->SetViewport(&lViewport);
}

void D3DDevice_SetScissorRect(void* /*lpDeviceArg*/, const void* lpRect)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpRect == nullptr)
        return;

    const s32* const lpiRect = static_cast<const s32*>(lpRect);
    RECT lScissor;
    lScissor.left   = static_cast<LONG>(lpiRect[0]);
    lScissor.top    = static_cast<LONG>(lpiRect[1]);
    lScissor.right  = static_cast<LONG>(lpiRect[2]);
    lScissor.bottom = static_cast<LONG>(lpiRect[3]);
    lpDevice->SetScissorRect(&lScissor);
}

} // extern "C"

// =============================================================================
// FLAG PC-platform leaf: the scene render-target bracket the shadow pass needs.
//
// On the console the frame's scene target is (re)bound by
// BrnRendererModule::BeginRenderAntiAliased @ Render:725 -- AFTER the shadow-map and
// env-map passes have each bound their own off-screen targets. This build has no
// BeginRenderAntiAliased: renderengine::Device::FrameBegin has already bound the D3D9
// IMPLICIT back buffer before Render's first instruction, and nothing rebinds it later.
// So a pass that binds another surface underneath it has to put it back, or every
// subsequent pass (world, sky, GUI, the 2D tail, the present) draws into that surface.
//
// This is that bracket: it saves and restores the bound colour surface, depth-stencil surface,
// viewport and scissor -- and, separately, parks the shadow map off sampler 15 (see the
// READ-WHILE-WRITTEN note in Save). The SURFACE half is a stand-in for BeginRenderAntiAliased; the
// SAMPLER half is not, and outlives it.
//
// BeginRenderAntiAliased exists as of 2026-08-13, but the deletion gate is NOT the function's
// existence: its definition is compiled out behind BRN_ANTIALIAS_BRACKET_AVAILABLE (six symbols it
// calls are undefined in the linked object set), the render-target pool must actually hold slots 0
// and 4, and Render must gate the call on EnsurePostFxSceneTargets(). The full four-part condition
// is on the declarations in ShadowPassPCLeaf.h. Deleting this today would swap a working bracket
// for a null deref and re-open the measured bug recorded at the gpLastRenderTargetState line below.
//
// ONE THING TO CARRY ACROSS WHEN IT DOES GO, stated with its evidence separated:
//   FACT -- BeginRenderAntiAliased sets no viewport and no scissor. It issues no
//   D3DDevice_SetViewportF and no D3DDevice_SetScissorRect anywhere in 0x823FFA18-0x823FFBD8, and
//   the bind it does perform (renderengine::Device::SetState, the PC body of which is
//   PostFxRenderTargetPCLeaf.cpp:465-485) touches only SetRenderTarget / SetDepthStencilSurface.
//   So a PC scene pass opened by that function alone inherits the previous pass's viewport --
//   i.e. whatever this bracket currently restores.
//   INTERPRETATION, from the documented Xenon tiling model and NOT recovered from this image
//   (D3DDevice_BeginTiling @0x82947BC0 has no body in the export) -- that on the console the
//   viewport and scissor came from the tile rectangles handed to D3DDevice_BeginTiling. Whoever
//   writes the PC leaf must settle the viewport on PC evidence rather than implement this half as
//   though it were recovered.
// =============================================================================
namespace renderengine
{
namespace
{
    IDirect3DSurface9* spSavedColourSurface = nullptr;
    IDirect3DSurface9* spSavedDepthSurface  = nullptr;
    D3DVIEWPORT9       sSavedViewport       = {};
    RECT               sSavedScissor        = {};
    BOOL               sbSavedScissorEnable = FALSE;
    bool               sbSurfacesSaved      = false;

    // The shadow map's own texture, unbound from sampler 15 for the duration of the pass.
    // See the READ-WHILE-WRITTEN note in PCSurfaceBracket_Save.
    IDirect3DBaseTexture9* spSavedShadowSampler = nullptr;

    const u32 KU_SHADOW_SAMPLER_UNIT = 15u;
}

void PCSurfaceBracket_Save()
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || sbSurfacesSaved)
        return;

    // GetRenderTarget / GetDepthStencilSurface both AddRef; the restore releases.
    if (FAILED(lpDevice->GetRenderTarget(0, &spSavedColourSurface)))
        spSavedColourSurface = nullptr;
    if (FAILED(lpDevice->GetDepthStencilSurface(&spSavedDepthSurface)))
        spSavedDepthSurface = nullptr;   // a device with no depth buffer bound is legal
    lpDevice->GetViewport(&sSavedViewport);
    lpDevice->GetScissorRect(&sSavedScissor);
    {
        DWORD luScissorEnable = FALSE;
        lpDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &luScissorEnable);
        sbSavedScissorEnable = static_cast<BOOL>(luScissorEnable);
    }

    // FLAG PC-platform leaf: READ-WHILE-WRITTEN. BrnRendererModule::Render binds the shadow
    // map's depth texture to sampler 15 (X360 Render:536-542) BEFORE this pass runs, and this
    // pass then binds the very same surface as the depth-stencil target. On the console those
    // are two different objects -- the pass writes tiled EDRAM and the sampler reads the
    // RESOLVED copy -- so the console can leave the bind in place. On PC there is no resolve:
    // the texture IS the surface, and Direct3D 9 leaves a surface that is simultaneously a
    // sampler source and the depth-stencil target UNDEFINED (drivers variously ignore the
    // writes, ignore the reads, or decompress). So unit 15 is parked for the length of the
    // pass and put back on the way out; the pointer restored is the same one shadow::Device's
    // resource cache still believes is bound, so the cache stays truthful.
    // DELETE with the bracket.
    spSavedShadowSampler = nullptr;
    if (SUCCEEDED(lpDevice->GetTexture(KU_SHADOW_SAMPLER_UNIT, &spSavedShadowSampler))
        && spSavedShadowSampler != nullptr)
    {
        lpDevice->SetTexture(KU_SHADOW_SAMPLER_UNIT, nullptr);
    }

    // The bracket already delimits exactly the shadow-map pass, so it is also what marks the
    // caster draws for the env-gated cull/depth-bias experiment in WorldDraw_IndexedUP.
    sbShadowPassActive = true;

    // ...and it is what keeps alpha-to-coverage OUT OF THE SHADOW MAP. The cascades render
    // the same alpha-tested foliage into a single-sampled depth atlas, and NO vendor document
    // says what its hook does on a single-sampled target (DXVK makes NVIDIA's inert; the AMD
    // side is undetermined and the practitioner reading is a visible dither, i.e. an artefact
    // and not a no-op). Reconciling on both edges of the bracket makes it true by construction
    // instead of by appeal. The console's request (sbA2cRequested) is untouched, so
    // PCSurfaceBracket_Restore puts it straight back.
    AlphaCoverage_Reconcile();

    sbSurfacesSaved = true;
}

void PCSurfaceBracket_Restore()
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (!sbSurfacesSaved)
        return;
    sbSurfacesSaved    = false;
    sbShadowPassActive = false;

    if (lpDevice != nullptr)
    {
        if (spSavedColourSurface != nullptr)
            lpDevice->SetRenderTarget(0, spSavedColourSurface);
        lpDevice->SetDepthStencilSurface(spSavedDepthSurface);   // null is a valid unbind
        lpDevice->SetViewport(&sSavedViewport);
        lpDevice->SetScissorRect(&sSavedScissor);
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, sbSavedScissorEnable);

        // Put the shadow map back on sampler 15 (see the READ-WHILE-WRITTEN note in Save) and
        // re-apply its sampler state, since the unit was cleared underneath it.
        if (spSavedShadowSampler != nullptr)
        {
            lpDevice->SetTexture(KU_SHADOW_SAMPLER_UNIT, spSavedShadowSampler);
            ShadowSampler_ApplyState(KU_SHADOW_SAMPLER_UNIT);
        }
    }

    if (spSavedShadowSampler != nullptr)
    {
        spSavedShadowSampler->Release();
        spSavedShadowSampler = nullptr;
    }

    // ⚠ THE INVALIDATION THAT MAKES THE BRACKET SAFE (added 2026-08-12, shadow-fetch wave).
    // The two surface binds above go STRAIGHT to D3D9 -- they are a raw restore, not a
    // Device::SetState -- so the engine's "last state installed" shadow (X360 dword_83010A30)
    // does not see them and would keep claiming the shadow-map state is still on the device.
    // The next CgsRenderTarget::SetRenderTargetState / RenderTarget::Begin would then compare
    // equal, SKIP its bind, and draw into whatever this function just restored. Measured
    // consequence before the fix: only the FIRST shadow frame reached the shadow map; every
    // later frame rendered its three cascades into the back buffer's colour and depth.
    // The console needs no such invalidation because its counterpart of this restore is
    // BeginRenderAntiAliased, which rebinds THROUGH Device::SetState and keeps the shadow in
    // step. DELETE this line together with the bracket -- gated on the definitions, the pool and
    // Render's call (the four-part condition on the declarations in ShadowPassPCLeaf.h), NOT on
    // BeginRenderAntiAliased merely existing, which it does as of 2026-08-13. Its rebind is a
    // compare-and-SKIP against this very shadow, so a stale "the shadow-map state is installed"
    // value differs from the anti-alias state and the bind still happens -- which is what makes the
    // invalidation redundant once the call is genuinely on the frame path, rather than load-bearing
    // in the other direction.
    gpLastRenderTargetState = nullptr;

    // The scene target is bound again and sbShadowPassActive is back to false, so put
    // alpha-to-coverage back where the last material's word[10] left it. AFTER the rebind,
    // because the reconciler asks the bound colour target for its sample count.
    AlphaCoverage_Reconcile();

    if (spSavedColourSurface != nullptr) { spSavedColourSurface->Release(); spSavedColourSurface = nullptr; }
    if (spSavedDepthSurface  != nullptr) { spSavedDepthSurface->Release();  spSavedDepthSurface  = nullptr; }
}

// =============================================================================
// FLAG PC bring-up: the SCENE-TARGET PRESENT BLIT's device state.
//
// See the banner on PCSceneBlit_Begin/_End in ShadowPassPCLeaf.h for WHAT this is and what
// retires it (BrnPostFx::Render @0x8240A468 -- the real composite, a later wave). Short form:
// once BrnRendererModule.cpp's BRN_ANTIALIAS_BRACKET_AVAILABLE is 1 the world renders
// off-screen, and one full-screen textured quad through the existing Im2d path puts it back on
// the back buffer before the 2D/GUI tail. These two set and unset the device state that quad
// needs and the Im2d API cannot express.
//
// NOTHING HERE IS RECOVERED FROM THE X360 IMAGE. There is no console counterpart: the Xenos
// resolves EDRAM into a texture and BrnPostFx samples it, so no equivalent state block exists to
// decompile. Every value below is a D3D9 platform decision and is justified against the host code
// it has to survive, never against an address.
// =============================================================================
namespace
{
    // What PCSceneBlit_Begin overwrote, so PCSceneBlit_End can put it back exactly. The RENDER
    // TARGET is deliberately NOT in here: leaving the back buffer bound is the entire point.
    DWORD suSavedAlphaBlendEnable = FALSE;
    DWORD suSavedAlphaTestEnable  = FALSE;
    DWORD suSavedZEnable          = 0;
    DWORD suSavedZWriteEnable     = 0;
    DWORD suSavedCullMode         = 0;
    DWORD suSavedScissorEnable    = FALSE;
    DWORD suSavedAddressU         = 0;
    DWORD suSavedAddressV         = 0;
    DWORD suSavedMinFilter        = 0;
    DWORD suSavedMagFilter        = 0;
    DWORD suSavedMipFilter        = 0;
    DWORD suSavedSrgbTexture      = 0;
    DWORD suSavedColourOp         = 0;
    DWORD suSavedColourArg1       = 0;
    DWORD suSavedAlphaOp          = 0;
    DWORD suSavedAlphaArg1        = 0;
    bool  sbSceneBlitStateSaved   = false;

    // THE SCENE'S DEPTH SURFACE, held across the blit so PCSceneBlit_End can put it back. The
    // full reasoning is on the capture in PCSceneBlit_Begin; the short version is that leaving
    // the device with NO depth-stencil surface past the end of the blit makes the NEXT frame's
    // renderengine::Device::FrameBegin Clear fail as a whole and clear nothing, COLOUR INCLUDED.
    // The flag is separate from sbSceneBlitStateSaved on purpose: it makes the rebind and the
    // Release run on every path out of _End, including the ones that skip the state restore.
    IDirect3DSurface9* spSavedSceneDepthSurface = nullptr;
    bool               sbSceneDepthSaved        = false;

    const u32 KU_BLIT_SAMPLER_UNIT = 0u;   // the Im2d path's only texture stage (CgsIm2d.cpp)
}

void PCSceneBlit_Begin()
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || sbSceneBlitStateSaved)
        return;

    // ---- (1) PUT THE BACK BUFFER BACK. --------------------------------------------------
    // With the bracket live, BeginRenderAntiAliased has bound the anti-alias buffer's surfaces
    // and NOTHING has unbound them -- that is EndRenderAntiAliased @0x82408B00, which is not
    // reconstructed yet. So this does it, and hands the frame back to the 2D tail on the swap
    // chain exactly as it found it before the bracket existed.
    IDirect3DSurface9* lpBackBuffer = nullptr;
    if (SUCCEEDED(lpDevice->GetBackBuffer(0u, 0u, D3DBACKBUFFER_TYPE_MONO, &lpBackBuffer))
        && lpBackBuffer != nullptr)
    {
        lpDevice->SetRenderTarget(0, lpBackBuffer);   // also resets the viewport to the full surface
        lpBackBuffer->Release();                      // GetBackBuffer AddRefs
    }
    // No depth surface: the blit is a screen-space quad with Z off, and the depth surface still
    // bound is the SCENE's, not the swap chain's. Everything after this point in the frame -- the
    // loading-screen layers, the GUI/Apt flush, the movie quad, the debug HUD, the thread monitors
    // -- runs with D3DRS_ZENABLE FALSE (ImRenderer<V>::BeginRendering, CgsIm2d.cpp:140), so none
    // of them reads or writes depth. The next frame's world pass binds its own depth through
    // BeginRenderAntiAliased's renderengine::Device::SetState.
    //
    // ⚠ BUT IT IS CAPTURED FIRST, AND PCSceneBlit_End PUTS IT BACK. Leaving the device with no
    // depth-stencil surface past the end of the blit breaks the NEXT frame outright, and not
    // only its depth: renderengine::Device::FrameBegin clears with D3DCLEAR_TARGET |
    // D3DCLEAR_ZBUFFER (device.cpp:117) and IDirect3DDevice9::Clear is ALL-OR-NOTHING -- with no
    // depth surface bound the whole call fails D3DERR_INVALIDCALL and the COLOUR is not cleared
    // either. Because D3DSWAPEFFECT_COPY preserves the back buffer (device.cpp:89), any frame
    // that then skips the world+blit composites its 2D tail over the LAST PRESENTED FRAME
    // instead of over black. That regression arrives with this bracket, so the bracket carries
    // the fix.
    //
    // WHAT GOES BACK is whatever was bound when the blit opened -- the SCENE target's own depth
    // surface, which BeginRenderAntiAliased bound. That is legal for the back buffer now on
    // colour slot 0: both are the swap chain's extent (BrnRendererMemory sizes the scene target
    // from renderengine::gDisplayWidth/gDisplayHeight, the same pair device.cpp hands
    // D3DPRESENT_PARAMETERS::BackBufferWidth/Height) and neither is multisampled, which is
    // exactly what D3D9 requires of a depth surface. The only consequence is that FrameBegin's
    // Clear now also clears the scene depth -- which the resolve clears every frame anyway, and
    // which nothing in the 2D tail reads (it all runs with D3DRS_ZENABLE FALSE).
    IDirect3DSurface9* lpSceneDepth = nullptr;
    if (FAILED(lpDevice->GetDepthStencilSurface(&lpSceneDepth)))   // AddRefs on success
        lpSceneDepth = nullptr;                                    // D3DERR_NOTFOUND == none bound
    if (spSavedSceneDepthSurface != nullptr)
    {
        // Unreachable while Begin/End alternate (the sbSceneBlitStateSaved gate at the top of
        // this function enforces that). Released rather than overwritten so that even a future
        // caller that breaks the pairing cannot leak the reference.
        spSavedSceneDepthSurface->Release();
    }
    spSavedSceneDepthSurface = lpSceneDepth;
    sbSceneDepthSaved        = true;
    lpDevice->SetDepthStencilSurface(nullptr);

    // The two binds above go STRAIGHT to D3D9, so the engine's "last render-target state
    // installed" cache (X360 dword_83010A30) did not see them and would keep claiming the scene
    // state is on the device -- and the next SetRenderTargetState / RenderTarget::Begin would
    // compare equal and SKIP its bind. That exact miss is what put the shadow cascades in the back
    // buffer (see the note in PCSurfaceBracket_Restore above). Invalidate, same as there.
    gpLastRenderTargetState = nullptr;

    // ---- (2) SAVE WHAT WE ARE ABOUT TO OVERWRITE. ---------------------------------------
    lpDevice->GetRenderState(D3DRS_ALPHABLENDENABLE,  &suSavedAlphaBlendEnable);
    lpDevice->GetRenderState(D3DRS_ALPHATESTENABLE,   &suSavedAlphaTestEnable);
    lpDevice->GetRenderState(D3DRS_ZENABLE,           &suSavedZEnable);
    lpDevice->GetRenderState(D3DRS_ZWRITEENABLE,      &suSavedZWriteEnable);
    lpDevice->GetRenderState(D3DRS_CULLMODE,          &suSavedCullMode);
    lpDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &suSavedScissorEnable);
    lpDevice->GetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_ADDRESSU,    &suSavedAddressU);
    lpDevice->GetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_ADDRESSV,    &suSavedAddressV);
    lpDevice->GetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MINFILTER,   &suSavedMinFilter);
    lpDevice->GetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MAGFILTER,   &suSavedMagFilter);
    lpDevice->GetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MIPFILTER,   &suSavedMipFilter);
    lpDevice->GetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_SRGBTEXTURE, &suSavedSrgbTexture);
    lpDevice->GetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_COLOROP,   &suSavedColourOp);
    lpDevice->GetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_COLORARG1, &suSavedColourArg1);
    lpDevice->GetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_ALPHAOP,   &suSavedAlphaOp);
    lpDevice->GetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_ALPHAARG1, &suSavedAlphaArg1);
    sbSceneBlitStateSaved = true;

    // ---- (3) THE SAMPLER. ----------------------------------------------------------------
    // CLAMP is REQUIRED, not hygiene: the half-texel-corrected UVs run to 1.0 + 0.5/width, and
    // D3D9's default is WRAP, which would fetch column 0 at the right edge and row 0 at the
    // bottom. LINEAR matches what the console's post-fx chain samples with; at the 1:1 sizing this
    // blit runs at (scene target == swap chain extent, both from renderengine::gDisplayWidth /
    // gDisplayHeight -- see BrnRendererMemory::PCBringUpCreatePostFxSceneTargets) a CORRECT
    // half-texel offset makes LINEAR land exactly on texel centres, i.e. it degenerates to a
    // point fetch. THAT IS THE TRIPWIRE: if the whole screen looks slightly soft, the offset is
    // wrong, not the filter. Point-sampling here would HIDE that error rather than fix it, which
    // is why the filter stays LINEAR.
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_ADDRESSU,    D3DTADDRESS_CLAMP);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_ADDRESSV,    D3DTADDRESS_CLAMP);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MINFILTER,   D3DTEXF_LINEAR);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MAGFILTER,   D3DTEXF_LINEAR);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MIPFILTER,   D3DTEXF_NONE);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_SRGBTEXTURE, FALSE);

    // ---- (4) OPAQUE, TEXTURE ONLY. -------------------------------------------------------
    // ⚠ THE ALPHA IS THE TRAP. ImRenderer<V>::BeginRendering turns blending ON with
    // SRCALPHA/INVSRCALPHA (CgsIm2d.cpp:142-144) and ImRendererBase::SetTexture sets ALPHAOP to
    // MODULATE (CgsIm2d.cpp:87-89), so the quad's coverage would be the SCENE TARGET's alpha
    // channel -- which BeginRenderAntiAliased clears to KF_BACKGROUND_COLOUR_ALPHA == 0.0f
    // (X360 flt_82001CC0, `lfs` @0x823FFA5C / @0x823FFA9C). A blit under that state draws
    // nothing at all, and the symptom is a black screen indistinguishable from "the bracket did
    // not work". SELECTARG1 on both ops takes the scene colour and ignores both the vertex
    // colour and the scene alpha; blending off makes the copy a copy.
    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,  FALSE);
    lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE,   FALSE);
    lpDevice->SetRenderState(D3DRS_ZENABLE,           D3DZB_FALSE);
    lpDevice->SetRenderState(D3DRS_ZWRITEENABLE,      FALSE);
    lpDevice->SetRenderState(D3DRS_CULLMODE,          D3DCULL_NONE);
    lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    // The alpha test just went off and the back buffer (single-sampled) is bound, so
    // alpha-to-coverage has to follow both. Derived, not cleared -- see the note in
    // SetWorldPassDefaultStates for why clearing the console's request would strand it.
    AlphaCoverage_Reconcile();
}

// Put back exactly what Begin overwrote -- and ONLY that. The render target is left on the back
// buffer deliberately: handing the frame back on the swap chain is the whole purpose.
void PCSceneBlit_End()
{
    IDirect3DDevice9* const lpDevice = Dev();

    // ---- (0) PUT THE DEPTH SURFACE BACK, AHEAD OF EVERY EARLY RETURN. ---------------------
    // See the capture in PCSceneBlit_Begin for why this is not optional: without it the next
    // frame's renderengine::Device::FrameBegin Clear fails as a whole and clears neither depth
    // NOR colour. It sits before the state guard, and on its own flag, so that no path out of
    // this function -- a null device, a state block that was never saved, a second call -- can
    // either skip the rebind or leak the reference GetDepthStencilSurface took.
    if (sbSceneDepthSaved)
    {
        sbSceneDepthSaved = false;
        if (lpDevice != nullptr)
            lpDevice->SetDepthStencilSurface(spSavedSceneDepthSurface);   // null is a valid unbind
        if (spSavedSceneDepthSurface != nullptr)
        {
            spSavedSceneDepthSurface->Release();   // balances _Begin's GetDepthStencilSurface
            spSavedSceneDepthSurface = nullptr;
        }
    }

    if (lpDevice == nullptr || !sbSceneBlitStateSaved)
        return;
    sbSceneBlitStateSaved = false;

    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,  suSavedAlphaBlendEnable);
    lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE,   suSavedAlphaTestEnable);
    lpDevice->SetRenderState(D3DRS_ZENABLE,           suSavedZEnable);
    lpDevice->SetRenderState(D3DRS_ZWRITEENABLE,      suSavedZWriteEnable);
    lpDevice->SetRenderState(D3DRS_CULLMODE,          suSavedCullMode);
    lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, suSavedScissorEnable);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_ADDRESSU,    suSavedAddressU);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_ADDRESSV,    suSavedAddressV);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MINFILTER,   suSavedMinFilter);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MAGFILTER,   suSavedMagFilter);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_MIPFILTER,   suSavedMipFilter);
    lpDevice->SetSamplerState(KU_BLIT_SAMPLER_UNIT, D3DSAMP_SRGBTEXTURE, suSavedSrgbTexture);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_COLOROP,   suSavedColourOp);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_COLORARG1, suSavedColourArg1);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_ALPHAOP,   suSavedAlphaOp);
    lpDevice->SetTextureStageState(KU_BLIT_SAMPLER_UNIT, D3DTSS_ALPHAARG1, suSavedAlphaArg1);
    AlphaCoverage_Reconcile();   // the alpha test was just put back; follow it

    // The texture stays bound at unit 0 only until the next Im2d SetTexture, which every
    // subsequent 2D layer issues for itself.
}

// =============================================================================
// FLAG PC-platform leaf: the SHADOW-MAP sampler state (see the SAMPLE SEMANTICS SEAM
// banner in ShadowPassPCLeaf.h).
//
// The console binds sampler 15 through sub_8227D158 -- the shadow cache's TEXTURE STATE
// path -- which installs a renderengine::TextureState carrying the filter, the address
// modes and the comparison, all built once in BrnRendererModule::Construct @0x8240A778.
// This build has no TextureState objects (they need the render-target pool), so
// BrnRendererModule binds the texture through shadow::Device::SetResource, which sets the
// RESOURCE only. Unit 15 therefore keeps whatever it last held -- and nothing else in this
// build ever touches unit 15, so that is the raw D3D9 default: POINT filter, WRAP address.
// On a hardware-comparison depth texture POINT defeats the 2x2 PCF the compare exists to
// give, and WRAP makes a cascade's edge sample bleed round to the opposite edge of the
// 1x3 atlas. So the state is installed explicitly here, matched to the format that was
// actually created:
//
//   HW-compare (D24X8 / D16)  LINEAR min+mag -- on these formats the LINEAR filter IS the
//                             PCF: the hardware compares four texels and returns the
//                             filtered 0..1 fraction, which is what the shipped shaders'
//                             `mul r0.w, r0.w, rN.x` wants.
//   RAW (INTZ / DF24 / DF16)  POINT -- filtering raw depth VALUES averages depths, which is
//                             meaningless; a raw-depth path has to point-sample.
//
// CLAMP on both axes in either case (the atlas is a 1x3 vertical strip; a wrapped V would
// read a different cascade). No mip chain, no sRGB.
// DELETE when Construct's TextureState pair lands and the console's own bind path is live.
// =============================================================================
void ShadowSampler_ApplyState(u32 luUnit)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || luUnit >= 16u)
        return;

    const DWORD leFilter = ShadowDepthFormatIsHardwareCompare() ? D3DTEXF_LINEAR : D3DTEXF_POINT;

    lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER,   leFilter);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER,   leFilter);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER,   D3DTEXF_NONE);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU,    D3DTADDRESS_CLAMP);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV,    D3DTADDRESS_CLAMP);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSW,    D3DTADDRESS_CLAMP);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXMIPLEVEL, 0u);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_MAXANISOTROPY, 1u);
    lpDevice->SetSamplerState(luUnit, D3DSAMP_SRGBTEXTURE, FALSE);
}

// =============================================================================
// The RAW-DEPTH sampler seam's two public entry points (see the banner over
// D3DDevice_SetTexture, which is where the work actually happens).
//
// PostFxDepthSampler_ApplyState  -- force one unit to the raw-depth sampler words. Nothing in
//   this build needs to call it (the bind hook does it automatically); it exists so a future
//   caller that binds a depth texture WITHOUT going through D3DDevice_SetTexture has a named
//   way to say the same thing, instead of open-coding nine SetSamplerState calls.
// PostFxDepthSampler_BoundUnitMask -- which units are holding a raw-depth texture right now.
//   renderengine::Device::SetState reads it to unbind them before binding the same texture's
//   surface as the depth-stencil (D3D9 will not do that for you, and the sample and the write
//   cannot both be legal at once).
// =============================================================================
void PostFxDepthSampler_ApplyState(u32 luUnit)
{
    ApplyRawDepthSamplerState(Dev(), luUnit);
}

// PostFxSourceSampler_ApplyState -- the post-fx composite's SOURCE unit (KU_SAMPLER_SOURCE == 0).
// See ApplyPostFxSourceSamplerState's banner above. The only caller is BrnPostFxShader::Render,
// immediately after `shadow::Device::SetState(lpSourceSampler, KU_SAMPLER_SOURCE)`, and it passes
// the min/mag filter word and the MAX ANISOTROPY of the sampler object it just bound -- so all
// three of Render's arms (motion blur / bilinear / point) land on the unit, not just the blur one.
void PostFxSourceSampler_ApplyState(u32 luUnit, u32 luMinMagFilterWord, u32 luMaxAnisotropy)
{
    ApplyPostFxSourceSamplerState(Dev(), luUnit, luMinMagFilterWord, luMaxAnisotropy);
}

u32 PostFxDepthSampler_BoundUnitMask()
{
    // SELF-HEALING (rung-6 verifier): the claim bitmask is maintained by the D3DDevice_SetTexture hook,
    // but two world paths bind through lpDevice->SetTexture DIRECTLY (WorldShader_BindTextureUnit and the
    // material sampler loop), so a unit claimed for raw depth can silently become an ordinary texture
    // and a later RenderTargetState bind would null what the world had bound there. So the mask is
    // RE-VALIDATED against what the runtime actually holds before it is handed out: a claimed unit whose
    // bound resource is no longer a raw-depth texture drops its claim here.
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice != nullptr && guRawDepthSamplerUnits != 0u)
    {
        for (u32 luUnit = 0u; luUnit < 16u; ++luUnit)
        {
            if ((guRawDepthSamplerUnits & (1u << luUnit)) == 0u)
                continue;
            IDirect3DBaseTexture9* lpBound = nullptr;
            if (SUCCEEDED(lpDevice->GetTexture(luUnit, &lpBound)))
            {
                if (!BoundTextureIsRawDepth(lpBound, nullptr))
                    guRawDepthSamplerUnits &= ~(1u << luUnit);
                if (lpBound != nullptr)
                    lpBound->Release();
            }
        }
    }
    return guRawDepthSamplerUnits;
}

// [FLAG PC bring-up probe] see the increment site in WorldDraw_IndexedUP.
u64 WorldDrawCallCount()
{
    return guWorldDrawCalls;
}

// =============================================================================
// [FLAG PC bring-up probe] the SHADOW-PASS OCCLUSION PROBE -- ground truth for
// "is the shadow map actually being WRITTEN?".
//
// Nothing else can answer that question on this backend: a D3DPOOL_DEFAULT
// depth-stencil texture cannot be locked and cannot be StretchRect'd out, so the
// depth bytes are simply not readable from the CPU. What IS readable is how many
// fragments the cascade's draws got through the depth test, and that is exactly the
// question -- a cascade that writes N > 0 pixels has real geometry in its band; a
// cascade that writes 0 while the world pass draws thousands does not, and then the
// bug is in the pass (viewport band, cull mode, the matrices the casters are
// transformed by), not in the sampling.
//
// The queries are read ONE FRAME LATE and NON-BLOCKING. A GetData with
// D3DGETDATA_FLUSH straight after Issue(END) stalls the CPU on the GPU every frame,
// which would distort the very frame it is measuring; deferring the read costs
// nothing and the numbers are still per-frame.
//
// DELETE with the rest of the shadow bring-up probes.
// =============================================================================
namespace
{
    const u32 KU_SHADOW_PROBE_CASCADES = KU_SHADOW_TALLY_SLOTS;   // 3 cascades + the control

    IDirect3DQuery9* sapShadowQuery[KU_SHADOW_PROBE_CASCADES]           = {};
    bool             sabShadowQueryActive[KU_SHADOW_PROBE_CASCADES]     = {};
    bool             sabShadowQueryPending[KU_SHADOW_PROBE_CASCADES]    = {};
    u32              sauShadowQueryPixels[KU_SHADOW_PROBE_CASCADES]     = {};
    bool             sabShadowQueryHaveResult[KU_SHADOW_PROBE_CASCADES] = {};
}

void ShadowProbe_Begin(u32 luCascade)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || luCascade >= KU_SHADOW_PROBE_CASCADES)
        return;

    // Open this slot's clip-space tally (see the ShadowClipTally banner) and make the draw
    // path attribute its samples to it. The scope is closed by ShadowProbe_End, so a draw
    // outside any Begin/End pair is never counted against a cascade.
    std::memset(&saShadowClip[luCascade], 0, sizeof(saShadowClip[luCascade]));
    suShadowClipSlot = luCascade;

    // Drain the previous issue first -- a query that is still in flight cannot be re-issued.
    if (sabShadowQueryPending[luCascade] && sapShadowQuery[luCascade] != nullptr)
    {
        DWORD luPixels = 0;
        if (sapShadowQuery[luCascade]->GetData(&luPixels, sizeof(luPixels), 0) == S_OK)
        {
            sauShadowQueryPixels[luCascade]     = static_cast<u32>(luPixels);
            sabShadowQueryHaveResult[luCascade] = true;
            sabShadowQueryPending[luCascade]    = false;
        }
        else
        {
            return;   // still in flight; skip this frame's measurement rather than stall
        }
    }

    if (sapShadowQuery[luCascade] == nullptr)
    {
        if (FAILED(lpDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION, &sapShadowQuery[luCascade])))
        {
            sapShadowQuery[luCascade] = nullptr;
            LogOnce("shprobe", "[shadow-fetch] occlusion queries unsupported - pixel counts unavailable\n");
            return;
        }
    }

    if (SUCCEEDED(sapShadowQuery[luCascade]->Issue(D3DISSUE_BEGIN)))
        sabShadowQueryActive[luCascade] = true;
}

void ShadowProbe_End(u32 luCascade)
{
    // Close the clip-space attribution window first, unconditionally: a draw issued outside a
    // Begin/End pair must never land in a cascade's tally, even when the query half below
    // bails out.
    suShadowClipSlot = 0xFFFFFFFFu;

    if (luCascade >= KU_SHADOW_PROBE_CASCADES || !sabShadowQueryActive[luCascade]
        || sapShadowQuery[luCascade] == nullptr)
    {
        return;
    }
    sabShadowQueryActive[luCascade] = false;
    if (SUCCEEDED(sapShadowQuery[luCascade]->Issue(D3DISSUE_END)))
        sabShadowQueryPending[luCascade] = true;
}

bool ShadowProbe_LastPixels(u32 luCascade, u32* lpuPixels)
{
    if (luCascade >= KU_SHADOW_PROBE_CASCADES || lpuPixels == nullptr
        || !sabShadowQueryHaveResult[luCascade])
    {
        return false;
    }
    *lpuPixels = sauShadowQueryPixels[luCascade];
    return true;
}

// The clip-space tally + the first-draw device state for one probe slot. Returns false when
// the slot never saw a draw, so the caller can say "not measured" rather than print zeroes.
bool ShadowProbe_ClipTally(u32 luSlot, ShadowClipReport* lpReport)
{
    if (luSlot >= KU_SHADOW_TALLY_SLOTS || lpReport == nullptr)
        return false;

    const ShadowClipTally& lrTally = saShadowClip[luSlot];
    if (!lrTally.mbStateCaptured)
        return false;

    lpReport->muSampled       = lrTally.muSampled;
    lpReport->muInside        = lrTally.muInside;
    lpReport->muOutXY         = lrTally.muOutXY;
    lpReport->muOutZNear      = lrTally.muOutZNear;
    lpReport->muOutZFar       = lrTally.muOutZFar;
    lpReport->muBehindW       = lrTally.muBehindW;
    lpReport->muNoWvp         = lrTally.muNoWvp;
    for (u32 luLane = 0; luLane < 3u; ++luLane)
        lpReport->mafFirstObject[luLane] = lrTally.mafFirstObject[luLane];
    for (u32 luLane = 0; luLane < 4u; ++luLane)
        lpReport->mafFirstClip[luLane] = lrTally.mafFirstClip[luLane];
    lpReport->muVpX           = lrTally.muVpX;
    lpReport->muVpY           = lrTally.muVpY;
    lpReport->muVpW           = lrTally.muVpW;
    lpReport->muVpH           = lrTally.muVpH;
    lpReport->mfVpMinZ        = lrTally.mfVpMinZ;
    lpReport->mfVpMaxZ        = lrTally.mfVpMaxZ;
    lpReport->miScissorL      = lrTally.miScissorL;
    lpReport->miScissorT      = lrTally.miScissorT;
    lpReport->miScissorR      = lrTally.miScissorR;
    lpReport->miScissorB      = lrTally.miScissorB;
    lpReport->muScissorEnable = lrTally.muScissorEnable;
    lpReport->muZEnable       = lrTally.muZEnable;
    lpReport->muZWrite        = lrTally.muZWrite;
    lpReport->muZFunc         = lrTally.muZFunc;
    lpReport->muCull          = lrTally.muCull;
    lpReport->muColourWrite   = lrTally.muColourWrite;
    lpReport->muZFuncEffective = lrTally.muZFuncEffective;
    lpReport->muCullEffective  = lrTally.muCullEffective;
    lpReport->mfHalfWidthMetres  = lrTally.mfHalfWidthMetres;
    lpReport->mfHalfHeightMetres = lrTally.mfHalfHeightMetres;
    lpReport->mfDepthSpanMetres  = lrTally.mfDepthSpanMetres;
    lpReport->muTrisSampled      = lrTally.muTrisSampled;
    lpReport->muTrisSubPixel     = lrTally.muTrisSubPixel;
    lpReport->mfMaxTriPixelArea  = lrTally.mfMaxTriPixelArea;
    for (u32 luAxis = 0; luAxis < 3u; ++luAxis)
    {
        lpReport->mafClipMin[luAxis] = lrTally.mafClipMin[luAxis];
        lpReport->mafClipMax[luAxis] = lrTally.mafClipMax[luAxis];
    }
    return true;
}

// Is anything actually bound at sampler unit luUnit on the device right now? The bind goes
// through shadow::Device's cache, so "we called SetResource" is not the same as "the runtime
// holds a texture there" -- this asks the runtime.
bool ShadowProbe_TextureBound(u32 luUnit)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || luUnit >= 16u)
        return false;

    IDirect3DBaseTexture9* lpTexture = nullptr;
    if (FAILED(lpDevice->GetTexture(luUnit, &lpTexture)) || lpTexture == nullptr)
        return false;
    lpTexture->Release();
    return true;
}
}

// =============================================================================
// FLAG PC-platform leaf: THE PREDICATED-TILING / EDRAM-RESOLVE FOUR.
//
// renderengine::D3DDevice_BeginTiling / _SetPredication / _Resolve / _EndTiling -- the four
// Xenos entry points BrnRendererModule::BeginRenderAntiAliased @0x823FFA18 and
// BrnRendererModule::ResolveMSAA @0x823FFBE0 call. They were declared in
// pc/gcm/renderengine/Xbox2SurfaceShims.h (with the ABI decoded from the X360 asm) and defined
// NOWHERE; they are four of the six symbols that keep BRN_ANTIALIAS_BRACKET_AVAILABLE at 0.
// The signatures below are that header's, UNCHANGED -- nothing here re-derives them.
//
// -------------------------------------------------------------------------------------------
// WHAT IS CONSOLE-FAITHFUL (recovered, with the address it is recovered from)
// -------------------------------------------------------------------------------------------
//  * THE CLEAR EXISTS AND ITS VALUES ARE THE CALLER'S. On the Xenos the tiling bracket is what
//    clears the surface: the colour/Z/stencil arguments are handed to D3DDevice_BeginTiling
//    (`addi r7, r1, var_40 # pClearColor` @0x823FFB48, `lfs f1, flt_82001C98 # ClearZ`
//    @0x823FFB54, `clrlwi r9, r27, 24` @0x823FFB44) and to the 0x300 D3DDevice_Resolve
//    (`addi r10, r1, var_70 # pClearColor` @0x823FFCF8 / @0x823FFDE8), and to no other call in
//    either function. A leaf that drops them drops the frame's clear. That is why these two are
//    NOT empty.
//  * WHICH CALL CARRIES WHICH. Flags is an immediate at every site: 0x14 (`li r4, 0x14`
//    @0x823FFCDC and @0x823FFDCC) with a NULL clear colour (`li r10, 0` @0x823FFCC0 /
//    @0x823FFDA4), and 0x300 (`li r4, 0x300` @0x823FFD18 and @0x823FFE08) with a live one. The
//    code below keys off those two VALUES, exactly as the BRN_ANTIALIAS_BRACKET_AVAILABLE banner
//    in BrnRendererModule.cpp instructs.
//  * THE ORDER INSIDE A RESOLVE IS COPY-THEN-CLEAR. The console clears the EDRAM surface *behind*
//    the copy that is moving its contents out; clearing first would resolve a cleared surface.
//  * THE RECTANGLES ARE THE CALLER'S. They are honoured as passed (clamped to the bound surface),
//    never widened.
//
// -------------------------------------------------------------------------------------------
// WHAT IS A PC BRING-UP CHOICE (nothing below is claimed to be recovered)
// -------------------------------------------------------------------------------------------
//  * THE COPY. On the Xenos, Resolve moves an EDRAM surface out to a sampleable texture -- EDRAM
//    is not addressable by the texture units, so the copy is the only way to see the pixels. On
//    PC there is no EDRAM: the scene render target already IS a texture. But the DESTINATION is
//    still a DIFFERENT object here (the anti-alias buffer, pool slot 0, resolves into the
//    down-sample buffer, pool slot 4 -- two independently created CgsRenderTargets), so the copy
//    is not vacuous, it is just an ordinary surface-to-surface blit. It is implemented as
//    IDirect3DDevice9::StretchRect. It is NOT a no-op and NOT dressed up as one.
//    THE DEPTH HALF OF IT IS a no-op, honestly: D3D9 StretchRect refuses depth-stencil surfaces,
//    and there is no other PC route from a bound depth surface into a texture. So the 0x14
//    depth/stencil resolve does nothing here and says so (LogOnce). Nothing in the tree samples
//    the down-sample buffer's depth today; BrnPostFx's depth of field is the wave that will need
//    it, and it will need a shader-side answer, not a copy.
//  * THE SCENE SUSPEND AROUND THE COPY. The whole of BrnRendererModule::Render runs inside ONE
//    scene -- renderengine::Device::FrameBegin calls BeginScene (device.cpp:117) and
//    ShowPixelBuffer calls EndScene (device.cpp:206) -- so the copy above is issued between them.
//    The D3D9 reference carries the sentence "StretchRect cannot be called inside of a
//    BeginScene/EndScene pair", and BE PRECISE ABOUT ITS SCOPE, because the correct decision here
//    turns on it: in the current reference text that sentence is the last bullet of the sub-list
//    headed "Additional Restrictions for Depth and Stencil Surfaces", NOT of the general
//    "StretchRect Restrictions" list above it. So for a COLOUR render-target copy the restriction
//    is not squarely documented, and claiming it is would be a fabricated citation.
//    IT IS SUSPENDED ANYWAY, and here is the reasoning rather than an appeal to the doc:
//      (a) the scoping is genuinely ambiguous -- the sentence is unqualified, several renderings
//          of the same page present the restrictions as one flat list, and driver behaviour for
//          StretchRect is explicitly documented to vary;
//      (b) the failure it guards against is UNOBSERVABLE. StretchRect reports failure only in its
//          HRESULT; the destination simply is not written, and the frame that is presented comes
//          from the destination. That is a permanent black screen with no exception, no assert
//          and nothing on screen to read -- the exact silent shape this campaign keeps paying for;
//      (c) the cost of removing the question entirely is ONE EndScene/BeginScene pair per frame
//          (the 0x14 depth resolve does no copy, so only the 0x300 colour resolve suspends).
//    A GPU flush per frame is a real cost and it is a bring-up choice, not a recovered behaviour;
//    when the real composite (BrnPostFx::Render @0x8240A468) lands and the copy moves, this goes
//    with it. If it is ever removed, the thing to verify first is the D3D9 DEBUG runtime's output
//    on a colour StretchRect inside a scene -- that, not the doc page, is the arbiter.
//  * THE COPY AND THE CLEAR ARE COUPLED, which the console did not need to do. On the Xenos the
//    resolve and the clear are one GPU operation: the clear cannot happen without the copy having
//    happened. Two separate D3D9 calls can diverge, and the divergence is catastrophic in exactly
//    one direction -- copy fails, clear succeeds: the source is wiped and the destination is never
//    written, so the screen is cleanly black FOREVER and the anti-alias buffer looks pristine to
//    anyone inspecting it. The clear is therefore withheld when a required copy did not land, so
//    the failure degrades to the world accumulating in an uncleared scene target (a smear, with
//    stale depth) instead of to a clean black frame. Restoring the console's coupling, not adding
//    a behaviour it lacked.
//  * THE COPY-FAILURE TRIPWIRE. Withholding the clear fixes what the SOURCE looks like, but the
//    picture comes from the DESTINATION, and a destination that was never written still reads as
//    black. So after KU_RESOLVE_TRIPWIRE_FRAMES consecutive failed copies the destination is
//    ColorFill'd magenta. This is the one place in this leaf that can put a colour on screen that
//    no console frame contains, and it is deliberate: it fires ONLY on a path whose alternative is
//    an indistinguishable black screen, and it fires only after the failure has proven permanent.
//  * THE CLEAR'S VIEWPORT / SCISSOR BRACKET. D3D9's Clear obeys the viewport and the scissor test,
//    neither of which this leaf sets for rendering; TilingClearBoundSurfaces therefore saves them,
//    forces the test off and the viewport to the bound surface's full extent for the length of the
//    clear, and puts both back. The reasoning, with the two reference sentences it rests on, is at
//    the call site. Platform mechanics: the console's clear rode inside the tiling pass and had no
//    such state to trip over.
//  * THE RENDERING VIEWPORT / SCISSOR, WHICH THIS LEAF STILL DOES NOT SET. The
//    BRN_ANTIALIAS_BRACKET_AVAILABLE banner records, as INTERPRETATION, that on the console the
//    viewport came from the tile rectangles handed to BeginTiling, and tells the leaf author to
//    settle it on PC evidence. THE PC EVIDENCE SETTLES IT: IDirect3DDevice9::SetRenderTarget
//    resets the viewport to the full extent of the new render target, and the bind
//    BeginRenderAntiAliased performs -- renderengine::Device::SetState, PC body at
//    PostFxRenderTargetPCLeaf.cpp:465-486 -- calls exactly that. So by the time BeginTiling would
//    run, the viewport already IS the whole scene target; there is nothing for this leaf to owe
//    the DRAWS that follow. (The clear's own bracket above is a different question: it is not
//    setting a viewport for anything to render with, it is stopping leftover state from eating a
//    clear.) The scissor is not set for rendering for the same reason it is saved/restored rather
//    than set in PCSurfaceBracket_*: nothing in this bracket sets one on the console either.
//  * THE D3DCLEAR_STENCIL DECISION, WHICH IS LOAD-BEARING AND EASY TO GET WRONG. D3D9's Clear is
//    ALL-OR-NOTHING: ask for D3DCLEAR_STENCIL on a depth surface with no stencil bits and the
//    call fails and clears NOTHING -- including the depth. The scene target's depth surface is
//    picked by a format ladder whose FIRST and most likely candidate is D3DFMT_D24X8, which has
//    NO STENCIL (PostFxRenderTargetPCLeaf.cpp:230-237). So the stencil bit is added only when the
//    bound depth surface's format actually carries stencil, and a failed Clear is retried without
//    it and then logged. Getting this wrong would leave depth uncleared every frame with no error
//    anywhere -- the exact silent-failure shape this project keeps paying for.
//  * THE ORDERING TRIPWIRE. The clear lands on whatever is BOUND, because that is what the
//    console's clear lands on. If ResolveMSAA is ever called after the scene has been handed back
//    to the swap chain (PCSceneBlit_Begin rebinds the back buffer and deliberately leaves it
//    bound), the clear would wipe the finished frame. This leaf REFUSES to clear the swap chain
//    and logs the fix instead. It cannot fire when the calls are in console order.
//  * THE RETURN VALUES. D3DDevice_Resolve and D3DDevice_EndTiling keep the `int` the committed
//    header declares; no attested caller reads it (ResolveMSAA tail-branches straight after its
//    EndTiling call; PixelBuffer::Xbox2ResolveTo discards both and returns its own constant 1),
//    so 0 is a bring-up choice for "no error", not a recovered value.
//
// -------------------------------------------------------------------------------------------
// WHICH OF THE FOUR ACTUALLY RUNS ON THIS BUILD  [RESTATED FOR RUNG 8 -- MSAA IS ON]
// -------------------------------------------------------------------------------------------
// UNTIL RUNG 8 the PC pool created the anti-alias buffer with MSAA explicitly off
// (`CreateAntiAliasBuffer(lpAllocator, false)`), so both bracket bodies took their UNTILED branch
// -- BeginTiling / EndTiling / SetPredication were never called and only the two Resolves ran.
// THAT IS NO LONGER TRUE. The anti-alias wave flips the bring-up to the console's own choice
// (MSAA on, KMSAA_TILING_PLAN, multisample format 1 == 2 samples), which puts
// BrnRendererModule::mbMultisampledBackbuffer on the TILED branch. On this build now:
//     D3DDevice_BeginTiling     runs once a frame -- it is the frame-TOP clear over the plan's
//                               TWO tile rectangles, which partition 1280x720 exactly
//                               ({0,0,1280,384} + {0,384,1280,720}, BrnAntiAliasTiling.h), so
//                               there is no uncleared band at y=384. On PC there is ONE pass over
//                               ONE surface, so the viewport must span BOTH rects; it does,
//                               because RenderTarget::Begin sets the full extent immediately
//                               before, and that is now CHECKED rather than assumed (below).
//     D3DDevice_Resolve         runs FOUR times a frame (two tiles x {depth, colour}). The colour
//                               half IS the MSAA resolve -- a same-size, unfiltered StretchRect
//                               from a multisampled source is exactly D3D9's resolve primitive.
//                               The depth half is no longer a no-op: it goes through RESZ.
//     D3DDevice_EndTiling       runs once a frame with every pointer argument null -- still
//                               nothing to do, and still honestly empty.
//     D3DDevice_SetPredication  runs three times a frame and is still an honest no-op (there is
//                               no per-tile command-stream replay on PC to select).
// =============================================================================
namespace renderengine
{
namespace
{
    // --- the two Xenon call-argument blocks ---------------------------------------------------
    // The Xenon D3DRECT these entry points take: FOUR dwords, left / top / right / bottom. It is
    // read as four dwords rather than as any one caller's C++ type because the attested callers
    // already spell it with two different ones:
    //   * BrnRendererModule::BeginRenderAntiAliased / ::ResolveMSAA pass
    //     BrnGraphics::AntiAliasTilingPlan::TileRect (BrnAntiAliasTiling.h -- mu32Left, mu32Top,
    //     mu32Right, mu32Bottom, stride 0x10);
    //   * renderengine::PixelBuffer::Xbox2ResolveTo builds a bare `u32 luSourceRect[6]` and fills
    //     [0] = left, [1] = top, [2] = right, [3] = bottom (PixelBuffer.cpp:169-193).
    // Two independent callers, one layout. File-local for the same reason BrnRendererModule.cpp's
    // XenonPoint is file-local: this is a platform-API call-argument block, not a shared Burnout
    // type. If a third shape ever appears it moves to Xbox2SurfaceShims.h beside the declarations
    // that consume it -- it does not get copied.
    struct XenonResolveRect
    {
        u32 muLeft;     // +0x00
        u32 muTop;      // +0x04
        u32 muRight;    // +0x08
        u32 muBottom;   // +0x0C
    };

    // The Xenon D3DPOINT D3DDevice_Resolve takes as its destination corner: two dwords, x then y
    // (the same block BrnRendererModule.cpp builds as its file-local XenonPoint and fills from
    // maTile[tile].mu32Left / .mu32Top @0x823FFCAC/B0, or zeroes @0x823FFD7C/D84).
    struct XenonResolvePoint
    {
        s32 miX;    // +0x00
        s32 miY;    // +0x04
    };

    // 0x04 -- "the surface being resolved is the DEPTH/STENCIL one". PROVEN IN-IMAGE:
    // renderengine::PixelBuffer::Xbox2ResolveTo @0x82B62300 sets exactly this bit
    // (`ori r28, r28, 4` @0x82B62358) on exactly the branch where the surface kind is
    // 1 == depth-stencil (PixelBuffer.cpp:139-144).
    const u32 KU_RESOLVE_DEPTH_STENCIL_BIT = 0x04u;

    // 0x300 -- the CLEARS. The two bits always travel together: the only two Flags values in the
    // whole image are 0x14 and 0x300, and the 0x300 call is the one that carries a non-null clear
    // colour while the 0x14 call passes null. WHICH of 0x100 / 0x200 is the colour clear and which
    // is the depth/stencil clear is NOT recovered, and nothing here needs it -- the pair is
    // treated as one mask, which is the only form the image ever uses.
    const u32 KU_RESOLVE_CLEAR_BITS = 0x300u;

    // Bounded because the tile count is a caller-supplied argument and this is a leaf. The largest
    // shipped plan has two tiles and the record has room for four (BrnAntiAliasTiling.h).
    const u32 KU_MAX_TILE_RECTS = 8u;

    // ---- DIAGNOSTICS -------------------------------------------------------------------------
    // A LogOnce is the WRONG instrument for these. Every failure reported below is one that, once
    // it starts, repeats every frame forever -- and LogOnce prints it in frame 1 and then never
    // again, so the log reads like a one-off hiccup while the screen is permanently wrong. These
    // report the first occurrence and then every KU_RESOLVE_LOG_INTERVAL-th, carrying the running
    // count, so a permanent failure looks permanent.
    const u32 KU_RESOLVE_LOG_INTERVAL = 300u;      // ~5 s at 60 Hz

    // How many CONSECUTIVE failed colour copies before the destination is painted with the
    // tripwire colour (see THE COPY-FAILURE TRIPWIRE in the banner). One second at 60 Hz: long
    // enough that a transient failure shows as a frozen frame rather than a flash of colour, short
    // enough that a permanent one is on screen immediately.
    const u32 KU_RESOLVE_TRIPWIRE_FRAMES = 60u;

    u32 suResolveCopyFailures      = 0u;
    u32 suResolveCopyFailureStreak = 0u;
    u32 suResolveClearWithheld     = 0u;
    u32 suResolveSceneReopenFails  = 0u;
    u32 suTilingClearFailures      = 0u;

    void LogRepeating(u32& lruCount, const char* lpcMessage)
    {
        ++lruCount;
        if (lruCount == 1u || (lruCount % KU_RESOLVE_LOG_INTERVAL) == 0u)
        {
            char lacMessage[320];
            std::snprintf(lacMessage, sizeof(lacMessage), "%s [occurrence %u]\n",
                          lpcMessage, static_cast<unsigned>(lruCount));
            CgsDev::Log::WriteToLog(lacMessage);
        }
    }


    // Does this depth format carry stencil bits? See the D3DCLEAR_STENCIL note in the banner: a
    // stencil clear requested against a stencil-less surface makes Clear fail ENTIRELY, taking the
    // depth clear down with it. The formats listed are the ones this build can actually end up
    // with, and since 2026-08-15 there are TWO ladders in PostFxRenderTargetPCLeaf.cpp: the
    // COMPARE ladder (the shadow map) tries D24X8, D16, INTZ, DF24, DF16 and the RAW-VALUE ladder
    // (every sampled post-fx target) tries INTZ, DF24, DF16; both fall back to a plain D24S8
    // depth-stencil surface, and the device's own auto depth-stencil is D24S8 (device.cpp:95).
    // NOTHING HERE HAD TO CHANGE FOR THAT SPLIT and that is by design: this answers from the
    // format of the surface actually bound, so a target that moved from D24X8 (no stencil) to
    // INTZ (stencil, it aliases D24S8) or to DF24 (no stencil) is handled without being named.
    bool TilingDepthFormatHasStencil(D3DFORMAT leFormat)
    {
        switch (static_cast<u32>(leFormat))
        {
        case static_cast<u32>(D3DFMT_D24S8):
        case static_cast<u32>(D3DFMT_D24FS8):
        case static_cast<u32>(D3DFMT_D24X4S4):
        case static_cast<u32>(D3DFMT_D15S1):
            return true;
        default:
            break;
        }
        // INTZ and RAWZ are the two FOURCC readable-depth formats that alias a D24S8 layout, so
        // they do carry stencil; DF24 / DF16 (the ATI pair) do not.
        return leFormat == static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'))
            || leFormat == static_cast<D3DFORMAT>(MAKEFOURCC('R', 'A', 'W', 'Z'));
    }

    // The D3DCLEAR_* set the surfaces CURRENTLY BOUND can actually accept (see the banner).
    DWORD TilingClearFlagsForBoundSurfaces(IDirect3DDevice9* lpDevice, bool lbWantColourClear)
    {
        DWORD luFlags = lbWantColourClear ? static_cast<DWORD>(D3DCLEAR_TARGET) : 0u;

        IDirect3DSurface9* lpDepth = nullptr;
        if (SUCCEEDED(lpDevice->GetDepthStencilSurface(&lpDepth)) && lpDepth != nullptr)
        {
            luFlags |= static_cast<DWORD>(D3DCLEAR_ZBUFFER);

            D3DSURFACE_DESC lDesc;
            if (SUCCEEDED(lpDepth->GetDesc(&lDesc)) && TilingDepthFormatHasStencil(lDesc.Format))
            {
                luFlags |= static_cast<DWORD>(D3DCLEAR_STENCIL);
            }
            lpDepth->Release();   // GetDepthStencilSurface AddRefs
        }
        return luFlags;
    }

    // The clear colour the console passes is four floats in x/y/z/w = R/G/B/A order -- the same
    // rw::math::vpu::Vector4 BrnRendererModule::BeginRenderAntiAliased publishes into
    // mvBackgroundColour (0.72 -> .x -> RED, `stfs f0, var_40` @0x823FFA7C; 0.83 -> .y; 0.89 ->
    // .z; alpha -> .w). D3D9's Clear takes a packed D3DCOLOR instead, so the floats are clamped
    // and quantised here. That quantisation is a D3D9 fact, not a loss of console information.
    D3DCOLOR TilingColourFromFloat4(const void* lpClearColour)
    {
        const f32* const lpafChannel = static_cast<const f32*>(lpClearColour);
        u32 lauByte[4];
        for (u32 luChannel = 0; luChannel < 4u; ++luChannel)
        {
            f32 lfValue = lpafChannel[luChannel];
            if (!(lfValue > 0.0f))      // also catches NaN
                lfValue = 0.0f;
            if (lfValue > 1.0f)
                lfValue = 1.0f;
            lauByte[luChannel] = static_cast<u32>(lfValue * 255.0f + 0.5f);
        }
        return D3DCOLOR_ARGB(lauByte[3], lauByte[0], lauByte[1], lauByte[2]);
    }

    // Clamp one Xenon rect to a surface extent. Returns false when nothing is left of it.
    bool TilingClampRect(const XenonResolveRect& lrSource, u32 luSurfaceWidth, u32 luSurfaceHeight,
                         D3DRECT* lpOut)
    {
        u32 luLeft   = lrSource.muLeft;
        u32 luTop    = lrSource.muTop;
        u32 luRight  = lrSource.muRight;
        u32 luBottom = lrSource.muBottom;

        if (luRight  > luSurfaceWidth)  luRight  = luSurfaceWidth;
        if (luBottom > luSurfaceHeight) luBottom = luSurfaceHeight;
        if (luLeft >= luRight || luTop >= luBottom)
            return false;

        lpOut->x1 = static_cast<LONG>(luLeft);
        lpOut->y1 = static_cast<LONG>(luTop);
        lpOut->x2 = static_cast<LONG>(luRight);
        lpOut->y2 = static_cast<LONG>(luBottom);
        return true;
    }

    // The bound colour render target, AddRef'd (caller releases), with its extent.
    IDirect3DSurface9* TilingAcquireBoundColour(IDirect3DDevice9* lpDevice,
                                                u32* lpuWidth, u32* lpuHeight)
    {
        IDirect3DSurface9* lpSurface = nullptr;
        if (FAILED(lpDevice->GetRenderTarget(0u, &lpSurface)) || lpSurface == nullptr)
            return nullptr;

        D3DSURFACE_DESC lDesc;
        if (FAILED(lpSurface->GetDesc(&lDesc)))
        {
            lpSurface->Release();
            return nullptr;
        }
        if (lpuWidth  != nullptr) *lpuWidth  = static_cast<u32>(lDesc.Width);
        if (lpuHeight != nullptr) *lpuHeight = static_cast<u32>(lDesc.Height);
        return lpSurface;
    }

    // Is that surface the SWAP CHAIN's back buffer? See the ordering tripwire in the banner.
    bool TilingBoundColourIsBackBuffer(IDirect3DDevice9* lpDevice, IDirect3DSurface9* lpBound)
    {
        IDirect3DSurface9* lpBackBuffer = nullptr;
        if (FAILED(lpDevice->GetBackBuffer(0u, 0u, D3DBACKBUFFER_TYPE_MONO, &lpBackBuffer))
            || lpBackBuffer == nullptr)
        {
            return false;
        }
        const bool lbSame = (lpBackBuffer == lpBound);
        lpBackBuffer->Release();   // GetBackBuffer AddRefs
        return lbSame;
    }

    // THE CLEAR, shared by D3DDevice_BeginTiling and the 0x300 D3DDevice_Resolve because it is
    // the same operation on the console: clear the surfaces the tiling pass owns, over the rects
    // it was given, to the caller's colour / Z / stencil.
    //
    // lpRects may be null (or luCount 0), which means the whole surface -- that is what
    // Clear(0, nullptr, ...) does and it is the only reading available when no rect is supplied.
    // lbExpectFullCoverage: true only for the BeginTiling clear, whose rect list is the WHOLE
    // tiling plan and must partition the surface; the per-tile 0x300 Resolve clear hands in ONE
    // band of that plan and is partial by design (two bands, two calls) -- it must not trip the
    // extent tripwire below.
    void TilingClearBoundSurfaces(IDirect3DDevice9* lpDevice, IDirect3DSurface9* lpBoundColour,
                                  u32 luSurfaceWidth, u32 luSurfaceHeight,
                                  const XenonResolveRect* lpRects, u32 luCount,
                                  const void* lpClearColour, f32 lfClearZ, u32 luClearStencil,
                                  bool lbExpectFullCoverage)
    {
        D3DRECT laRects[KU_MAX_TILE_RECTS];
        u32     luRects       = 0u;
        u32     luCoveredArea = 0u;

        if (lpRects != nullptr)
        {
            const u32 luWanted = (luCount < KU_MAX_TILE_RECTS) ? luCount : KU_MAX_TILE_RECTS;
            for (u32 luRect = 0; luRect < luWanted; ++luRect)
            {
                if (TilingClampRect(lpRects[luRect], luSurfaceWidth, luSurfaceHeight,
                                    &laRects[luRects]))
                {
                    luCoveredArea +=
                        static_cast<u32>(laRects[luRects].x2 - laRects[luRects].x1)
                        * static_cast<u32>(laRects[luRects].y2 - laRects[luRects].y1);
                    ++luRects;
                }
            }

            // The shipped tiling plans partition a 1280x720 screen EXACTLY (BrnAntiAliasTiling.h:
            // one 1280x720 rect with MSAA off, two 1280x384 / 1280x336 bands with it on) and their
            // extents are HARD-CODED there, not derived from the target. So if the display is not
            // 720p the rects cover only part of the surface and the rest is never cleared. That is
            // a real, visible failure (uncleared bands) and it is reported rather than papered
            // over: widening the caller's rectangles would be inventing a clear the console does
            // not perform.
            if (lbExpectFullCoverage && luRects != 0u
                && luCoveredArea != luSurfaceWidth * luSurfaceHeight)
            {
                LogOnce("tiling-rect-extent",
                        "[tiling] clear rects do not cover the bound surface -- the tiling plan is"
                        " hard-coded 1280x720 (BrnAntiAliasTiling.h) and the display is not."
                        " The uncovered region is never cleared.\n");
            }
        }

        const bool lbWantColourClear = (lpClearColour != nullptr) && (lpBoundColour != nullptr);
        DWORD luFlags = TilingClearFlagsForBoundSurfaces(lpDevice, lbWantColourClear);
        if (luFlags == 0u)
            return;

        const D3DCOLOR lColour = lbWantColourClear ? TilingColourFromFloat4(lpClearColour)
                                                   : static_cast<D3DCOLOR>(0);
        const D3DRECT* const lpaRects = (luRects != 0u) ? laRects : nullptr;

        // ---- THE CLEAR'S OWN CLIP STATE ------------------------------------------------------
        // D3D9's Clear is NOT a raw surface fill. It is clipped twice, by state this leaf does not
        // set:
        //   * by the SCISSOR RECTANGLE whenever the scissor test is on -- "The scissor test also
        //     affects the device IDirect3DDevice9::Clear operation" (Direct3D 9 SDK, "Scissor
        //     Test"), and
        //   * by the VIEWPORT -- Clear's own reference says the coordinates in pRects "are clipped
        //     to the bounds of the viewport rectangle", and a null rect list clears the viewport,
        //     not the surface.
        // On the untiled path this is the ONLY clear in the entire bracket, so whatever either one
        // shrinks is a band that keeps the PREVIOUS frame's colour AND depth -- a smear with stale
        // geometry standing up in it, and no error reported anywhere.
        //
        // Today the bind that precedes this (renderengine::Device::SetState ->
        // IDirect3DDevice9::SetRenderTarget) resets the viewport and the scissor RECTANGLE to the
        // full target -- but it does NOT reset D3DRS_SCISSORTESTENABLE, and PCSurfaceBracket_
        // Restore puts back whatever enable was live before the shadow pass, i.e. whatever the
        // PREVIOUS frame's 2D tail left (CgsIm2d.cpp:449 turns it on for a mask; PopMask at :464
        // turns it off again, so it is balanced only as long as every push is popped). The clear's
        // extent therefore rests on a chain of invariants this leaf neither owns nor can check.
        // It is made explicit instead: force the test off and the viewport to the bound surface's
        // full extent, clear, then put back exactly what was found.
        //
        // This is platform mechanics, not an invention -- the console's clear rode inside the
        // tiling pass and had no viewport or scissor state to trip over. Only the XY extent of the
        // viewport is forced; MinZ/MaxZ are carried across from the saved one, so nothing about the
        // depth range changes.
        D3DVIEWPORT9 lSavedViewport;
        std::memset(&lSavedViewport, 0, sizeof(lSavedViewport));
        const bool lbSavedViewport = SUCCEEDED(lpDevice->GetViewport(&lSavedViewport));

        DWORD luSavedScissorEnable = FALSE;
        if (FAILED(lpDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &luSavedScissorEnable)))
            luSavedScissorEnable = FALSE;
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        // Not forced at all unless it can be put back afterwards, and not forced when the extent
        // is unknown (no colour surface bound -- a depth-only clear keeps whatever viewport the
        // caller had).
        bool lbForcedViewport = false;
        if (lbSavedViewport && luSurfaceWidth != 0u && luSurfaceHeight != 0u)
        {
            D3DVIEWPORT9 lFullViewport;
            lFullViewport.X      = 0;
            lFullViewport.Y      = 0;
            lFullViewport.Width  = static_cast<DWORD>(luSurfaceWidth);
            lFullViewport.Height = static_cast<DWORD>(luSurfaceHeight);
            lFullViewport.MinZ   = lSavedViewport.MinZ;
            lFullViewport.MaxZ   = lSavedViewport.MaxZ;
            lbForcedViewport = SUCCEEDED(lpDevice->SetViewport(&lFullViewport));
        }

        HRESULT lHr = lpDevice->Clear(luRects, lpaRects, luFlags, lColour, lfClearZ,
                                      static_cast<DWORD>(luClearStencil));

        // Clear is ALL-OR-NOTHING (see the banner). The one failure mode worth surviving is a
        // stencil clear against a stencil-less depth format, because losing it would silently take
        // the DEPTH clear with it.
        if (FAILED(lHr) && (luFlags & static_cast<DWORD>(D3DCLEAR_STENCIL)) != 0u)
        {
            luFlags &= ~static_cast<DWORD>(D3DCLEAR_STENCIL);
            LogOnce("tiling-clear-nostencil",
                    "[tiling] Clear rejected D3DCLEAR_STENCIL -- the bound depth surface has no"
                    " stencil bits; retrying depth-only.\n");
            lHr = lpDevice->Clear(luRects, lpaRects, luFlags, lColour, lfClearZ,
                                  static_cast<DWORD>(luClearStencil));
        }

        if (FAILED(lHr))
        {
            char lacMessage[192];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[tiling] Clear FAILED hr=0x%08X flags=0x%02X rects=%u -- the scene"
                          " target is NOT being cleared, so the world composites over the previous"
                          " frame and its depth",
                          static_cast<unsigned>(lHr), static_cast<unsigned>(luFlags),
                          static_cast<unsigned>(luRects));
            LogRepeating(suTilingClearFailures, lacMessage);
        }

        // Put the clip state back exactly as it was found. The scissor RECTANGLE was never
        // touched, only the enable, so there is nothing else to restore.
        if (lbForcedViewport)
            lpDevice->SetViewport(&lSavedViewport);
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, luSavedScissorEnable);
    }

    // The destination surface of a resolve: mip level luLevel of the destination texture. Returns
    // it AddRef'd (caller releases), or null when the destination cannot be expressed on PC.
    IDirect3DSurface9* TilingAcquireDestSurface(Texture* lpDestTexture, u32 luLevel, u32 luSlice)
    {
        if (lpDestTexture == nullptr || lpDestTexture->mpD3DTexture == nullptr)
            return nullptr;

        // Every attested call site passes slice/face 0 (`li r9, 0` @0x823FFCC8 / @0x823FFD00 /
        // @0x823FFDA8 / @0x823FFDF0). A cube face or array slice would need a different accessor
        // than GetSurfaceLevel, so it is reported rather than silently resolved to face 0.
        if (luSlice != 0u)
        {
            LogOnce("resolve-slice",
                    "[postfx-resolve] non-zero DestSliceOrFace is not implemented on the PC leaf;"
                    " the copy is skipped.\n");
            return nullptr;
        }

        IDirect3DBaseTexture9* const lpBase = lpDestTexture->mpD3DTexture;
        if (lpBase->GetType() != D3DRTYPE_TEXTURE)
        {
            LogOnce("resolve-desttype",
                    "[postfx-resolve] the destination is not a 2D texture; the copy is skipped.\n");
            return nullptr;
        }

        IDirect3DSurface9* lpSurface = nullptr;
        if (FAILED(static_cast<IDirect3DTexture9*>(lpBase)->GetSurfaceLevel(luLevel, &lpSurface)))
            return nullptr;
        return lpSurface;
    }

    // Is this surface multisampled? Asked of the D3D object itself rather than of any engine-side
    // record, for the same reason the raw-depth sampler hook asks GetLevelDesc: the runtime is the
    // only thing that knows what was actually created.
    bool TilingSurfaceIsMultisampled(IDirect3DSurface9* lpSurface)
    {
        if (lpSurface == nullptr)
            return false;
        D3DSURFACE_DESC lDesc;
        std::memset(&lDesc, 0, sizeof(lDesc));
        if (FAILED(lpSurface->GetDesc(&lDesc)))
            return false;
        return lDesc.MultiSampleType != D3DMULTISAMPLE_NONE;
    }

    // =========================================================================================
    // THE DEPTH RESOLVE -- 'RESZ', THE D3D9 VENDOR MECHANISM, REPLACING A DOCUMENTED NO-OP.
    //
    // WHAT THE CONSOLE DOES. BrnRendererModule::ResolveMSAA issues TWO resolves per tile. The
    // 0x14 one carries KU_RESOLVE_DEPTH_STENCIL_BIT and its destination is
    // `GetDownSampleBuffer()->GetDepthTexture()` (BrnRendererModule.cpp:2228 tiled, :2272
    // untiled): on the Xenos that copies -- and down-samples -- the EDRAM depth surface into a
    // linear, sampleable texture.
    //
    // WHAT THIS LEAF USED TO DO: nothing, and it said so ("StretchRect refuses depth surfaces;
    // the down-sample depth texture is not written"). That was TRUE and it was also A REAL HOLE
    // INDEPENDENT OF MSAA. Rung 6 gave the down-sample buffer an INTZ RAW-depth texture precisely
    // so the composite's DoF permutations (slots 2/3/6/7/10/11), its motion-blur permutations
    // (4..11) and rw::graphics::postfx::DepthOfField could read scene depth -- and NOTHING HAS
    // EVER WRITTEN IT. Every one of those fetches has been reading whatever D3D9 left in a fresh
    // D3DPOOL_DEFAULT surface. MSAA does not create that hole; it makes it unavoidable. Closing
    // it here closes it for the NON-multisampled path too.
    //
    // THE MECHANISM. D3D9 has no depth resolve and no depth copy: StretchRect rejects
    // depth-stencil surfaces, GetRenderTargetData rejects them, and a depth surface cannot be a
    // texture's level. The vendors expose one anyway, and NOT the same one:
    //   * AMD (Radeon HD 2000 and later) and Intel: the pseudo-format command hook 'RESZ' below.
    //   * NVIDIA: NO RESZ -- CheckDeviceFormat('RESZ') fails on every GeForce (verified on the
    //     GTX 1660 Ti this tree boots on). Its D3D9 depth resolve is NvAPI_D3D9_StretchRectEx
    //     (nvapi64.dll), which copies AND multisample-resolves between depth-stencil resources,
    //     multisampled D24S8 -> INTZ included. It is loaded DYNAMICALLY (nvapi_QueryInterface by
    //     the public interface ids from NVIDIA's nvapi_interface.h) so the build takes no NVAPI
    //     link dependency and a machine without the DLL just falls through to the honest line.
    // The RESZ recipe:
    //   * AVAILABILITY = CheckDeviceFormat(adapter, devtype, adapterFormat,
    //     D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, MAKEFOURCC('R','E','S','Z')) succeeding.
    //   * THE TRIGGER = bind the DESTINATION depth texture to a sampler stage, issue a dummy draw
    //     so the driver has consumed that binding, then SetRenderState(D3DRS_POINTSIZE,
    //     0x7FA05000). The driver resolves the CURRENTLY BOUND depth-stencil -- multisampled or
    //     not -- into that texture. The dummy draw is part of the published recipe, not
    //     decoration: without it the sampler bind has not reached the driver when the magic
    //     render state arrives.
    //
    // IT IS WHOLE-SURFACE ONLY. There is no rectangle in the hook, so the console's SOURCE RECT
    // is IGNORED here and that is stated rather than hidden: on the two-tile plan this runs twice
    // over the same full surface and the second pass writes identical pixels -- redundant, not
    // wrong. (CROSS-GROUP: BrnRendererModule may hoist the depth resolve out of the tile loop on
    // PC; this body is idempotent either way.)
    //
    // ONLY INTZ. RESZ writes a depth-stencil-layout texture; INTZ *is* a D24S8 layout, which is
    // what the hook produces. DF24 / DF16 are ATI depth-SAMPLE formats with a different internal
    // form and the hook does not write them -- so a machine that fell to those on rung 6's RAW
    // ladder gets the honest "not written" line instead of a silently wrong texture.
    //
    // EVERY PIECE OF STATE THE RECIPE DISTURBS IS SAVED AND PUT BACK: both shaders, the vertex
    // declaration (SetFVF replaces it) or the FVF when there was none, Z enable / Z write, the
    // colour write mask, D3DRS_POINTSIZE itself, and the stage-0 texture. The stage-0 texture is
    // restored to the EXACT object that was there, which is what keeps shadow::Device's sampler
    // cache honest -- it dedups on the TextureState it last installed, so a bind that ends where
    // it started leaves that belief TRUE, whereas an unbind would quietly make it a lie (the same
    // split-brain renderengine::Device::SetState's unbind loop exists to avoid). For the same
    // reason the bind is a RAW SetTexture and not D3DDevice_SetTexture: routing it through the
    // shim would claim stage 0 in the raw-depth unit mask and force POINT/CLAMP on a unit the
    // frame is about to reuse.
    //
    // NOTHING ELSE IS TOUCHED. DrawPrimitiveUP resets the stream-0 binding, which costs nothing
    // here -- but NOT for the reason the D3DDevice_EndVertices banner gives. That banner's "no TU
    // in b5-decomp/src calls IDirect3DDevice9::SetStreamSource at all" WENT STALE with the
    // retained world vertex/index buffers: WorldGeometryPCLeaf.cpp:810-817 calls SetStreamSource /
    // SetIndices / DrawIndexedPrimitive straight on the device. It costs nothing because NOTHING
    // CACHES a stream-0 binding across draws -- WorldGeometry_Submit re-binds stream 0 and the
    // indices on every submit, and shadowingdevice.cpp's FlushVertexProgramState re-issues every
    // stream source on every flush, both outside any dirty gate.
    // The draw needs an OPEN SCENE, and it has one: this runs from ResolveMSAA in
    // the middle of the frame, and the colour resolve's EndScene/BeginScene suspend is scoped to
    // its own StretchRect, which happens after this.
    //
    // IF RESZ IS UNAVAILABLE the call says so ONCE and does nothing -- the honest fallback, not a
    // silent one. It does NOT re-render the scene to reconstruct depth.
    // =========================================================================================
    const DWORD KU_RESZ_MAGIC = 0x7FA05000u;

    u32 suDepthResolveFailures = 0u;

    // -1 not yet asked, 0 no, 1 yes. CheckDeviceFormat is a driver round trip and the answer
    // cannot change for the life of the device.
    int siReszSupported = -1;

    bool TilingReszSupported(IDirect3DDevice9* lpDevice)
    {
        if (siReszSupported >= 0)
            return siReszSupported != 0;

        siReszSupported = 0;

        D3DDEVICE_CREATION_PARAMETERS lCreation;
        std::memset(&lCreation, 0, sizeof(lCreation));
        lCreation.AdapterOrdinal = D3DADAPTER_DEFAULT;
        lCreation.DeviceType     = D3DDEVTYPE_HAL;
        lpDevice->GetCreationParameters(&lCreation);

        IDirect3D9* lpD3D = nullptr;
        if (SUCCEEDED(lpDevice->GetDirect3D(&lpD3D)) && lpD3D != nullptr)
        {
            D3DFORMAT      leAdapterFormat = D3DFMT_X8R8G8B8;
            D3DDISPLAYMODE lMode;
            std::memset(&lMode, 0, sizeof(lMode));
            if (SUCCEEDED(lpD3D->GetAdapterDisplayMode(lCreation.AdapterOrdinal, &lMode)))
                leAdapterFormat = lMode.Format;

            if (SUCCEEDED(lpD3D->CheckDeviceFormat(
                    lCreation.AdapterOrdinal, lCreation.DeviceType, leAdapterFormat,
                    D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE,
                    static_cast<D3DFORMAT>(MAKEFOURCC('R', 'E', 'S', 'Z')))))
            {
                siReszSupported = 1;
            }
            lpD3D->Release();       // GetDirect3D AddRefs
        }
        return siReszSupported != 0;
    }

    // ---- THE NVIDIA PATH: NvAPI_D3D9_StretchRectEx, loaded at run time -------------------------
    // Interface ids are the published constants from nvapi_interface.h; a wrong or absent one
    // simply comes back null from nvapi_QueryInterface and the path reports itself unavailable.
    // NVAPI requires both resources to be registered (NvAPI_D3D9_RegisterResource) before
    // StretchRectEx accepts them; registration is cached per resource pointer.
    typedef void* (__cdecl* NvApiQueryInterfaceFn)(unsigned int);
    typedef int   (__cdecl* NvApiInitializeFn)();
    typedef int   (__cdecl* NvApiD3D9RegisterResourceFn)(IDirect3DResource9*);
    typedef int   (__cdecl* NvApiD3D9StretchRectExFn)(IDirect3DDevice9*, IDirect3DResource9*,
                                                     const RECT*, IDirect3DResource9*,
                                                     const RECT*, D3DTEXTUREFILTERTYPE);
    const unsigned int KU_NVAPI_ID_INITIALIZE             = 0x0150E828u;
    const unsigned int KU_NVAPI_ID_D3D9_REGISTER_RESOURCE = 0xA064BDFCu;
    const unsigned int KU_NVAPI_ID_D3D9_STRETCH_RECT_EX   = 0x22DE03AAu;
    const u32          KU_NVAPI_MAX_REGISTERED            = 8u;

    int                          siNvApiState       = -1;   // -1 untried, 0 unavailable, 1 ready
    NvApiD3D9RegisterResourceFn  spNvApiRegister    = nullptr;
    NvApiD3D9StretchRectExFn     spNvApiStretchRect = nullptr;
    IDirect3DResource9*          sapNvApiRegistered[KU_NVAPI_MAX_REGISTERED] = {};
    u32                          suNvApiRegistered  = 0u;

    bool TilingNvApiReady()
    {
        if (siNvApiState >= 0)
            return siNvApiState == 1;
        siNvApiState = 0;

        const HMODULE lhNvApi = LoadLibraryA("nvapi64.dll");
        if (lhNvApi == nullptr)
            return false;
        const NvApiQueryInterfaceFn lpQuery = reinterpret_cast<NvApiQueryInterfaceFn>(
            reinterpret_cast<void*>(GetProcAddress(lhNvApi, "nvapi_QueryInterface")));
        if (lpQuery == nullptr)
            return false;

        const NvApiInitializeFn lpInitialize =
            reinterpret_cast<NvApiInitializeFn>(lpQuery(KU_NVAPI_ID_INITIALIZE));
        spNvApiRegister    = reinterpret_cast<NvApiD3D9RegisterResourceFn>(
                                 lpQuery(KU_NVAPI_ID_D3D9_REGISTER_RESOURCE));
        spNvApiStretchRect = reinterpret_cast<NvApiD3D9StretchRectExFn>(
                                 lpQuery(KU_NVAPI_ID_D3D9_STRETCH_RECT_EX));
        if (lpInitialize == nullptr || spNvApiRegister == nullptr || spNvApiStretchRect == nullptr)
            return false;
        if (lpInitialize() != 0)
            return false;

        siNvApiState = 1;
        return true;
    }

    bool TilingNvApiRegister(IDirect3DResource9* lpResource)
    {
        for (u32 luSlot = 0u; luSlot < suNvApiRegistered; ++luSlot)
        {
            if (sapNvApiRegistered[luSlot] == lpResource)
                return true;
        }
        if (spNvApiRegister(lpResource) != 0)
            return false;
        if (suNvApiRegistered < KU_NVAPI_MAX_REGISTERED)
            sapNvApiRegistered[suNvApiRegistered++] = lpResource;
        return true;
    }

    // Returns true when the NVAPI path RAN (success or its own logged failure), false when it is
    // not available on this machine so the caller can say so.
    bool TilingResolveDepthViaNvApi(IDirect3DDevice9* lpDevice, IDirect3DTexture9* lpDest)
    {
        if (!TilingNvApiReady())
            return false;

        IDirect3DSurface9* lpSource = nullptr;
        if (FAILED(lpDevice->GetDepthStencilSurface(&lpSource)) || lpSource == nullptr)
        {
            LogOnce("resolve-depth-nvapi-nosrc",
                    "[postfx-resolve] NVAPI depth resolve: no depth-stencil surface is bound;"
                    " the down-sample depth stays unwritten.\n");
            return true;
        }
        IDirect3DSurface9* lpDestSurface = nullptr;
        if (FAILED(lpDest->GetSurfaceLevel(0u, &lpDestSurface)) || lpDestSurface == nullptr)
        {
            lpSource->Release();
            return true;
        }

        // NVAPI wants TOP-LEVEL resources (the texture, not its level-0 surface; the stand-alone
        // depth surface as itself). Which combination the driver accepts is not documented
        // precisely enough to bet on, so the variants are tried in order and the one that works
        // is remembered (siNvApiVariant) and named in the log:
        //   0: destination = texture, source left bound as the depth-stencil
        //   1: destination = texture, source UNBOUND for the copy (rebound after)
        //   2: destination = level-0 surface, source unbound
        static int siNvApiVariant = -1;      // -1 unknown yet
        const bool lbRegisteredSource = TilingNvApiRegister(lpSource);
        const bool lbRegisteredTex    = TilingNvApiRegister(lpDest);
        const bool lbRegisteredSurf   = TilingNvApiRegister(lpDestSurface);
        if (!lbRegisteredSource || (!lbRegisteredTex && !lbRegisteredSurf))
        {
            lpDestSurface->Release();
            lpSource->Release();
            LogOnce("resolve-depth-nvapi-register",
                    "[postfx-resolve] NVAPI depth resolve: NvAPI_D3D9_RegisterResource refused"
                    " the depth surface / INTZ texture -- the down-sample depth stays"
                    " unwritten.\n");
            return true;
        }

        int lStatus = -1;
        int lVariantUsed = -1;
        const int lFirst = (siNvApiVariant >= 0) ? siNvApiVariant : 0;
        const int lLast  = (siNvApiVariant >= 0) ? siNvApiVariant : 2;
        for (int lVariant = lFirst; lVariant <= lLast && lStatus != 0; ++lVariant)
        {
            IDirect3DResource9* const lpDestResource =
                (lVariant == 2) ? static_cast<IDirect3DResource9*>(lpDestSurface)
                                : static_cast<IDirect3DResource9*>(lpDest);
            if ((lVariant == 2 && !lbRegisteredSurf) || (lVariant != 2 && !lbRegisteredTex))
                continue;

            const bool lbUnbind = (lVariant != 0);
            if (lbUnbind)
                lpDevice->SetDepthStencilSurface(nullptr);

            lStatus = spNvApiStretchRect(lpDevice, lpSource, nullptr, lpDestResource, nullptr,
                                         D3DTEXF_NONE);
            if (lStatus != 0)
            {
                lStatus = spNvApiStretchRect(lpDevice, lpSource, nullptr, lpDestResource,
                                             nullptr, D3DTEXF_POINT);
            }

            if (lbUnbind)
                lpDevice->SetDepthStencilSurface(lpSource);
            if (lStatus == 0)
                lVariantUsed = lVariant;
        }
        lpDestSurface->Release();
        lpSource->Release();

        if (lStatus != 0)
        {
            char lacMessage[256];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[postfx-resolve] the NVAPI StretchRectEx depth resolve was REJECTED"
                          " (status %d, all variants) -- the down-sample depth texture is NOT"
                          " being written, so DoF / motion blur read stale depth", lStatus);
            LogRepeating(suDepthResolveFailures, lacMessage);
            return true;
        }

        siNvApiVariant = lVariantUsed;
        char lacOk[320];
        std::snprintf(lacOk, sizeof(lacOk),
                      "[postfx-resolve] depth resolve via NVAPI StretchRectEx -> INTZ (variant %d:"
                      " %s; NVIDIA has no RESZ; this also fills the down-sample depth texture on"
                      " the NON-multisampled path, which nothing wrote before).\n",
                      lVariantUsed,
                      (lVariantUsed == 0) ? "dest texture, source bound"
                    : (lVariantUsed == 1) ? "dest texture, source unbound"
                                          : "dest level-0 surface, source unbound");
        LogOnce("resolve-depth-nvapi", lacOk);
        return true;
    }

    void TilingResolveDepthToTexture(IDirect3DDevice9* lpDevice, Texture* lpDestTexture)
    {
        if (lpDestTexture == nullptr || lpDestTexture->mpD3DTexture == nullptr)
        {
            LogOnce("resolve-depth-nodest",
                    "[postfx-resolve] the depth resolve has no destination texture; the"
                    " down-sample depth stays unwritten.\n");
            return;
        }

        IDirect3DBaseTexture9* const lpBase = lpDestTexture->mpD3DTexture;
        if (lpBase->GetType() != D3DRTYPE_TEXTURE)
        {
            LogOnce("resolve-depth-desttype",
                    "[postfx-resolve] the depth resolve destination is not a 2D texture; the"
                    " down-sample depth stays unwritten.\n");
            return;
        }
        IDirect3DTexture9* const lpDest = static_cast<IDirect3DTexture9*>(lpBase);

        D3DSURFACE_DESC lDestDesc;
        std::memset(&lDestDesc, 0, sizeof(lDestDesc));
        if (FAILED(lpDest->GetLevelDesc(0u, &lDestDesc))
            || lDestDesc.Format != static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z')))
        {
            LogOnce("resolve-depth-notintz",
                    "[postfx-resolve] the depth resolve destination is not an INTZ texture -- the"
                    " RESZ hook writes only INTZ, so the down-sample depth stays unwritten.\n");
            return;
        }

        if (!TilingReszSupported(lpDevice))
        {
            if (TilingResolveDepthViaNvApi(lpDevice, lpDest))
                return;
            LogOnce("resolve-depth-noresz",
                    "[postfx-resolve] depth resolve: RESZ unsupported and NVAPI StretchRectEx"
                    " unavailable -> the down-sample depth stays unwritten (DoF / motion blur"
                    " read an unwritten INTZ).\n");
            return;
        }

        // ---- save everything the recipe disturbs ------------------------------------------
        IDirect3DBaseTexture9*       lpSavedTexture0 = nullptr;
        IDirect3DVertexShader9*      lpSavedVs       = nullptr;
        IDirect3DPixelShader9*       lpSavedPs       = nullptr;
        IDirect3DVertexDeclaration9* lpSavedDecl     = nullptr;
        DWORD luSavedFvf          = 0u;
        DWORD luSavedZEnable      = 0u;
        DWORD luSavedZWrite       = 0u;
        DWORD luSavedColourWrite  = 0u;
        DWORD luSavedPointSize    = 0u;

        lpDevice->GetTexture(0u, &lpSavedTexture0);         // AddRefs on success
        lpDevice->GetVertexShader(&lpSavedVs);              // AddRefs on success
        lpDevice->GetPixelShader(&lpSavedPs);               // AddRefs on success
        lpDevice->GetVertexDeclaration(&lpSavedDecl);       // AddRefs on success
        lpDevice->GetFVF(&luSavedFvf);
        lpDevice->GetRenderState(D3DRS_ZENABLE,          &luSavedZEnable);
        lpDevice->GetRenderState(D3DRS_ZWRITEENABLE,     &luSavedZWrite);
        lpDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &luSavedColourWrite);
        lpDevice->GetRenderState(D3DRS_POINTSIZE,        &luSavedPointSize);

        // ---- the published RESZ sequence ---------------------------------------------------
        lpDevice->SetTexture(0u, lpDest);
        lpDevice->SetVertexShader(nullptr);
        lpDevice->SetPixelShader(nullptr);
        lpDevice->SetFVF(D3DFVF_XYZ);
        lpDevice->SetRenderState(D3DRS_ZENABLE,          FALSE);
        lpDevice->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
        lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0u);

        // One degenerate point, colour writes off, depth test and write off: it rasterises
        // nothing and can touch neither the scene colour nor the depth it is about to resolve.
        const f32 lafDummyPoint[3] = { 0.0f, 0.0f, 0.0f };
        const HRESULT lHrDraw =
            lpDevice->DrawPrimitiveUP(D3DPT_POINTLIST, 1u, lafDummyPoint, sizeof(f32) * 3u);

        lpDevice->SetRenderState(D3DRS_ZWRITEENABLE,     TRUE);
        lpDevice->SetRenderState(D3DRS_ZENABLE,          TRUE);
        lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000Fu);

        const HRESULT lHrResolve = lpDevice->SetRenderState(D3DRS_POINTSIZE, KU_RESZ_MAGIC);

        // ---- put the device back exactly as it was found ------------------------------------
        lpDevice->SetRenderState(D3DRS_POINTSIZE,        luSavedPointSize);
        lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE, luSavedColourWrite);
        lpDevice->SetRenderState(D3DRS_ZWRITEENABLE,     luSavedZWrite);
        lpDevice->SetRenderState(D3DRS_ZENABLE,          luSavedZEnable);
        if (lpSavedDecl != nullptr)
            lpDevice->SetVertexDeclaration(lpSavedDecl);
        else
            lpDevice->SetFVF(luSavedFvf);       // nothing had a declaration: restore the FVF
        lpDevice->SetPixelShader(lpSavedPs);
        lpDevice->SetVertexShader(lpSavedVs);
        lpDevice->SetTexture(0u, lpSavedTexture0);

        if (lpSavedDecl     != nullptr) lpSavedDecl->Release();
        if (lpSavedPs       != nullptr) lpSavedPs->Release();
        if (lpSavedVs       != nullptr) lpSavedVs->Release();
        if (lpSavedTexture0 != nullptr) lpSavedTexture0->Release();

        if (FAILED(lHrDraw) || FAILED(lHrResolve))
        {
            char lacMessage[256];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[postfx-resolve] the RESZ depth resolve was REJECTED"
                          " (draw hr=0x%08X, trigger hr=0x%08X) -- the down-sample depth texture"
                          " is NOT being written, so DoF / motion blur read stale depth",
                          static_cast<unsigned>(lHrDraw), static_cast<unsigned>(lHrResolve));
            LogRepeating(suDepthResolveFailures, lacMessage);
            return;
        }

        LogOnce("resolve-depth-resz",
                "[postfx-resolve] depth resolve via RESZ -> INTZ (this also fills the down-sample"
                " depth texture on the NON-multisampled path, which nothing wrote before).\n");
    }
}

// ---------------------------------------------------------------------------------------------
// D3DDevice_BeginTiling -- open a predicated-tiling pass, clearing each tile as it opens.
//
// ON PC: the tiling half has no counterpart and needs none (there is one surface and one pass, so
// there is nothing to replay per tile and nothing to place in EDRAM); what the call really DOES,
// and what nothing else in BeginRenderAntiAliased does, is THE CLEAR. That is what is implemented.
// The viewport question the banner in BrnRendererModule.cpp leaves to this leaf is answered above:
// SetRenderTarget has already set the viewport to the whole scene target, so this owes nothing.
//
// luFlags is 0 at the only attested call site (`li r4, 0# Flags` @0x823FFB58) and has no decoded
// meaning; it is not acted on. lpDevice is the console's off_83271608 and is ignored for the same
// reason every other shim in this file ignores it -- Dev() is the one live PC device.
//
// REACHED EVERY FRAME SINCE RUNG 8: BeginRenderAntiAliased calls this on its multisampled branch
// and mbMultisampledBackbuffer is the console's constant TRUE (BrnRendererModule.h).
// ---------------------------------------------------------------------------------------------
void D3DDevice_BeginTiling(void* /*lpDevice*/, u32 /*luFlags*/, u32 luCount,
                           const void* lpTileRects, const void* lpClearColour, f32 lfClearZ,
                           u32 luClearStencil)
{
    IDirect3DDevice9* const lpD3DDevice = Dev();
    if (lpD3DDevice == nullptr)
        return;

    u32 luSurfaceWidth  = 0u;
    u32 luSurfaceHeight = 0u;
    IDirect3DSurface9* const lpBoundColour =
        TilingAcquireBoundColour(lpD3DDevice, &luSurfaceWidth, &luSurfaceHeight);

    if (lpBoundColour != nullptr && TilingBoundColourIsBackBuffer(lpD3DDevice, lpBoundColour))
    {
        // See the ordering tripwire in the banner.
        LogOnce("begintiling-backbuffer",
                "[tiling] BeginTiling with the SWAP CHAIN bound -- refusing to clear the frame."
                " BeginRenderAntiAliased must bind the scene target before this runs.\n");
        lpBoundColour->Release();
        return;
    }

    TilingClearBoundSurfaces(lpD3DDevice, lpBoundColour, luSurfaceWidth, luSurfaceHeight,
                             static_cast<const XenonResolveRect*>(lpTileRects), luCount,
                             lpClearColour, lfClearZ, luClearStencil,
                             /*lbExpectFullCoverage*/ true);

    // ---- THE ONE-PASS VIEWPORT (rung 8) ------------------------------------------------------
    // With the console's MSAA plan this call now receives TWO tile rectangles, and on PC there is
    // ONE pass over ONE surface -- so the viewport and the scissor must span BOTH tiles, not one.
    // They do, and nothing here has to set them: BeginRenderAntiAliased binds the anti-alias
    // buffer immediately before this call, that bind runs rw::graphics::postfx::RenderTarget::
    // Begin, and Begin sets the viewport AND the scissor to the target's full muWidth x muHeight
    // (PostFxRenderTargetPCLeaf.cpp). Setting one here would be inventing a console behaviour --
    // the console's viewport came from the tiling pass itself, which does not exist on PC.
    // The two shipped rects also partition the surface EXACTLY ({0,0,1280,384} + {0,384,1280,720};
    // rect 1's top IS rect 0's bottom, BrnAntiAliasTiling.h), so the clear above leaves no band at
    // y = 384 and TilingClearBoundSurfaces' own coverage check stays silent.
    // CHECKED rather than assumed, because the failure is silent: a viewport that does not span
    // the surface clips the whole world pass to one band with no error attached to it.
    if (lpBoundColour != nullptr && luSurfaceWidth != 0u && luSurfaceHeight != 0u)
    {
        D3DVIEWPORT9 lViewport;
        std::memset(&lViewport, 0, sizeof(lViewport));
        if (SUCCEEDED(lpD3DDevice->GetViewport(&lViewport))
            && (lViewport.X != 0u || lViewport.Y != 0u
                || lViewport.Width  != static_cast<DWORD>(luSurfaceWidth)
                || lViewport.Height != static_cast<DWORD>(luSurfaceHeight)))
        {
            char lacMessage[256];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[tiling] the viewport at BeginTiling is %lux%lu@%lu,%lu but the bound"
                          " surface is %ux%u -- the ONE-PASS world render is CLIPPED to the"
                          " viewport, so the tiles outside it stay at the clear colour.\n",
                          static_cast<unsigned long>(lViewport.Width),
                          static_cast<unsigned long>(lViewport.Height),
                          static_cast<unsigned long>(lViewport.X),
                          static_cast<unsigned long>(lViewport.Y),
                          static_cast<unsigned>(luSurfaceWidth),
                          static_cast<unsigned>(luSurfaceHeight));
            LogOnce("begintiling-viewport", lacMessage);
        }
    }

    if (lpBoundColour != nullptr)
        lpBoundColour->Release();
}

// ---------------------------------------------------------------------------------------------
// D3DDevice_SetPredication -- select which EDRAM tile's replay of the command stream the following
// commands join.
//
// HONEST NO-OP, and here is why it is honest rather than convenient. Predication exists because a
// tiled Xenos frame REPLAYS the whole command stream once per tile: every command is submitted
// several times and the mask says which replays it belongs to. On PC each command is submitted
// exactly once against one full-size surface, so there is no replay to select and no command to
// exclude. The per-tile work the console predicated -- the resolves -- is instead scoped
// explicitly by the rectangle each D3DDevice_Resolve is handed, so nothing is lost by dropping the
// mask. The existing precedent for a legitimately empty D3DDevice_* shim in this file is
// D3DDevice_Begin/EndConditionalRendering.
//
// REACHED THREE TIMES A FRAME SINCE RUNG 8 (BeginRenderAntiAliased once with mask 0, then once per
// resolve tile) -- mbMultisampledBackbuffer is the console's constant TRUE, so both bracket bodies
// take their tiled branch. See "WHICH OF THE FOUR ACTUALLY RUNS ON THIS BUILD" above.
// ---------------------------------------------------------------------------------------------
void D3DDevice_SetPredication(void* /*lpDevice*/, u32 /*luPredicationMask*/)
{
}

// ---------------------------------------------------------------------------------------------
// D3DDevice_EndTiling -- end predicated tiling, resolving the tiled surface.
//
// HONEST NO-OP AT ITS ATTESTED CALL SITE, which is the whole of the evidence: ResolveMSAA passes
// null for every pointer argument (`li r4, 0` ResolveFlags, `li r5, 0` pResolveRects, `li r6, 0`
// pDestTexture, `li r7, 0` pClearColor, `li r10, 0` pParameters -- @0x823FFD3C-0x823FFD54), so
// there is nothing to resolve and nothing to clear; the per-tile resolves above it already did
// both. With no tiling pass open on PC, closing one is a no-op too.
//
// THE ONE CALL SITE THAT PASSES A DESTINATION is PixelBuffer::Xbox2ResolveTo (PixelBuffer.cpp:164),
// on the branch gated by `gXbox2TilingEnabled` -- a global that is DECLARED (Xbox2SurfaceShims.h)
// and defined nowhere, in a TU that is not on tools/build/build_game_exe.bat. If that path is ever
// mounted, this body owes the same copy D3DDevice_Resolve below performs. It is left unwritten
// rather than guessed because no evidence in this image says what EndTiling's resolve rectangle
// would be.
//
// REACHED ONCE A FRAME SINCE RUNG 8 (ResolveMSAA's tiled branch closes the pass with every pointer
// argument null) -- see "WHICH OF THE FOUR ACTUALLY RUNS ON THIS BUILD" above.
// ---------------------------------------------------------------------------------------------
int D3DDevice_EndTiling(void* /*lpDevice*/, u32 /*luResolveFlags*/, const void* /*lpResolveRects*/,
                        Texture* /*lpDestTexture*/, const void* /*lpClearColour*/,
                        f32 /*lfClearZ*/, u32 /*luClearStencil*/, const void* /*lpParameters*/)
{
    return 0;   // unobservable: no attested caller reads it (see the banner)
}

// ---------------------------------------------------------------------------------------------
// D3DDevice_Resolve -- copy (and MSAA-downsample) the bound EDRAM surface out into a linear
// destination texture, optionally clearing the EDRAM behind it.
//
// THIS IS THE ONE OF THE FOUR THAT RUNS ON THIS BUILD, twice a frame, and it is the ONLY clear in
// the whole bracket on the untiled path -- BeginRenderAntiAliased's untiled branch
// (0x823FFB90-0x823FFBD8) never even loads the device pointer and clears nothing, deliberately,
// because the PREVIOUS frame's colour resolve left the surface clean. So the frame's clean-slate
// invariant is established here, at the END of frame N, for frame N+1.
//
// TWO HALVES, in the console's order:
//   (1) THE COPY. Colour resolves (Flags without the 0x04 depth bit) become a StretchRect from the
//       bound render target to the destination texture's surface. Depth resolves cannot be
//       expressed at all on D3D9 and say so.
//   (2) THE CLEAR. Flags carrying 0x300 clear the bound colour + depth/stencil, over the resolve
//       rectangle, to the caller's colour / Z / stencil.
// Copy BEFORE clear, because the console clears the surface it is copying OUT of.
//
// TWO PLATFORM DIFFERENCES SIT ON TOP OF THAT, both in the banner: the copy is issued with the
// frame's scene SUSPENDED (EndScene / StretchRect / BeginScene), and the clear is WITHHELD when a
// required copy did not land -- so that a copy failure degrades to a smear you can see rather than
// to a black screen you cannot tell apart from a dozen other faults.
//
// ⚠ THE FRAME ORDER THIS DEPENDS ON. The clear lands on the surfaces that are BOUND, which is what
// the console's clear lands on too. That makes the call order load-bearing on PC in a way it is
// not on the console: BrnRendererModule::ResolveMSAA must run while the scene target is still
// bound -- i.e. AFTER the world passes and BEFORE anything hands the frame back to the swap chain.
// If it runs after the present blit, the blit has already rebound the back buffer (PCSceneBlit_
// Begin, and it deliberately does not put the scene target back) and this would clear the finished
// frame. It refuses to, and logs, rather than doing it.
// ---------------------------------------------------------------------------------------------
int D3DDevice_Resolve(void* /*lpDevice*/, u32 luFlags, const void* lpSourceRect,
                      Texture* lpDestTexture, const void* lpDestPoint, u32 luDestLevel,
                      u32 luDestSliceOrFace, const void* lpClearColour, f32 lfClearZ,
                      u32 luClearStencil, const void* /*lpParameters*/)
{
    IDirect3DDevice9* const lpD3DDevice = Dev();
    if (lpD3DDevice == nullptr)
        return 0;

    u32 luSurfaceWidth  = 0u;
    u32 luSurfaceHeight = 0u;
    IDirect3DSurface9* const lpBoundColour =
        TilingAcquireBoundColour(lpD3DDevice, &luSurfaceWidth, &luSurfaceHeight);
    if (lpBoundColour == nullptr)
    {
        LogOnce("resolve-nort",
                "[postfx-resolve] no colour render target is bound; the resolve is skipped.\n");
        return 0;
    }

    if (TilingBoundColourIsBackBuffer(lpD3DDevice, lpBoundColour))
    {
        // See the frame-order warning above. This is the misordering tripwire, and it is a REFUSAL
        // (not a warning-and-proceed) because proceeding would erase the finished frame.
        LogOnce("resolve-backbuffer",
                "[postfx-resolve] ResolveMSAA ran with the SWAP CHAIN bound -- refusing, because"
                " the clear would erase the finished frame. Call ResolveMSAA while the scene"
                " target is still bound, i.e. before the present blit.\n");
        lpBoundColour->Release();
        return 0;
    }

    const XenonResolveRect* const lpRect =
        static_cast<const XenonResolveRect*>(lpSourceRect);

    // Did the copy this call owes actually land? The clear below is GATED on it -- see THE COPY
    // AND THE CLEAR ARE COUPLED in the banner. `lbCopyDone` is set in exactly one place, on a
    // succeeded StretchRect, so every other way of not copying (no destination texture, a
    // non-2D destination, a non-zero slice, an empty rectangle, the destination being the bound
    // surface) also counts as "did not copy" and also withholds the clear.
    const bool lbCopyRequired = ((luFlags & KU_RESOLVE_DEPTH_STENCIL_BIT) == 0u);
    bool       lbCopyDone     = false;

    // ---- (1) THE COPY ------------------------------------------------------------------------
    if ((luFlags & KU_RESOLVE_DEPTH_STENCIL_BIT) != 0u)
    {
        // THE DEPTH/STENCIL RESOLVE -- REAL SINCE RUNG 8, through the D3D9 vendor RESZ hook. The
        // mechanism, the evidence and every piece of state it saves are on
        // TilingResolveDepthToTexture above; the two things a reader needs HERE are:
        //   * it resolves the CURRENTLY BOUND depth-stencil, which is what the console's resolve
        //     copies too, so this call site needs no surface of its own -- and it must therefore
        //     run while the scene target is still bound, exactly like the colour half below;
        //   * the SOURCE RECT IS IGNORED by the hook (it has no rectangle) -- so on the two-tile
        //     plan the resolve must run ONCE PER BRACKET, for the FIRST tile only. The console's
        //     ResolveMSAA loop is depth(tile); colour+CLEAR(tile) per tile, and the tile-0 colour
        //     resolve's 0x300 flags carry D3DCLEAR_ZBUFFER over rows 0..383 (TilingClearFlagsFor
        //     BoundSurfaces): a second whole-surface RESZ after that would resolve an already-cleared
        //     top band into the INTZ -- DoF/motion blur reading Z=1 for the upper half, no error
        //     anywhere (the msaaframe group's finding, rung 8). The stateless discriminator is the
        //     tile rect's top: 0 for maTile[0] of BOTH shipped plans (KMSAA {0,0,1280,384} and
        //     KNO_MSAA {0,0,1280,720}), 384 for KMSAA's maTile[1]; a null rect (no plan) resolves.
        // The old body was `LogOnce("resolve-depth-noop", ...)` and the fact it reported was true
        // -- which is why the down-sample buffer's INTZ has never been written on ANY path, MSAA
        // or not, and why closing it here fixes the single-sampled build as well.
        if (lpRect == nullptr || lpRect->muTop == 0u)
        {
            TilingResolveDepthToTexture(lpD3DDevice, lpDestTexture);
        }
    }
    else
    {
        IDirect3DSurface9* const lpDestSurface =
            TilingAcquireDestSurface(lpDestTexture, luDestLevel, luDestSliceOrFace);
        if (lpDestSurface != nullptr && lpDestSurface != lpBoundColour)
        {
            D3DSURFACE_DESC lDestDesc;
            if (SUCCEEDED(lpDestSurface->GetDesc(&lDestDesc)))
            {
                // The source rectangle, clamped to the bound surface. A null rectangle means the
                // whole surface (no attested call site passes null, but PixelBuffer's EndTiling
                // branch shows the argument can be null in this family).
                D3DRECT lSourceRect = { 0, 0, static_cast<LONG>(luSurfaceWidth),
                                        static_cast<LONG>(luSurfaceHeight) };
                bool lbHaveSource = (luSurfaceWidth != 0u && luSurfaceHeight != 0u);
                if (lpRect != nullptr)
                {
                    lbHaveSource =
                        TilingClampRect(*lpRect, luSurfaceWidth, luSurfaceHeight, &lSourceRect);
                }

                // The destination corner: the caller's D3DPOINT, or the origin when it passes
                // null. ResolveMSAA passes maTile[tile].mu32Left / .mu32Top on the tiled path and
                // an explicitly zeroed pair on the untiled one.
                s32 liDestX = 0;
                s32 liDestY = 0;
                if (lpDestPoint != nullptr)
                {
                    const XenonResolvePoint* const lpPoint =
                        static_cast<const XenonResolvePoint*>(lpDestPoint);
                    liDestX = lpPoint->miX;
                    liDestY = lpPoint->miY;
                }

                if (lbHaveSource)
                {
                    RECT lSource;
                    lSource.left   = lSourceRect.x1;
                    lSource.top    = lSourceRect.y1;
                    lSource.right  = lSourceRect.x2;
                    lSource.bottom = lSourceRect.y2;

                    RECT lDest;
                    lDest.left   = liDestX;
                    lDest.top    = liDestY;
                    lDest.right  = liDestX + (lSourceRect.x2 - lSourceRect.x1);
                    lDest.bottom = liDestY + (lSourceRect.y2 - lSourceRect.y1);
                    if (lDest.right  > static_cast<LONG>(lDestDesc.Width))
                        lDest.right  = static_cast<LONG>(lDestDesc.Width);
                    if (lDest.bottom > static_cast<LONG>(lDestDesc.Height))
                        lDest.bottom = static_cast<LONG>(lDestDesc.Height);

                    if (lDest.right > lDest.left && lDest.bottom > lDest.top)
                    {
                        // D3DTEXF_NONE: same size, no filtering wanted. (If the scene target is
                        // ever created multisampled, this same call IS the D3D9 MSAA resolve, and
                        // D3DTEXF_NONE is the required filter for that too.)
                        //
                        // THE SCENE SUSPEND -- see the banner for WHY. The frame's scene is closed
                        // for the length of the copy and reopened immediately after it, with the
                        // failure handling (which touches the destination SURFACE) kept inside the
                        // same window.
                        //
                        // D3DERR_INVALIDCALL from EndScene means exactly one thing -- no scene was
                        // open -- so there is nothing to put back and opening one would leave it
                        // unbalanced. ANY other result is reopened, including a driver error that
                        // may have left the scene open, because every draw in the REST of the frame
                        // (the loading layers, the GUI/Apt flush, the movie quad, the HUD) needs an
                        // open scene; silently losing all of them would be worse than a redundant
                        // BeginScene, which merely returns D3DERR_INVALIDCALL and changes nothing.
                        //
                        // Two plain calls rather than an RAII guard on purpose: a guard's
                        // destructor makes MSVC emit unwind funclets and pulls __GSHandlerCheck_EH4
                        // and __std_terminate into a TU that references neither today (measured
                        // with dumpbin), and this region is straight-line code with no early exit
                        // and nothing that throws, so the guard would buy nothing for the two new
                        // link-time externals it costs.
                        const bool lbSceneSuspended =
                            (lpD3DDevice->EndScene() != D3DERR_INVALIDCALL);

                        // ⚠ WITH A MULTISAMPLED SOURCE, THIS CALL *IS* THE MSAA RESOLVE (rung 8).
                        // D3D9 resolves a multisampled render target when StretchRect copies it to
                        // a NON-multisampled surface of the SAME format at the SAME size with no
                        // filtering -- which is exactly the call below: D3DTEXF_NONE, and a
                        // destination rectangle built from the source rectangle's own width and
                        // height a few lines up (`lDest.right = liDestX + (x2 - x1)`), so there is
                        // no stretch. Both conditions are load-bearing on a multisampled source:
                        // D3D9 refuses to stretch one and refuses to filter one. The formats match
                        // too -- the anti-alias colour section and the down-sample colour section
                        // are both A8R8G8B8 (PostFxRenderTargetPCLeaf.cpp creates both).
                        const bool lbSourceIsMultisampled =
                            TilingSurfaceIsMultisampled(lpBoundColour);

                        HRESULT lHr = lpD3DDevice->StretchRect(lpBoundColour, &lSource,
                                                               lpDestSurface, &lDest,
                                                               D3DTEXF_NONE);

                        // Some drivers accept an MSAA resolve only over the WHOLE surface and not
                        // over a sub-rectangle -- and the console's MSAA plan hands this function
                        // two half-screen tiles. Rather than assume either way, the console's own
                        // sub-rectangle is tried FIRST and only a refusal on a multisampled source
                        // falls back to the full-surface resolve, loudly and once. (Nothing is
                        // widened on the single-sampled path: the console's rectangle stands.)
                        //
                        // ONLY THE FIRST TILE MAY WIDEN. Section (2) of THIS SAME call clears the
                        // tile's band of the SOURCE (the 0x300 flags carry colour AND Z), so by
                        // the time tile 1 arrives rows 0..383 of the multisampled surface are
                        // already the background colour -- a full-surface resolve for tile 1 would
                        // copy that cleared band over the good pixels tile 0's widened resolve had
                        // just written, and leave the TOP HALF OF THE SCREEN flat. Tile 0 already
                        // wrote every pixel the later tiles own, so once its widened resolve has
                        // SUCCEEDED their copy is genuinely done and only the clear is still owed;
                        // if it did not succeed, the failure path below reports it as before.
                        // (Verify round 2026-08-16: the earlier text claimed the second pass was
                        // "redundant rather than wrong" -- it was the poisoning the depth half
                        // already guards against.)
                        static bool sbMsaaFullSurfaceResolved = false;
                        if (FAILED(lHr) && lbSourceIsMultisampled)
                        {
                            LogOnce("resolve-msaa-fullrect",
                                    "[postfx-resolve] the driver refused a SUB-RECTANGLE MSAA"
                                    " resolve; falling back to a full-surface resolve on the"
                                    " FIRST tile of the bracket.\n");
                            if (lSourceRect.y1 == 0)
                            {
                                lHr = lpD3DDevice->StretchRect(lpBoundColour, nullptr,
                                                               lpDestSurface, nullptr,
                                                               D3DTEXF_NONE);
                                sbMsaaFullSurfaceResolved = SUCCEEDED(lHr);
                            }
                            else if (sbMsaaFullSurfaceResolved)
                            {
                                lHr = S_OK;   // tile 0's widened resolve already copied this band
                            }
                        }

                        if (FAILED(lHr))
                        {
                            ++suResolveCopyFailureStreak;

                            // The one cap that governs this copy: both surfaces here are
                            // render-target TEXTURE surfaces, and the D3D9 reference makes texture
                            // sources/destinations conditional on
                            // D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES. Reporting it beside the hr
                            // turns "StretchRect failed" into an answerable question.
                            D3DCAPS9 lCaps;
                            std::memset(&lCaps, 0, sizeof(lCaps));
                            const bool lbFromTextures =
                                SUCCEEDED(lpD3DDevice->GetDeviceCaps(&lCaps))
                                && (lCaps.DevCaps2
                                    & static_cast<DWORD>(
                                          D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) != 0u;

                            char lacMessage[256];
                            std::snprintf(lacMessage, sizeof(lacMessage),
                                          "[postfx-resolve] StretchRect FAILED hr=0x%08X"
                                          " (CAN_STRETCHRECT_FROM_TEXTURES=%u) -- the down-sample"
                                          " buffer is NOT being written; the clear is withheld and"
                                          " the destination gets the tripwire colour",
                                          static_cast<unsigned>(lHr),
                                          lbFromTextures ? 1u : 0u);
                            LogRepeating(suResolveCopyFailures, lacMessage);

                            // ⚠ THE COPY-FAILURE TRIPWIRE (banner). Withholding the clear fixes
                            // what the SOURCE looks like, but what reaches the screen is the
                            // DESTINATION -- and a destination nothing ever wrote is a
                            // D3DPOOL_DEFAULT render-target texture holding undefined contents,
                            // which in practice reads as black. A permanent copy failure would
                            // therefore still present as a CLEAN BLACK FRAME, indistinguishable
                            // from "the bracket did not work" and from a dozen other faults. So
                            // once the copy has failed KU_RESOLVE_TRIPWIRE_FRAMES times in a row
                            // the destination is filled with a colour nothing in this game
                            // produces. ColorFill is legal on "a render target, a render-target
                            // texture surface, or an off-screen plain surface with a pool type of
                            // D3DPOOL_DEFAULT" (D3D9 reference) -- exactly what this is.
                            if (suResolveCopyFailureStreak >= KU_RESOLVE_TRIPWIRE_FRAMES)
                            {
                                lpD3DDevice->ColorFill(lpDestSurface, nullptr,
                                                       D3DCOLOR_ARGB(255, 255, 0, 255));
                            }
                        }
                        else
                        {
                            suResolveCopyFailureStreak = 0u;
                        }

                        if (lbSceneSuspended)
                        {
                            const HRESULT lHrScene = lpD3DDevice->BeginScene();
                            // D3DERR_INVALIDCALL here means a scene is ALREADY open (the EndScene
                            // above did not close one after all) -- precisely the state wanted, so
                            // it is not a failure.
                            if (FAILED(lHrScene) && lHrScene != D3DERR_INVALIDCALL)
                            {
                                LogRepeating(suResolveSceneReopenFails,
                                             "[postfx-resolve] BeginScene FAILED after the resolve"
                                             " copy -- every draw in the REST of this frame (GUI,"
                                             " movie, HUD) is being dropped");
                            }
                        }

                        lbCopyDone = SUCCEEDED(lHr);
                    }
                }
            }
        }
        if (lpDestSurface != nullptr)
            lpDestSurface->Release();   // GetSurfaceLevel AddRefs
    }

    // ---- (2) THE CLEAR -----------------------------------------------------------------------
    // Only the 0x300 variant carries it, and only that variant is handed a clear colour.
    //
    // ⚠ AND IT IS CONDITIONAL ON THE COPY -- see THE COPY AND THE CLEAR ARE COUPLED in the banner.
    // Clearing whether or not the copy landed is what turns a failed copy from a VISIBLE fault
    // into an invisible one: the source is wiped, the destination is never written, and the screen
    // goes cleanly black forever. Withholding it leaves the world accumulating in the scene target
    // instead, which is a fault you can see.
    if ((luFlags & KU_RESOLVE_CLEAR_BITS) != 0u)
    {
        if (lbCopyRequired && !lbCopyDone)
        {
            LogRepeating(suResolveClearWithheld,
                         "[postfx-resolve] the colour copy did not happen, so the 0x300 clear is"
                         " WITHHELD -- the scene target keeps what it holds (a visible smear)"
                         " rather than being wiped to the background colour (a silent black)");
        }
        else
        {
            TilingClearBoundSurfaces(lpD3DDevice, lpBoundColour, luSurfaceWidth, luSurfaceHeight,
                                     lpRect, (lpRect != nullptr) ? 1u : 0u,
                                     lpClearColour, lfClearZ, luClearStencil,
                                     /*lbExpectFullCoverage*/ false);   // one band of the plan
        }
    }

    lpBoundColour->Release();   // GetRenderTarget AddRefs
    return 0;                   // unobservable: no attested caller reads it (see the banner)
}

// =================================================================================================
// PCStampMotionBlurMask -- THE MOTION-BLUR MASK'S PC CARRIER.
//
// WHAT THE CONSOLE DOES, and it is fully recovered (drive-fx wave, step 10): the per-pixel
// motion-blur mask IS the STENCIL BUFFER. BrnRendererModule::Render quantises the layer-0
// effects frame at @0x8240C2C0-0x8240C314 into worldByte = (u8)(mfWorldBlurAmount*255) and
// carsByte = (u8)(mfCarsBlurAmount*255); worldByte is the frame's stencil CLEAR (through
// D3DDevice_BeginTiling's / D3DDevice_EndTiling's ClearStencil), and carsByte is REPLACED into
// the stencil over the two car mesh lists by shadow::Device::BeginForceStencilWrite /
// EndForceStencilWrite (Render @0x8240CEC4 and @0x8240D338; the override itself is
// shadowingdevice.cpp:746-754, StencilFunc ALWAYS / StencilPass REPLACE / ref = the byte).
// The composite then samples the depth-stencil as A8R8G8B8 and MULTIPLIES THE SCREEN VELOCITY
// BY THE STENCIL LANE. Both halves of the producer are already live on PC.
//
// THE PC PROBLEM IS THE CONSUMER, AND IT IS ONE SENTENCE: Direct3D 9 cannot SAMPLE the stencil
// plane of a depth-stencil surface (INTZ / DF24 / DF16 return the depth VALUE in .r and nothing
// else). It CAN still TEST it. So the mask is MOVED, not reinvented: two full-screen
// ALPHA-ONLY quads, stencil-TESTED against the same two bytes, copy the console's stencil
// content into the scene target's ALPHA LANE while that target and its own D24S8 depth-stencil
// are still bound; the resolve carries all four lanes out (StretchRect), and the eight blur
// permutations read `tex2D(SamplerSource, uv).a` at the unjittered uv
// (tools/assets/shaders/brn_postfx_composite.fx). The VALUE the shader multiplies by is
// bit-for-bit the console's: the quad's diffuse ALPHA BYTE is the stencil byte itself, and an
// A8R8G8B8 target stores that byte unchanged, so the shader's `.a` is stencil/255 exactly.
//
// ⚠ WHY NOT THE OBVIOUS THING -- writing the mask into alpha FROM THE CAR DRAWS THEMSELVES,
// with D3DRS_SEPARATEALPHABLENDENABLE + SRCBLENDALPHA = BLENDFACTOR and the forced byte in
// D3DRS_BLENDFACTOR's alpha. IT IS ARITHMETICALLY WRONG, and not marginally. D3D9's alpha
// blend is A_out = A_src * SrcBlendAlpha + A_dst * DestBlendAlpha, so BLENDFACTOR/ZERO gives
//     A_out = A_src * carsByte/255
// -- the SHADER'S OWN alpha output times the byte, not the byte. There is no assignment of
// D3D9 blend factors that yields a constant independent of A_src and A_dst (every factor is
// either a constant MULTIPLIER of one of them or zero, and the equation has no constant term).
// That reduces to the console's REPLACE only where A_src == 1, and one of the two force
// windows is the CAR TRANSPARENT list (GDL 0x14), drawn by RenderWorldPasses under
// SetWorldPassDefaultStates(true) -- SRCALPHA/INVSRCALPHA blending, i.e. glass, whose whole
// point is A_src < 1. The console stamps carsByte on a windscreen regardless of how
// transparent it is; that scheme would stamp carsByte * glassAlpha. Nothing in the image
// attests what any car pixel program writes to alpha (the console never reads the scene's
// alpha lane), so "A_src is 1 for the opaque list" would be an assumption, not a recovery.
// The stencil test needs no such assumption: it asks the buffer the console actually wrote.
//
// TWO MORE THINGS THE STENCIL TEST BUYS, both of which the per-draw scheme costs:
//   * ZERO CHANGE TO THE WORLD PASS. Nothing about any material's colour-write word, blend
//     word or blend factor is touched, so a material that blends its colour on DEST_ALPHA
//     reads exactly what it read before this wave (the census in the blend-state leaf above
//     counts how many do). The per-draw scheme has to mask the alpha write bit for the whole
//     world pass, which CHANGES what such a material reads.
//   * ALPHA-TO-COVERAGE IS UNTOUCHED (rung 9). A2C derives coverage from the fragment's alpha
//     BEFORE blending, so a scheme that leaves the shader alpha alone is safe -- but only for
//     the DRAWS. These quads write an authored alpha and would be eaten by their own coverage,
//     which is what sbMaskStampActive (top of this file) makes impossible.
//
// WHAT IT COSTS: two full-screen alpha-only draws with no texture, no depth test and no
// blending -- ROP traffic and nothing else -- and ONLY on frames where a camera actually
// requested motion blur (the caller gates on mMotionBlurData.mbIsActive, exactly as the
// console gates its own force windows). One quad when the two amounts are equal.
//
// TWO HONEST LIMITS, both reported rather than hidden:
//   (1) NO STENCIL, NO DISCRIMINATION. The scene target's depth format is picked by a ladder
//       (PostFxRenderTargetPCLeaf.cpp): the MULTISAMPLED path takes D3DFMT_D24S8 first, so the
//       stencil is real; the single-sampled path takes the COMPARE ladder, whose first
//       candidate D3DFMT_D24X8 has NO stencil bits -- and there the console's force-stencil
//       write is already a no-op too. When the bound depth carries no stencil this writes the
//       WORLD byte everywhere in ONE quad and says so once. That is still strictly better than
//       what the composite did before this wave (a hard-coded 1.0, i.e. every pixel blurred at
//       FULL strength no matter what the camera asked for), because
//       BrnPostFxShader::Render never scales BlurMatrixX/Y/W by the amount.
//   (2) A THIRD STENCIL VALUE would come out as the world amount. The console reads the
//       stencil BYTE, so any value/255 is a legal mask; this pair of tests can only express
//       two. On the shipped frame graph there are exactly two -- the clear and the car
//       replace -- and a material that wrote its own stencil inside a non-force window would
//       be getting a mask the game never intended anyway, so the world amount is the sane
//       reading. Named here so nobody has to rediscover it.
//
// NOT A "DELETE-WHEN" SEAM. It stands in for no console function: it is the D3D9 realisation
// of a console mechanism whose original form the API cannot express. What retires it is a
// BACKEND with a stencil SRV (D3D10+), not more reconstruction.
// =================================================================================================
namespace
{
    // The stamp's vertex: a pre-transformed screen corner plus a diffuse colour whose ALPHA
    // byte IS the console's stencil byte. D3DFVF_XYZRHW bypasses vertex processing entirely
    // (no shader, no transform), which is what makes this a fixed-function draw that cannot
    // disturb the world's constant file.
    struct MaskStampVertex
    {
        f32      mfX;
        f32      mfY;
        f32      mfZ;
        f32      mfRhw;
        D3DCOLOR mDiffuse;
    };
    const DWORD KU_MASK_STAMP_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

    // Every render state the stamp overwrites, saved and put back in one place so the list
    // cannot drift away from the SetRenderState calls below.
    const D3DRENDERSTATETYPE KAE_MASK_STAMP_SAVED_STATES[] =
    {
        D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,  D3DRS_ZENABLE,
        D3DRS_ZWRITEENABLE,     D3DRS_CULLMODE,         D3DRS_SCISSORTESTENABLE,
        D3DRS_COLORWRITEENABLE, D3DRS_FILLMODE,         D3DRS_SRGBWRITEENABLE,
        D3DRS_MULTISAMPLEMASK,  D3DRS_STENCILENABLE,    D3DRS_TWOSIDEDSTENCILMODE,
        D3DRS_STENCILFUNC,      D3DRS_STENCILREF,       D3DRS_STENCILMASK,
        D3DRS_STENCILWRITEMASK, D3DRS_STENCILFAIL,      D3DRS_STENCILZFAIL,
        D3DRS_STENCILPASS
    };
    const u32 KU_MASK_STAMP_SAVED_STATE_COUNT =
        static_cast<u32>(sizeof(KAE_MASK_STAMP_SAVED_STATES)
                         / sizeof(KAE_MASK_STAMP_SAVED_STATES[0]));

    u32 suMaskStampDrawFailures = 0u;
}

void PCStampMotionBlurMask(u32 luCarsBlurStencil, u32 luWorldBlurStencil)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr)
        return;

    u32 luSurfaceWidth  = 0u;
    u32 luSurfaceHeight = 0u;
    IDirect3DSurface9* const lpBoundColour =
        TilingAcquireBoundColour(lpDevice, &luSurfaceWidth, &luSurfaceHeight);
    if (lpBoundColour == nullptr || luSurfaceWidth == 0u || luSurfaceHeight == 0u)
    {
        LogOnce("mask-alpha-nort",
                "[mask-alpha] no colour render target is bound; the motion-blur mask stamp is"
                " skipped and the composite reads whatever the alpha lane holds.\n");
        if (lpBoundColour != nullptr)
            lpBoundColour->Release();
        return;
    }

    // The same misordering tripwire D3DDevice_Resolve carries, for the same reason: this writes
    // into the surface that is BOUND, so running it after the present blit would stamp the
    // finished frame's alpha instead of the scene target's.
    if (TilingBoundColourIsBackBuffer(lpDevice, lpBoundColour))
    {
        LogOnce("mask-alpha-backbuffer",
                "[mask-alpha] the SWAP CHAIN is bound -- refusing to stamp the motion-blur mask"
                " into the finished frame. Call PCStampMotionBlurMask while the scene target is"
                " still bound, i.e. immediately before ResolveMSAA.\n");
        lpBoundColour->Release();
        return;
    }

    // Can the bound depth actually TELL THE TWO HALVES APART? Asked of the surface the device
    // holds right now, through the same predicate the tiling clear uses, so a format ladder
    // change cannot silently strand this. See LIMIT (1) in the banner.
    bool lbHaveStencil  = false;
    u32  luDepthFormat  = 0u;
    IDirect3DSurface9* lpDepthSurface = nullptr;
    if (SUCCEEDED(lpDevice->GetDepthStencilSurface(&lpDepthSurface)) && lpDepthSurface != nullptr)
    {
        D3DSURFACE_DESC lDepthDesc;
        std::memset(&lDepthDesc, 0, sizeof(lDepthDesc));
        if (SUCCEEDED(lpDepthSurface->GetDesc(&lDepthDesc)))
        {
            luDepthFormat = static_cast<u32>(lDepthDesc.Format);
            lbHaveStencil = TilingDepthFormatHasStencil(lDepthDesc.Format);
        }
        lpDepthSurface->Release();   // GetDepthStencilSurface AddRefs
    }

    const u32  luCarsByte  = luCarsBlurStencil  & 0xFFu;
    const u32  luWorldByte = luWorldBlurStencil & 0xFFu;
    const bool lbTwoQuads  = lbHaveStencil && (luCarsByte != luWorldByte);

    // ---- SAVE ------------------------------------------------------------------------------
    // This BORROWS the device in the middle of a frame and its caller has no idea it happened,
    // so everything it touches is captured here and restored below -- the same contract
    // PCBringUpClearRenderTargetState and TilingResolveDepthToTexture keep.
    DWORD lauSavedState[KU_MASK_STAMP_SAVED_STATE_COUNT];
    for (u32 luState = 0; luState < KU_MASK_STAMP_SAVED_STATE_COUNT; ++luState)
    {
        if (FAILED(lpDevice->GetRenderState(KAE_MASK_STAMP_SAVED_STATES[luState],
                                            &lauSavedState[luState])))
        {
            lauSavedState[luState] = 0u;
        }
    }

    IDirect3DVertexShader9*      lpSavedVs   = nullptr;
    IDirect3DPixelShader9*       lpSavedPs   = nullptr;
    IDirect3DVertexDeclaration9* lpSavedDecl = nullptr;
    DWORD                        luSavedFvf  = 0u;
    lpDevice->GetVertexShader(&lpSavedVs);          // AddRefs on success
    lpDevice->GetPixelShader(&lpSavedPs);           // AddRefs on success
    lpDevice->GetVertexDeclaration(&lpSavedDecl);   // AddRefs on success
    lpDevice->GetFVF(&luSavedFvf);

    DWORD luSavedColourOp = 0u, luSavedColourArg1 = 0u;
    DWORD luSavedAlphaOp  = 0u, luSavedAlphaArg1  = 0u;
    DWORD luSavedStage1ColourOp = 0u;
    lpDevice->GetTextureStageState(0u, D3DTSS_COLOROP,   &luSavedColourOp);
    lpDevice->GetTextureStageState(0u, D3DTSS_COLORARG1, &luSavedColourArg1);
    lpDevice->GetTextureStageState(0u, D3DTSS_ALPHAOP,   &luSavedAlphaOp);
    lpDevice->GetTextureStageState(0u, D3DTSS_ALPHAARG1, &luSavedAlphaArg1);
    lpDevice->GetTextureStageState(1u, D3DTSS_COLOROP,   &luSavedStage1ColourOp);

    D3DVIEWPORT9 lSavedViewport;
    std::memset(&lSavedViewport, 0, sizeof(lSavedViewport));
    const bool lbSavedViewport = SUCCEEDED(lpDevice->GetViewport(&lSavedViewport));

    const bool lbA2cWasApplied = sbA2cApplied;

    // ---- SET -------------------------------------------------------------------------------
    // Alpha-to-coverage OFF first: it is the one piece of state that would consume the very
    // value these quads exist to write (see sbMaskStampActive's note at the top of this file).
    sbMaskStampActive = true;
    AlphaCoverage_Reconcile();

    lpDevice->SetVertexShader(nullptr);
    lpDevice->SetPixelShader(nullptr);
    lpDevice->SetFVF(KU_MASK_STAMP_FVF);   // also clears any bound vertex declaration

    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,  FALSE);
    lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE,   FALSE);
    lpDevice->SetRenderState(D3DRS_ZENABLE,           D3DZB_FALSE);
    lpDevice->SetRenderState(D3DRS_ZWRITEENABLE,      FALSE);
    lpDevice->SetRenderState(D3DRS_CULLMODE,          D3DCULL_NONE);
    lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    lpDevice->SetRenderState(D3DRS_FILLMODE,          D3DFILL_SOLID);
    lpDevice->SetRenderState(D3DRS_SRGBWRITEENABLE,   FALSE);
    // EVERY SAMPLE, NOT SOME. The world's rasteriser states push their own
    // D3DRS_MULTISAMPLEMASK (shadowingdevice.cpp's rasteriser applier), and a mask left with a
    // sample switched off would leave that sample's alpha at the world's leftovers -- which the
    // resolve would then average into the mask. Forced and restored.
    lpDevice->SetRenderState(D3DRS_MULTISAMPLEMASK,   0xFFFFFFFFu);
    // THE ALPHA LANE AND NOTHING ELSE. This is what makes the stamp free of side effects: the
    // scene colour the world just spent the frame producing is untouchable while it runs.
    lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE,  D3DCOLORWRITEENABLE_ALPHA);

    // Fixed-function, texture-free: alpha comes from the vertex colour and nothing else. Stage
    // 1 is disabled so a stage the 2D tail left enabled cannot make the draw invalid.
    lpDevice->SetTextureStageState(0u, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    lpDevice->SetTextureStageState(0u, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    lpDevice->SetTextureStageState(0u, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    lpDevice->SetTextureStageState(0u, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    lpDevice->SetTextureStageState(1u, D3DTSS_COLOROP,   D3DTOP_DISABLE);

    // The quad is authored in the surface's own pixel coordinates, so the viewport has to be
    // the whole surface (the world pass may have left a partial one). MinZ/MaxZ are carried
    // across untouched -- the same rule TilingClearBoundSurfaces follows.
    bool lbForcedViewport = false;
    if (lbSavedViewport)
    {
        D3DVIEWPORT9 lFullViewport;
        lFullViewport.X      = 0u;
        lFullViewport.Y      = 0u;
        lFullViewport.Width  = static_cast<DWORD>(luSurfaceWidth);
        lFullViewport.Height = static_cast<DWORD>(luSurfaceHeight);
        lFullViewport.MinZ   = lSavedViewport.MinZ;
        lFullViewport.MaxZ   = lSavedViewport.MaxZ;
        lbForcedViewport = SUCCEEDED(lpDevice->SetViewport(&lFullViewport));
    }

    // The stencil is READ, never written: every op is KEEP and the write mask is zero, so the
    // console's own stencil content survives the stamp untouched (the resolve's 0x300 clear is
    // what resets it, exactly as before).
    lpDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
    lpDevice->SetRenderState(D3DRS_STENCILFAIL,      D3DSTENCILOP_KEEP);
    lpDevice->SetRenderState(D3DRS_STENCILZFAIL,     D3DSTENCILOP_KEEP);
    lpDevice->SetRenderState(D3DRS_STENCILPASS,      D3DSTENCILOP_KEEP);
    lpDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0u);
    lpDevice->SetRenderState(D3DRS_STENCILMASK,      0xFFu);
    lpDevice->SetRenderState(D3DRS_STENCILREF,       static_cast<DWORD>(luCarsByte));

    // ---- DRAW ------------------------------------------------------------------------------
    // The half-pixel offset is the D3D9 pre-transformed-vertex rule: a quad from -0.5 to
    // extent-0.5 covers pixel centres 0..extent-1 exactly, with no edge pixel left out and none
    // sampled twice.
    const f32 lfLeft   = -0.5f;
    const f32 lfTop    = -0.5f;
    const f32 lfRight  = static_cast<f32>(luSurfaceWidth)  - 0.5f;
    const f32 lfBottom = static_cast<f32>(luSurfaceHeight) - 0.5f;

    MaskStampVertex laQuad[4];
    laQuad[0].mfX = lfLeft;   laQuad[0].mfY = lfTop;
    laQuad[1].mfX = lfRight;  laQuad[1].mfY = lfTop;
    laQuad[2].mfX = lfLeft;   laQuad[2].mfY = lfBottom;
    laQuad[3].mfX = lfRight;  laQuad[3].mfY = lfBottom;
    for (u32 luVertex = 0; luVertex < 4u; ++luVertex)
    {
        laQuad[luVertex].mfZ   = 0.0f;
        laQuad[luVertex].mfRhw = 1.0f;
    }

    HRESULT lHrDraw = S_OK;
    u32     luQuads = 0u;

    // Pass 1: the CAR silhouette -- the pixels whose stencil the force windows REPLACED.
    // Skipped entirely when there is nothing to discriminate (no stencil, or the two amounts
    // are equal), in which case pass 2 below covers the whole surface with the world value.
    if (lbTwoQuads)
    {
        lpDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        lpDevice->SetRenderState(D3DRS_STENCILFUNC,   D3DCMP_EQUAL);
        const D3DCOLOR lCarsColour = D3DCOLOR_ARGB(static_cast<int>(luCarsByte), 0, 0, 0);
        for (u32 luVertex = 0; luVertex < 4u; ++luVertex)
            laQuad[luVertex].mDiffuse = lCarsColour;
        const HRESULT lHr = lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2u, laQuad,
                                                      sizeof(MaskStampVertex));
        if (FAILED(lHr))
            lHrDraw = lHr;
        ++luQuads;
    }

    // Pass 2: everything else -- the frame's stencil CLEAR value, i.e. the world amount.
    lpDevice->SetRenderState(D3DRS_STENCILENABLE, lbTwoQuads ? TRUE : FALSE);
    lpDevice->SetRenderState(D3DRS_STENCILFUNC,   D3DCMP_NOTEQUAL);
    {
        const D3DCOLOR lWorldColour = D3DCOLOR_ARGB(static_cast<int>(luWorldByte), 0, 0, 0);
        for (u32 luVertex = 0; luVertex < 4u; ++luVertex)
            laQuad[luVertex].mDiffuse = lWorldColour;
        const HRESULT lHr = lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2u, laQuad,
                                                      sizeof(MaskStampVertex));
        if (FAILED(lHr))
            lHrDraw = lHr;
        ++luQuads;
    }

    // ---- RESTORE ---------------------------------------------------------------------------
    if (lbForcedViewport)
        lpDevice->SetViewport(&lSavedViewport);

    lpDevice->SetTextureStageState(1u, D3DTSS_COLOROP,   luSavedStage1ColourOp);
    lpDevice->SetTextureStageState(0u, D3DTSS_ALPHAARG1, luSavedAlphaArg1);
    lpDevice->SetTextureStageState(0u, D3DTSS_ALPHAOP,   luSavedAlphaOp);
    lpDevice->SetTextureStageState(0u, D3DTSS_COLORARG1, luSavedColourArg1);
    lpDevice->SetTextureStageState(0u, D3DTSS_COLOROP,   luSavedColourOp);

    if (lpSavedDecl != nullptr)
        lpDevice->SetVertexDeclaration(lpSavedDecl);
    else
        lpDevice->SetFVF(luSavedFvf);       // nothing had a declaration: restore the FVF
    lpDevice->SetPixelShader(lpSavedPs);
    lpDevice->SetVertexShader(lpSavedVs);

    for (u32 luState = 0; luState < KU_MASK_STAMP_SAVED_STATE_COUNT; ++luState)
        lpDevice->SetRenderState(KAE_MASK_STAMP_SAVED_STATES[luState], lauSavedState[luState]);

    sbMaskStampActive = false;
    AlphaCoverage_Reconcile();   // the inputs are back; follow them

    // [DIAG, mask-alpha] a stamp COMPLETED on this frame -- consumed by the next composite draw
    // (see the declaration's note). Raised after the draws and the restore, so it cannot claim a
    // stamp the device refused; hr is reported separately below.
    sbMaskStampedSinceComposite = true;

    if (lpSavedDecl != nullptr) lpSavedDecl->Release();
    if (lpSavedPs   != nullptr) lpSavedPs->Release();
    if (lpSavedVs   != nullptr) lpSavedVs->Release();
    lpBoundColour->Release();    // GetRenderTarget AddRefs

    // ---- REPORT ----------------------------------------------------------------------------
    // ONE line, on the first stamp of the run: it answers the three questions a wrong mask
    // raises -- which two bytes went in, whether the stencil could discriminate them at all,
    // and whether alpha-to-coverage was live at the moment they were written.
    // ⚠ NOT LogOnce ANY MORE, and the reason is a wave that was lost to the old line. `quads` and
    // the two bytes are per-FRAME facts: the first stamp of a run is the junkyard entry, where
    // mfCarsBlurAmount == mfWorldBlurAmount == 1.0, so lbTwoQuads is false and the frozen line
    // says `quads=1` forever -- which reads like a refused draw and is nothing of the kind. Later
    // frames genuinely stamp TWO quads (run 20260820_101433: `[postfx-cam] produce 4500 ... mb=1
    // cars=0.000000 world=1.000000`, and the composite's own read-back on that frame,
    // `[mask-alpha] source alpha ... composite draws 4500 centre=0 (0.000) quarter=255 (1.000)`,
    // is the cars-vs-world split arriving in the lane). So the line is re-printed whenever the
    // tuple it describes changes, capped so a ramping amount cannot flood the log.
    {
        static u32 suLastStampKey = 0xFFFFFFFFu;
        static u32 suStampPrints  = 0u;
        const u32  luStampKey = (luCarsByte << 16) | (luWorldByte << 8)
                              | (luQuads << 1) | (lbHaveStencil ? 1u : 0u);
        if (luStampKey != suLastStampKey && suStampPrints < 12u)
        {
            suLastStampKey = luStampKey;
            ++suStampPrints;
            char lacMessage[384];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[mask-alpha] stamp: cars=%u world=%u carrier=scene-alpha stencil=%s"
                          " depthFmt=0x%08X a2c=%s quads=%u target=%ux%u hr=0x%08X\n",
                          static_cast<unsigned>(luCarsByte),
                          static_cast<unsigned>(luWorldByte),
                          lbHaveStencil ? "yes" : "NO (world amount everywhere)",
                          static_cast<unsigned>(luDepthFormat),
                          lbA2cWasApplied ? "on (suppressed for the stamp)" : "off",
                          static_cast<unsigned>(luQuads),
                          static_cast<unsigned>(luSurfaceWidth),
                          static_cast<unsigned>(luSurfaceHeight),
                          static_cast<unsigned>(lHrDraw));
            CgsDev::Log::WriteToLog(lacMessage);
        }
    }

    if (FAILED(lHrDraw))
    {
        LogRepeating(suMaskStampDrawFailures,
                     "[mask-alpha] the mask stamp's DrawPrimitiveUP was REJECTED -- the scene"
                     " target's alpha lane keeps whatever the world left, so the composite's"
                     " motion blur is masked by garbage");
    }
}

// =============================================================================
// FLAG PC bring-up INITIALISATION: give a freshly created off-screen target DEFINED contents
// before anything renders into it. THERE IS NO CONSOLE COUNTERPART, and there is no console
// counterpart for a reason worth stating rather than hiding.
//
// WHY THE FIRST FRAME NEEDS IT. On the Xenos the scene target is EDRAM and the TILING PASS
// clears it at the TOP of every frame, the first one included -- that is exactly what
// D3DDevice_BeginTiling's pClearColor / ClearZ / stencil arguments above are for. The PC
// bracket's only clear is the one at the BOTTOM of the frame: the 0x300 D3DDevice_Resolve
// clears the scene target BEHIND the copy, which is what leaves it clean for the NEXT frame's
// world pass. That is faithful -- and it is also why the FIRST frame after these targets are
// created has been cleared by nothing at all.
//
// D3D9 leaves a freshly created D3DPOOL_DEFAULT render-target texture and depth texture
// UNDEFINED, and nothing at creation writes them: in PostFxRenderTargetPCLeaf.cpp the colour
// texture (:586-592) and the depth texture (:229-237) are created and handed straight out --
// `grep -n "Clear(\|ColorFill" pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp` returns
// NOTHING (run this round). So without this call the bracket's first frame renders the world
// into undefined colour over undefined depth, and that frame is copied and PRESENTED. The
// depth half is the worse one: undefined depth can reject every pixel of the world, which on
// screen reads as "the bracket does not work" rather than as one bad frame.
//
// THE VALUES ARE ATTESTED, and not one of them is a placeholder zero:
//   * RGB black and Z 1.0f are exactly what this build's own frame-start clear puts in the
//     back buffer today -- renderengine::Device::FrameBegin issues
//     `Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0)`
//     (device.cpp:117). Starting the off-screen target from the SAME slate the back buffer
//     starts from is what makes the flip's first frame look like the first frame before it --
//     this step's entire success criterion.
//   * 1.0f is independently the console's own ClearZ for this bracket: flt_82001C98 = 1.0f
//     (DATA_DUMP.md, +0x0000), the constant BeginTiling is handed at 0x823FFB54.
//   * alpha 0.0f is the console's KF_BACKGROUND_COLOUR_ALPHA (flt_82001CC0 = 0.0f, DATA_DUMP.md
//     +0x0000) -- the alpha the per-frame scene clear writes. The blit ignores alpha
//     (SELECTARG1 + blending off, see PCSceneBlit_Begin), so it is unobservable either way;
//     this matches the recovered value instead of picking one.
//   * stencil 0 -- device.cpp:117 again, and the console's own `li r8, 0`.
//
// IT IS DELIBERATELY NOT THE PER-FRAME BACKGROUND COLOUR (lfWhiteLevel * 0.72/0.83/0.89, X360
// flt_820473A4/A8/AC). That value lives in BrnRendererModule's per-frame shader-constants block
// and does not exist at pool-creation time; reaching for it here would be inventing a console
// behaviour that has no counterpart at this point in the program. The consequence is bounded
// and worth knowing: on the FIRST bracket frame only, a pixel that neither the world, the sky
// dome nor the 2D tail covers is black rather than the pale background colour it will be from
// frame 2 on. Black is what that pixel is TODAY, without the bracket.
//
// The clear goes through TilingClearBoundSurfaces -- the SAME clear the resolve uses -- so the
// viewport/scissor bracket and the D3DCLEAR_STENCIL all-or-nothing retry exist exactly once.
//
// No back-buffer refusal here (unlike D3DDevice_Resolve, which runs every frame at a point
// where the finished frame is on the swap chain): this runs ONCE, at pool creation, from
// EnsurePostFxSceneTargets -- i.e. just after Device::FrameBegin cleared the back buffer to
// that same black -- and its two callers hand it the two CREATE-mode scene targets, which own
// their own surfaces and are never the swap chain. If the colour texture failed to create, the
// [postfx-rt] line at PostFxRenderTargetPCLeaf.cpp already reports `colourTexture=NULL` at that
// same moment; this function does not re-report it.
//
// DELETE IT when a frame-TOP clear exists again -- the multisampled path's D3DDevice_BeginTiling
// clear, or BrnPostFx::Render @0x8240A468's own composite -- NOT merely when the bracket lands.
// =============================================================================
void PCBringUpClearRenderTargetState(const RenderTargetState* lpState)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpState == nullptr)
        return;

    // This BORROWS the device: it runs from the middle of a frame (EnsurePostFxSceneTargets,
    // inside Render) and its caller has no idea it happened, so everything it touches is saved
    // here and put back below.
    IDirect3DSurface9* lpSavedColour = nullptr;
    if (FAILED(lpDevice->GetRenderTarget(0u, &lpSavedColour)))
        lpSavedColour = nullptr;                    // GetRenderTarget AddRefs on success
    IDirect3DSurface9* lpSavedDepth = nullptr;
    if (FAILED(lpDevice->GetDepthStencilSurface(&lpSavedDepth)))
        lpSavedDepth = nullptr;                     // D3DERR_NOTFOUND == nothing bound

    D3DVIEWPORT9 lSavedViewport;
    std::memset(&lSavedViewport, 0, sizeof(lSavedViewport));
    const bool lbSavedViewport = SUCCEEDED(lpDevice->GetViewport(&lSavedViewport));

    RECT lSavedScissor;
    std::memset(&lSavedScissor, 0, sizeof(lSavedScissor));
    const bool lbSavedScissor = SUCCEEDED(lpDevice->GetScissorRect(&lSavedScissor));

    // Bind the target's own surfaces through the engine's bind, not a raw one, so a depth-only
    // state still gets a legal colour attachment (Device::SetState owns that rule).
    renderengine::Device::SetState(lpState);

    u32 luWidth  = 0u;
    u32 luHeight = 0u;
    IDirect3DSurface9* const lpBoundColour = TilingAcquireBoundColour(lpDevice, &luWidth, &luHeight);

    // The four lanes are R, G, B, A in that order (TilingColourFromFloat4 above).
    const f32 lafClearColourRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const f32 lfClearZ              = 1.0f;

    TilingClearBoundSurfaces(lpDevice, lpBoundColour, luWidth, luHeight,
                             nullptr, 0u, lafClearColourRGBA, lfClearZ, 0u,
                             /*lbExpectFullCoverage*/ true);

    if (lpBoundColour != nullptr)
    {
        lpBoundColour->Release();   // GetRenderTarget AddRefs
    }
    else
    {
        // Nothing on colour slot 0 after the bind means the clear just did depth only (or
        // nothing at all). Silence here would be the exact undefined-first-frame this call
        // exists to remove, so it is reported.
        LogOnce("bringup-clear-no-colour",
                "[postfx-bringup] the scene target's colour surface is not bound after"
                " Device::SetState -- the one-shot creation clear cleared no colour, so the"
                " first bracket frame renders into UNDEFINED colour\n");
    }

    // Hand the device back exactly as it was found. Depth is dropped first because D3D9 checks
    // "the depth surface is at least as large as the render target" on SetRenderTarget -- the
    // same ordering renderengine::Device::SetState itself uses -- and the viewport/scissor go
    // back LAST because SetRenderTarget resets both.
    lpDevice->SetDepthStencilSurface(nullptr);
    if (lpSavedColour != nullptr)
        lpDevice->SetRenderTarget(0u, lpSavedColour);
    lpDevice->SetDepthStencilSurface(lpSavedDepth);   // null is a valid unbind
    if (lbSavedViewport)
        lpDevice->SetViewport(&lSavedViewport);
    if (lbSavedScissor)
        lpDevice->SetScissorRect(&lSavedScissor);

    // Those binds went STRAIGHT to D3D9, so the engine's "last render-target state installed"
    // shadow did not see them -- and a shadow that lies causes a SKIPPED bind, which is what
    // put the shadow cascades in the back buffer. Invalidate, exactly as PCSurfaceBracket_
    // Restore and PCSceneBlit_Begin do.
    gpLastRenderTargetState = nullptr;

    if (lpSavedColour != nullptr)
        lpSavedColour->Release();
    if (lpSavedDepth != nullptr)
        lpSavedDepth->Release();
}
}

// =============================================================================
// The Xenon predicated-draw (conditional rendering) extension the occlusion-cull
// manager brackets its mesh draws with (CgsXboxConditionalRenderShims.h declares
// them; CgsOcclusionCullManager.cpp @0x827E9130 / @0x827E9190 calls them). The
// X360 reads the device from off_83271608, the same global as gpD3DDevice above.
//
// FLAG PC-platform leaf: D3DDevice_Begin/EndConditionalRendering are an Xbox 360
// XDK D3D9 extension (GPU-side predicated draw keyed on an occlusion-query id).
// Direct3D 9 on PC has no equivalent -- predication arrived with D3D10. The
// bring-up therefore runs the draws UNPREDICATED, which is conservatively
// correct (it draws geometry the console would have skipped, never the
// opposite). Both occlusion switches (mbOcclusionCullWorldOpaque et al.) are off
// on the PC render path, so these are not on the live world-opaque walk.
namespace CgsGraphics
{
    void* gpXboxD3DDevice = nullptr;

    void D3DDevice_BeginConditionalRendering(void* /*lpDevice*/, u32 /*luIdentifier*/)
    {
        gpXboxD3DDevice = renderengine::gDevice;
    }

    void D3DDevice_EndConditionalRendering(void* /*lpDevice*/)
    {
        gpXboxD3DDevice = renderengine::gDevice;
    }
}

// ---------------------------------------------------------------------------
// XGRegisterVertexShader / XGRegisterPixelShader
//
// The Xenon XDK microcode-registration entry points (declared extern "C" in
// SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h). On the
// X360 they patch a shader header so the GPU can fetch the microcode from its
// physical address; they are called by ProgramBuffer::ReBase and by
// CgsResource::RwShaderProgramBufferResourceType::ReBase @0x828A8E90 when a
// compiled program moves in memory.
//
// FLAG PC-platform leaf: there is no D3D9-on-PC equivalent -- shader objects
// are created through IDirect3DDevice9::Create{Vertex,Pixel}Shader from
// bytecode and have no client-visible physical address to re-point. The PC
// bring-up compiles/creates its shaders through the renderengine shader path
// instead, so these registrations are inert here. Kept as no-ops (rather than
// asserts) because ReBase runs whenever a resource block is relocated, which
// is normal pool behaviour and must not fault the sim.
// ---------------------------------------------------------------------------
// (the two opaque XDK shader-object types, declared exactly as
// programbuffer.h declares them -- that header is NOT included here because it
// must not precede <d3d9.h> in this TU)
// (the two opaque XDK shader-object types + the microcode-parts struct,
// declared exactly as programbuffer.h declares them -- that header is NOT
// included here because it must not precede <d3d9.h> in this TU)
struct D3DVertexShader;
struct D3DPixelShader;
struct XGMICROCODE_SHADER_PARTS;

extern "C"
{
    // Microcode inspection: on the X360 these parse a compiled Xenos shader blob
    // (its parts table / constant table). PC shaders are D3D9 bytecode created
    // through the device, so there is nothing to parse: report "no parts" and an
    // empty constant table. Callers (ProgramBuffer::GetResourceDescriptor /
    // Xbox2CreateConstantTable) then size the resource with no microcode tail.
    void* XGGetMicrocodeShaderParts(const void* /*lpFunction*/, XGMICROCODE_SHADER_PARTS* /*lpParts*/)
    {
        return 0;
    }

    s32 XGMicrocodeGetConstantTable(const void* /*lpFunction*/, void** lppConstantTable, u32* lpuSize)
    {
        if (lppConstantTable != 0) *lppConstantTable = 0;
        if (lpuSize != 0)          *lpuSize = 0;
        return 0;
    }

    // Header stamping: the X360 writes the GPU-visible shader header in place so
    // the command processor can fetch the microcode. No PC equivalent exists --
    // shader objects are opaque COM interfaces. Inert.
    void XGSetVertexShaderHeader(D3DVertexShader* /*lpShader*/, u32 /*luShaderSize*/,
                                 const XGMICROCODE_SHADER_PARTS* /*lpParts*/)
    {
    }

    void XGSetPixelShaderHeader(D3DPixelShader* /*lpShader*/, u32 /*luShaderSize*/,
                                const XGMICROCODE_SHADER_PARTS* /*lpParts*/)
    {
    }

    // Registration: patches a shader header to point at the microcode's PHYSICAL
    // address after a pool move (ProgramBuffer::ReBase / RwShaderProgramBuffer-
    // ResourceType::ReBase @0x828A8E90). PC has no client-visible physical
    // address to re-point. Inert -- and deliberately NOT an assert, because
    // ReBase runs on ordinary pool relocation and must not fault the sim.
    void XGRegisterVertexShader(D3DVertexShader* /*lpShader*/, void* /*lpPhysicalPart*/)
    {
    }

    void XGRegisterPixelShader(D3DPixelShader* /*lpShader*/, void* /*lpPhysicalPart*/)
    {
    }

    // The Xenon block-copy intrinsic -- plain memcpy semantics.
    void* XMemCpy(void* lpDest, const void* lpSrc, u32 luCount)
    {
        return std::memcpy(lpDest, lpSrc, luCount);
    }

}

namespace renderengine
{
    // FLAG PC-platform leaf: the GPU buffers' physical-memory classification.
    //
    // X360 IndexBuffer::Xbox2CheckPhysicalMemoryFlags @0x82B60818 / VertexBuffer's
    // @0x82B61148 read the buffer's GPU base address, ask the Xenon kernel for that
    // page's protection class (XQueryMemoryProtect) and set/clear the "system memory"
    // bit 0x200000 in the D3DResource Common word accordingly. Those X360 bodies ARE
    // reconstructed, in pc/gcm/renderengine/IndexBuffer.cpp and VertexBuffer.cpp -- but
    // neither TU is on the exe source list (they pull the whole XGRAPHICS header-layout
    // surface), and on this backend there is no Xenon physical page behind a buffer at
    // all: the data lives in the resource pool's ordinary host allocation and the draw
    // leaf reads it straight through DrawIndexedPrimitiveUP. So there is nothing to
    // query and no flag to set, exactly as for renderengine::Texture::
    // Xbox2CheckPhysicalMemoryFlags (texture.cpp, same X360 idiom, same PC verdict).
    //
    // Called per mesh from CgsResource::RwRenderableResourceType::FixUpRenderableMesh.
    // DELETE these two definitions if IndexBuffer.cpp / VertexBuffer.cpp ever join the
    // source list -- they would then be LNK2005 duplicates of the X360 bodies.
    u32 IndexBuffer::Xbox2CheckPhysicalMemoryFlags(u32* /*lpHeaderDwords*/)
    {
        return 0u;
    }

    u32 VertexBuffer::Xbox2CheckPhysicalMemoryFlags(u32* /*lpHeaderDwords*/)
    {
        return 0u;
    }
}

// ProgramBuffer_ReleaseResource (X360 sub_82B62088) -- the external resource-
// release hook programbuffer.cpp calls from ProgramBuffer::Release. On the X360
// it hands the microcode block back to the GPU resource heap; the PC build
// creates its shaders through the device instead, so the block it would release
// does not exist here.
namespace renderengine { struct ProgramBufferData; }
void ProgramBuffer_ReleaseResource(renderengine::ProgramBufferData* /*lpData*/)
{
}
