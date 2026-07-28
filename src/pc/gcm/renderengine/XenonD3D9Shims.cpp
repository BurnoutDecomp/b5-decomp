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
#include "GameShared/GameClasses/Graphics/Dispatch/CgsXboxConditionalRenderShims.h" // the predicated-draw externs homed at the bottom of this TU
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>

// The renderengine D3D device singleton alias the fast-path callers name
// (X360 off_83271608 dereferenced). Defined here; refreshed from
// renderengine::gDevice on every shim entry (device creation happens later
// than static init).
IDirect3DDevice9* gpD3DDevice = nullptr;

namespace
{
    inline IDirect3DDevice9* Dev()
    {
        gpD3DDevice = renderengine::gDevice;
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
        if (lpVdImage == nullptr)
            return nullptr;

        std::unordered_map<const void*, Vd32Cached>::iterator lIt = sVdCache.find(lpVdImage);
        if (lIt != sVdCache.end())
        {
            *lpuStride = lIt->second.muStride;
            sbLastDeclHasTexcoord0 = lIt->second.mbHasTexcoord0;
            suLastDeclUsageMask    = lIt->second.muUsageMask;
            return lIt->second.mpDeclaration;
        }

        Vd32Cached lEntry = { nullptr, 0, false, 0 };
        const u8* lpImage = static_cast<const u8*>(lpVdImage);
        u16 lu16NumElements;
        std::memcpy(&lu16NumElements, lpImage + 0x08, 2);

        D3DVERTEXELEMENT9 laElements[17];
        bool lbOk = (lu16NumElements > 0 && lu16NumElements <= 16);
        u32 luMaxEnd = 0;
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
            // The +8 byte is the record's METHOD lane, but every world element observed uses
            // the plain (DEFAULT) method and D3D9 rejects a whole declaration on an
            // out-of-range method; keep DEFAULT and take only usage/usageIndex from the image.
            laElements[lu].Method     = D3DDECLMETHOD_DEFAULT;
            laElements[lu].Usage      = lu8Usage;
            laElements[lu].UsageIndex = lu8UsageIndex;
            if (lu8Usage == D3DDECLUSAGE_TEXCOORD && lu8UsageIndex == 0)
                lEntry.mbHasTexcoord0 = true;
            lEntry.muUsageMask |= DeclUsageBit(lu8Usage, lu8UsageIndex);
            if (static_cast<u32>(lu16Offset) + 16u > luMaxEnd)
                luMaxEnd = lu16Offset + 16u;
        }

        if (lbOk)
        {
            const D3DVERTEXELEMENT9 lEnd = D3DDECL_END();
            laElements[lu16NumElements] = lEnd;

            // The per-element stride bytes follow the element table; element 0's
            // byte is stream 0's stride.
            lEntry.muStride = lpImage[0x10 + 16 * lu16NumElements];

            IDirect3DDevice9* lpDevice = Dev();
            if (lpDevice != nullptr)
            {
                const HRESULT lhr =
                    lpDevice->CreateVertexDeclaration(laElements, &lEntry.mpDeclaration);
                if (FAILED(lhr) || lEntry.mpDeclaration == nullptr)
                {
                    lEntry.mpDeclaration  = nullptr;
                    lEntry.mbHasTexcoord0 = false;
                    LogOnce("vdfail",
                            "[WorldVd32] CreateVertexDeclaration FAILED for a world descriptor"
                            " - that mesh cannot draw\n");
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
        return lEntry.mpDeclaration;
    }

    // [PC bring-up shim] Choose the fallback pair for the mesh just bound: the
    // TEXTURED variant only when the material bound a texture AND this mesh's
    // declaration actually supplies TEXCOORD0 (see Vd32Cached::mbHasTexcoord0).
    void WorldFallbackShader_SelectForMesh()
    {
        IDirect3DDevice9* lpDevice = Dev();
        if (lpDevice == nullptr)
            return;

        // The technique's REAL programs are bound and this mesh's declaration supplies every
        // input the vertex program declares -> nothing to choose, keep them.
        // (A vs_3_0 input the declaration lacks makes the D3D9 draw fail silently, so a mesh
        // that cannot feed the real shader has to drop back to the flagged fallback pair --
        // which then needs its own c0..c3 WVP restored, because the technique's constants
        // have just been uploaded over those registers.)
        const bool lbRealUsable = sbRealProgramsBound
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

    void WorldDraw_SetIndexSource(const void* lpIndexBufferHeader)
    {
        spIndexSource = static_cast<const IndexBufferHeader32*>(lpIndexBufferHeader);
    }

    void WorldDraw_SetVertexSource(const void* lpVertexBufferHeader, u32 luStride)
    {
        spVertexSource = static_cast<const VertexBufferHeader32*>(lpVertexBufferHeader);
        suVertexStride = luStride;
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
            || suVertexStride == 0)
        {
            LogOnce("updraw", "[WorldDraw] draw skipped: no device/geometry stash (stride 0?)\n");
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
        const u8* lpVertices = static_cast<const u8*>(lpVertexData) + luBaseVertexIndex * suVertexStride;
        const UINT luNumVertices = spVertexSource->muSize / suVertexStride;

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
                return;
            lePrim      = D3DPT_TRIANGLELIST;
            luPrimCount = luTriangles;
            lpIndices   = &sResetScratch[0];
        }

        lpDevice->DrawIndexedPrimitiveUP(lePrim, 0, luNumVertices, luPrimCount,
                                         lpIndices,
                                         lb32Bit ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
                                         lpVertices, suVertexStride);
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
        renderengine::WorldDraw_SetVertexSource(lpStreamData, luStride);
}

unsigned int D3DDevice_SetTexture(IDirect3DDevice9* /*lpDeviceArg*/, u32 luSampler,
                                  void* lpTexture, unsigned int /*luFlags*/)
{
    IDirect3DDevice9* lpDevice = Dev();
    if (lpDevice != nullptr)
        lpDevice->SetTexture(luSampler, static_cast<IDirect3DBaseTexture9*>(lpTexture));
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
    // FACE (bit 2) }, and the Xbox 360 D3DCULL enumerators are exactly that
    // field: NONE = 0, then the two single-bit values in the same order the PC
    // enum uses them (NONE, CW, CCW). That fixes the FACE default: with FACE = 0
    // the CLOCKWISE winding is the front face, so bit 0 (cull front) is
    // D3DCULL_CW == 1 and bit 1 (cull back) is D3DCULL_CCW == 2.
    //
    // Measured on the world, which carries only 0 (none) and 2: reading value 2
    // the other way round -- as "cull the clockwise triangles" -- erased almost
    // every surface in the city (scratch/GEO_B_state), while this reading leaves
    // it solid with the interiors gone (scratch/GEO_C_cull).
    inline DWORD XenonCullToD3D9(u32 luValue)
    {
        const bool lbCullFront = (luValue & 1u) != 0;
        const bool lbCullBack  = (luValue & 2u) != 0;
        const bool lbCwIsFront = (luValue & 4u) == 0;
        if (lbCullFront == lbCullBack)
            return D3DCULL_NONE;      // neither, or both (nothing would draw anyway)
        const bool lbCullClockwise = lbCullFront ? lbCwIsFront : !lbCwIsFront;
        return lbCullClockwise ? D3DCULL_CW : D3DCULL_CCW;
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

} // extern "C"

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
