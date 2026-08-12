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
#include "GameShared/GameClasses/Graphics/Dispatch/CgsXboxConditionalRenderShims.h" // the predicated-draw externs homed at the bottom of this TU
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
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
    // [DIAG carverts] which of the two publishers bound the current stream: the DISPATCH path
    // (WorldVd32_GetDeclaration + WorldDraw_SetVertexSource) or the Xenon FAST-SET path
    // (D3DDevice_SetStreamSource -> WorldDraw_SetVertexSourceRaw). DELETE with the probe.
    bool                         sbVertexSourceFastSet = false;
    // [DIAG carverts] the pass the renderer is currently walking: 19 == CARS OPAQUE,
    // 20 == CARS TRANSPARENT, 0 == anything else. Set by BrnRendererModule::RenderWorldPasses
    // so a probe can separate a CAR draw from the world meshes around it. DELETE with the probe.
    u32                          suPassTag = 0;

    // Some D3D9 drivers (notably current NVIDIA drivers) do not expose
    // D3DDTCAPS_DEC3N. DrawIndexedPrimitiveUP already copies the submitted vertex
    // data, so unsupported packed normals are expanded into FLOAT3 records here
    // without touching the converted world bundle or the native AMD/Intel path.
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
    // No cache: the expansion is one linear pass over an index run that
    // DrawIndexedPrimitiveUP is about to copy wholesale anyway, and a cache keyed on
    // the run address would go stale when a track unit is streamed out and another
    // is loaded over it.
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
    };
    std::unordered_map<const void*, Vd32Cached> sVdCache;

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
    // Set by WorldVd32_GetDeclaration for the mesh currently being bound.
    bool                    sbLastDeclHasTexcoord0 = false;
    u64                     suLastDeclUsageMask = 0;
    u32                     suLastDeclSourceStride = 0;
    u32                     suLastDeclDec3nCount = 0;
    u16                     sau16LastDeclDec3nOffsets[16] = {};
    // [FLAG PC data gap] Set by WorldFallbackShader_ForceForNextMesh (see its banner further
    // down): the next mesh must use the fallback pair even when the technique's real programs
    // are bound and usable. Console-instanced meshes only.
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

    // The fallback shader's WVP register base (see the KPC_FALLBACK_VS note).
    const u32 KU_FALLBACK_WVP_REGISTER = 240u;

    // The technique's real programs, as chosen by the last SetMeshTechniquePC.
    IDirect3DVertexShader9* spRealVs = nullptr;
    IDirect3DPixelShader9*  spRealPs = nullptr;
    u64         suRealVsInputMask = 0;
    bool        sbRealProgramsBound = false;
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

        bool mbStateCaptured;
        u32  muVpX, muVpY, muVpW, muVpH;
        f32  mfVpMinZ, mfVpMaxZ;
        s32  miScissorL, miScissorT, miScissorR, miScissorB;
        u32  muScissorEnable;
        u32  muZEnable, muZWrite, muZFunc, muCull, muColourWrite;
    };

    const u32      KU_SHADOW_TALLY_SLOTS = 4u;   // 3 cascades + the world-pass control
    ShadowClipTally saShadowClip[KU_SHADOW_TALLY_SLOTS] = {};
    u32             suShadowClipSlot = 0xFFFFFFFFu;
}

namespace renderengine
{
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
    bool WorldPrograms_Bind(const void* lpVertexPayload, const void* lpPixelPayload)
    {
        IDirect3DDevice9* lpDevice = Dev();
        sbRealProgramsBound = false;
        if (lpDevice == nullptr || lpVertexPayload == nullptr || lpPixelPayload == nullptr)
            return false;
        if (!LooksLikeD3D9Bytecode(lpVertexPayload, false) || !LooksLikeD3D9Bytecode(lpPixelPayload, true))
        {
            LogOnce("realbc", "[WorldShader] technique program is not D3D9 bytecode - fallback kept\n");
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
            return false;
        }

        lpDevice->SetVertexShader(lrpVs);
        lpDevice->SetPixelShader(lrpPs);
        spRealVs            = lrpVs;
        spRealPs            = lrpPs;
        suRealVsInputMask   = VertexShaderInputMask(lpVertexPayload);
        sbRealProgramsBound = true;
        LogOnce("realok", "[WorldShader] REAL per-technique programs bound (SHADERS.BNDL)\n");
        return true;
    }

    void WorldShader_ClearRealPrograms()
    {
        sbRealProgramsBound = false;
        spRealVs            = nullptr;
        spRealPs            = nullptr;
        suRealVsInputMask   = 0;
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
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(luUnit, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
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
            return lIt->second.mpDeclaration;
        }

        Vd32Cached lEntry = {};
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
        sVdCache[lpVdImage] = lEntry;
        *lpuStride = lEntry.muStride;
        sbLastDeclHasTexcoord0 = lEntry.mbHasTexcoord0;
        suLastDeclUsageMask    = lEntry.muUsageMask;
        suLastDeclSourceStride = lEntry.muSourceStride;
        suLastDeclDec3nCount   = lEntry.muDec3nCount;
        std::memcpy(sau16LastDeclDec3nOffsets, lEntry.mau16Dec3nOffsets,
                    sizeof(sau16LastDeclDec3nOffsets));
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

        const bool lbForceFallback = sbForceFallbackNextMesh;
        sbForceFallbackNextMesh = false;
        if (lbForceFallback)
        {
            LogOnce("forcefb",
                    "[WorldShader] console-instanced mesh forced onto the flagged fallback pair:"
                    " its *_Instanced technique program has no InstancingMatrixArray"
                    " [FLAG PC data gap]\n");
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
            LogOnce("realdraw", "[WorldShader] REAL technique shaders are drawing the world\n");
            // Re-assert: an earlier mesh of this same technique may have swapped in the
            // fallback pair.
            lpDevice->SetVertexShader(spRealVs);
            lpDevice->SetPixelShader(spRealPs);
            return;
        }
        if (sbRealProgramsBound)
        {
            LogOnce("realdecl",
                    "[WorldShader] mesh declaration lacks an input the technique's vertex program"
                    " declares - flagged fallback used for it\n");
        }

        if (!CompileFallbackShaders(lpDevice))
            return;

        const bool lbTextured = sbMaterialTextureBound && sbLastDeclHasTexcoord0
                             && spFallbackTexVs != nullptr && spFallbackTexPs != nullptr;
        if (lbTextured)
        {
            LogOnce("wtex", "[WorldFallback] TEXTURED world draws live (material samplers bound)\n");
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
    // FLAG PC-platform leaf (DATA gap): CONSOLE-INSTANCED MESHES CANNOT USE THEIR OWN
    // TECHNIQUE PROGRAM ON THIS BUILD.
    //
    // A mesh with mu8InstanceCount > 1 carries geometry the Xenos instancing shader decodes
    // by hand: one copy of the vertices, an index buffer of N slices, and the instance number
    // in the HIGH BITS of every index (index = (instance << 12) | vertexIndex). The shader
    // fetches with (index & 0xFFF) and selects its world matrix out of "InstancingMatrixArray"
    // (shader constant 6) with (index >> 12).
    //
    // The PC technique programs for that set do not implement it. As CgsShaderConstants.cpp
    // (":Missing shader constant from table") already records, the 19 `*_Instanced` techniques
    // are recompiled from the REMASTER's `_Instanced.fx`, whose instancing was reworked and
    // which never declares InstancingMatrixArray or InstancingIndexArray at all -- so the
    // constants Model::SetupShaderConstantsForInstancing publishes are dropped on the floor
    // and the program transforms every vertex by whatever is left in those registers. MEASURED
    // (task #133): every wheel draw reached the device with S_OK, an in-range index run and
    // 240 triangles, and produced EXACTLY ZERO pixels.
    //
    // The flagged fallback pair CAN draw it: DrawRenderable::Interpret already unrolls the
    // N-instance object command into N single-instance mesh commands each carrying instance
    // i's own world-view-projection, Device::SetObjectTransformPC has already published that
    // WVP at c240..c243, and Device::DrawIndexedMeshPC submits slice 0, whose index values are
    // in range of the single vertex copy. So the fallback draws instance i's geometry at
    // instance i's place -- which is exactly the frame the console's shader produces, minus
    // the technique's own shading.
    //
    // DELETE when the PC `*_Instanced` programs are recompiled from sources that implement the
    // console scheme (they need manual vertex fetch, i.e. not D3D9 SM3).
    // (the flag itself lives with the other per-mesh selection state, above)
    // ========================================================================
    void WorldFallbackShader_ForceForNextMesh()
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
        sbVertexSourceFastSet = false;                 // [DIAG carverts] which publisher ran
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
    // MEASURED, on the frame the car is on screen ([carverts] probe): stride 20 published by the
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
        sbVertexSourceFastSet = true;                  // [DIAG carverts] which publisher ran
        suVertexStride        = luStride;
        suVertexSourceStride  = luStride;
        suVertexDec3nCount   = 0;
        std::memset(sau16VertexDec3nOffsets, 0, sizeof(sau16VertexDec3nOffsets));
    }

    // The technique's primitive-reset render state, republished on every technique change.
    // [DIAG carverts] see suPassTag. DELETE with the probe.
    void WorldDraw_SetPassTag(u32 luTag)
    {
        suPassTag = luTag;
    }

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

        // ---- [DIAG carverts 2026-08-12] THE EXPLODING-GEOMETRY PROBE --------------------
        // ⛔ DELETE-WHEN the car body panels are confirmed clean on a booted run.
        //
        // The car draws long stretched silver ribbons out of its panels. Only two things in
        // this leaf can do that to geometry whose object-space vertices and whose WVP are both
        // already known-good (the [carrender] witness proves the part matrices, and the wheels
        // -- same fallback pair, same WVP channel -- land correctly):
        //   (a) an index value OUTSIDE the vertex buffer, which D3D9 UP fetches as whatever
        //       follows the buffer -> one vertex of the triangle flies off; and
        //   (b) a triangle STRIP whose runs were never cut, i.e. the run carries the primitive
        //       reset value but the bound rasteriser state left the reset DISABLED, so the
        //       expansion below is skipped and the end of one run is stitched to the start of
        //       the next -> exactly a long thin ribbon.
        // Both are self-selecting, so this probe reports the CONDITION rather than a sample:
        // it stays silent on a clean draw and names the offender on a broken one.
        //
        // LATCHED ON THE SIGNATURE, never a "printed once" bool (project lesson): a one-shot
        // here fires on the boot loading screen, ~200 log lines before a car exists.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            u32 luOutOfRange = 0, luResetValues = 0, luMaxIndex = 0;
            for (u32 luI = 0; luI < luIndexCount; ++luI)
            {
                const u32 luV = lb32Bit ? reinterpret_cast<const u32*>(lpIndices)[luI]
                                        : reinterpret_cast<const u16*>(lpIndices)[luI];
                if (luV == suResetIndex) { ++luResetValues; continue; }
                if (luV > luMaxIndex) luMaxIndex = luV;
                if (luV >= luNumVertices) ++luOutOfRange;
            }

            const bool lbUncutStrip = (lePrim == D3DPT_TRIANGLESTRIP)
                                   && (luResetValues != 0) && !spResetEnabled;
            if (luOutOfRange != 0 || lbUncutStrip)
            {
                // One line per distinct (cause, primitive, stride) signature, budgeted.
                static u32 suSignatures[16] = {};
                static u32 suSignatureCount = 0;
                const u32  luSignature = (lbUncutStrip ? 0x80000000u : 0u)
                                       | ((luOutOfRange != 0) ? 0x40000000u : 0u)
                                       | (sbVertexSourceFastSet ? 0x20000000u : 0u)
                                       | (static_cast<u32>(lePrim) << 16)
                                       | (suVertexStride & 0xFFFFu);
                bool lbSeen = false;
                for (u32 luS = 0; luS < suSignatureCount; ++luS)
                    if (suSignatures[luS] == luSignature) { lbSeen = true; break; }
                if (!lbSeen && suSignatureCount < 16u)
                {
                    suSignatures[suSignatureCount++] = luSignature;
                    *CgsDev::Log::gpDebugPrint
                        << "[carverts] " << (lbUncutStrip ? "UNCUT STRIP" : "in-range")
                        << (luOutOfRange != 0 ? " + OUT-OF-RANGE INDICES" : "")
                        << " via " << (sbVertexSourceFastSet ? "FAST-SET" : "dispatch")
                        << " clipOrigin (" << safLastWvp[12] << ", " << safLastWvp[13]
                        << ", " << safLastWvp[14] << ", " << safLastWvp[15] << ")"
                        << ": prim " << static_cast<s32>(lePrim)
                        << " indices " << static_cast<s32>(luIndexCount)
                        << " resetEnabled " << (spResetEnabled ? 1 : 0)
                        << " resetIndex 0x" << static_cast<s32>(suResetIndex)
                        << " resetValuesInRun " << static_cast<s32>(luResetValues)
                        << " maxIndex " << static_cast<s32>(luMaxIndex)
                        << " bufferVerts " << static_cast<s32>(luNumVertices)
                        << " outOfRange " << static_cast<s32>(luOutOfRange)
                        << " stride " << static_cast<s32>(suVertexStride)
                        << "/" << static_cast<s32>(suVertexSourceStride)
                        << " dec3n " << static_cast<s32>(suVertexDec3nCount)
                        << " baseVertex " << static_cast<s32>(luBaseVertexIndex)
                        << " startIndex " << static_cast<s32>(luStartIndex) << "\n";
                }
            }
        }

        const u8* lpVertices = nullptr;
        if (suVertexDec3nCount != 0)
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
        else
        {
            lpVertices = static_cast<const u8*>(lpVertexData)
                       + static_cast<size_t>(luBaseVertexIndex) * suVertexSourceStride;
        }

        // ---- [DIAG carverts B] THE CAR-PASS OBJECT-SPACE EXTENT WITNESS -----------------
        // ⛔ DELETE-WHEN the car body panels are confirmed clean on a booted run.
        //
        // Probe A (above) cleared the sky dome, but stretched sheets survive around the car,
        // and A is silent on them -- so their indices are IN RANGE and their strips ARE cut.
        // That leaves one question, and this answers it directly: are the VERTEX BYTES being
        // read correctly? It decodes POSITION for every vertex this draw references and reports
        // the object-space bounding box. A body panel is at most a couple of metres across, so
        // a car-pass draw whose own vertices span tens of metres is being read through the wrong
        // stride or the wrong element offset -- and one whose box is sane exonerates the vertex
        // path entirely and moves the hunt to the transform or the shader.
        //
        // Runs ONLY inside the tagged car passes (lists 19/20), so it costs the world nothing.
        // POSITION is assumed to be element 0 at byte 0 -- the same assumption the wheel NDC
        // diag above already relies on; the printed stride lets that be checked.
        if (suPassTag != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            f32 lafMin[3] = {  3.0e38f,  3.0e38f,  3.0e38f };
            f32 lafMax[3] = { -3.0e38f, -3.0e38f, -3.0e38f };
            u32 luSampled = 0;
            for (u32 luI = 0; luI < luIndexCount; ++luI)
            {
                const u32 luV = lb32Bit ? reinterpret_cast<const u32*>(lpIndices)[luI]
                                        : reinterpret_cast<const u16*>(lpIndices)[luI];
                if (luV == suResetIndex || luV >= luNumVertices)
                    continue;
                f32 lafP[3];
                std::memcpy(lafP, lpVertices + static_cast<size_t>(luV) * suVertexStride,
                            sizeof(lafP));
                for (u32 luC = 0; luC < 3u; ++luC)
                {
                    if (lafP[luC] < lafMin[luC]) lafMin[luC] = lafP[luC];
                    if (lafP[luC] > lafMax[luC]) lafMax[luC] = lafP[luC];
                }
                ++luSampled;
            }

            if (luSampled != 0)
            {
                f32 lfExtent = lafMax[0] - lafMin[0];
                if (lafMax[1] - lafMin[1] > lfExtent) lfExtent = lafMax[1] - lafMin[1];
                if (lafMax[2] - lafMin[2] > lfExtent) lfExtent = lafMax[2] - lafMin[2];

                // Latched on the EXTENT BAND, not on a counter: a run where every part is
                // sane and a run where one panel spans 40 m print different lines, and the
                // bands keep the log to a handful of lines however many draws there are.
                const u32 luBand = (lfExtent < 3.0f)  ? 0u
                                 : (lfExtent < 10.0f) ? 1u
                                 : (lfExtent < 50.0f) ? 2u
                                 : (lfExtent < 500.0f)? 3u : 4u;
                static bool sabBandSeen[2][5] = {};
                const u32 luPassSlot = (suPassTag == 20u) ? 1u : 0u;
                if (!sabBandSeen[luPassSlot][luBand])
                {
                    sabBandSeen[luPassSlot][luBand] = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[carverts-B] list " << static_cast<s32>(suPassTag)
                        << " extentBand " << static_cast<s32>(luBand)
                        << " extent " << lfExtent
                        << " objBox (" << lafMin[0] << ".." << lafMax[0]
                        << ", " << lafMin[1] << ".." << lafMax[1]
                        << ", " << lafMin[2] << ".." << lafMax[2] << ")"
                        << " verts " << static_cast<s32>(luSampled)
                        << "/" << static_cast<s32>(luNumVertices)
                        << " stride " << static_cast<s32>(suVertexStride)
                        << "/" << static_cast<s32>(suVertexSourceStride)
                        << " dec3n " << static_cast<s32>(suVertexDec3nCount)
                        << " prim " << static_cast<s32>(lePrim)
                        << " realShader " << (sbRealProgramsBound ? 1 : 0) << "\n";
                }
            }
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
        if (spResetEnabled && lePrim == D3DPT_TRIANGLESTRIP)
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
        // ---- [PROBE] the clip-space tally + the cascade's first-draw device state ---------
        if (sbShadowPassActive && suShadowClipSlot < KU_SHADOW_TALLY_SLOTS)
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
                for (u32 luK = 0; luK < 3u && luK < luIndexCount; ++luK)
                {
                    const u32 luIdx = lb32Bit ? reinterpret_cast<const u32*>(lpIndices)[luK]
                                              : reinterpret_cast<const u16*>(lpIndices)[luK];
                    if (luIdx >= luNumVertices)
                        continue;

                    f32 lafPosition[3];
                    std::memcpy(lafPosition, lpVertices + luIdx * suVertexStride, sizeof(lafPosition));

                    f32 lafClip[4];
                    for (u32 luLane = 0; luLane < 4u; ++luLane)
                    {
                        lafClip[luLane] = lafPosition[0] * safLastWvp[0 + luLane]
                                        + lafPosition[1] * safLastWvp[4 + luLane]
                                        + lafPosition[2] * safLastWvp[8 + luLane]
                                        +                  safLastWvp[12 + luLane];
                    }

                    ++lrTally.muSampled;
                    const f32 lfW = lafClip[3];
                    if (!(lfW > 0.0f))                              ++lrTally.muBehindW;
                    else if (lafClip[2] < 0.0f)                     ++lrTally.muOutZNear;
                    else if (lafClip[2] > lfW)                      ++lrTally.muOutZFar;
                    else if (lafClip[0] < -lfW || lafClip[0] > lfW
                          || lafClip[1] < -lfW || lafClip[1] > lfW) ++lrTally.muOutXY;
                    else                                            ++lrTally.muInside;
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
        DWORD luSavedZFunc = 0, luSavedZEnable = 0, luSavedZWrite = 0;
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
                lpDevice->GetRenderState(D3DRS_ZFUNC,        &luSavedZFunc);
                lpDevice->GetRenderState(D3DRS_ZENABLE,      &luSavedZEnable);
                lpDevice->GetRenderState(D3DRS_ZWRITEENABLE, &luSavedZWrite);
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

        const HRESULT lhrDraw =
            lpDevice->DrawIndexedPrimitiveUP(lePrim, 0, luNumVertices, luPrimCount,
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
            lpDevice->SetRenderState(D3DRS_ZFUNC,        luSavedZFunc);
            lpDevice->SetRenderState(D3DRS_ZENABLE,      luSavedZEnable);
            lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, luSavedZWrite);
        }
        // [FLAG PC bring-up probe] every submitted world draw, counted. Read by the shadow
        // pass's [shadow-fetch] line to tell "no draws reached the shadow target" apart from
        // "draws reached it and rasterised nothing" -- the two have identical symptoms in the
        // frame and completely different causes. DELETE with the shadow bring-up probes.
        if (SUCCEEDED(lhrDraw))
            ++guWorldDrawCalls;

        if (lbZDefeated)
            lpDevice->SetRenderState(D3DRS_ZFUNC, luSavedZFunc);

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
                    for (u32 luK = 0; luK < 3u && luK < luIndexCount; ++luK)
                    {
                        const u32 luIdx = lb32Bit ? reinterpret_cast<const u32*>(lpIndices)[luK]
                                                  : reinterpret_cast<const u16*>(lpIndices)[luK];
                        if (luIdx >= luNumVertices)
                            continue;
                        f32 lafP[3];
                        std::memcpy(lafP, lpVertices + luIdx * suVertexStride, sizeof(lafP));
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
// Alpha-to-mask is the Xenos' own MSAA coverage feature (RB_COLORCONTROL
// ALPHA_TO_MASK_ENABLE + a 4x2-bit dither offset word). D3D9's nearest relative
// is the vendor-specific ATOC hack; the world pass runs unmultisampled, where
// alpha-to-mask is a no-op on the console too.
// FLAG PC-platform leaf: Xenos MSAA alpha-to-mask, no D3D9 counterpart.
void D3DDevice_SetRenderState_AlphaToMaskEnable(IDirect3DDevice9*, u32) {}
// FLAG PC-platform leaf: Xenos MSAA alpha-to-mask, no D3D9 counterpart.
void D3DDevice_SetRenderState_AlphaToMaskOffsets(IDirect3DDevice9*, u32) {}
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
// This is that bracket, and nothing more: it saves and restores the bound colour surface,
// depth-stencil surface, viewport and scissor. DELETE it when BeginRenderAntiAliased is
// reconstructed -- it is a stand-in for that call, not a piece of console behaviour.
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
    // step. DELETE this line together with the bracket, when BeginRenderAntiAliased lands.
    gpLastRenderTargetState = nullptr;

    if (spSavedColourSurface != nullptr) { spSavedColourSurface->Release(); spSavedColourSurface = nullptr; }
    if (spSavedDepthSurface  != nullptr) { spSavedDepthSurface->Release();  spSavedDepthSurface  = nullptr; }
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
