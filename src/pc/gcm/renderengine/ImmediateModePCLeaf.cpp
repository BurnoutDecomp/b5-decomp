// =============================================================================
// ImmediateModePCLeaf.cpp  (pc/gcm/renderengine)
//
// [PC platform leaf] The home for the renderengine / immediate-mode symbols the
// IMMEDIATE-MODE 3D path (the sky dome) declares and that no project TU defines.
// Every one of them is a Xenon-only surface -- a GPU buffer header stamped by
// XGRAPHICS, a Xenos vertex-declaration object, a command-buffer shader-constant
// row, a packed Xenos render-state block -- realised here over the PC
// IDirect3DDevice9, exactly like XenonD3D9Shims.cpp does for the world pass.
//
// It is the ONE leaf the sky-dome link closure needs. Machine-computed closure
// (dumpbin over the linked object set): the three sky TUs
//   GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dSkyDome.cpp
//   GameSource/Graphics/ImmediateMode/BrnIm3d.cpp
//   GameSource/Graphics/BrnSkyDomeManager.cpp
// raise 67 distinct externals, 45 already provided; of the 22 left, 2 come from
// mounting VertexDescriptorParameters.cpp + CgsRwVertexDescResourceType.cpp and
// the remaining 20 are defined here. (Mounting the EXISTING VertexBuffer.cpp /
// IndexBuffer.cpp / CgsImRenderer.cpp instead makes the closure WORSE -- they are
// X360-shaped, drag XGSetVertexBufferHeader / XMemGetPageSize / XQueryMemoryProtect
// and LNK2005 against the linked CgsIm2d.cpp.)
//
// ---- THE FOUR PC MECHANISMS -------------------------------------------------
//
// 1. A LOW-4GB ARENA for buffer data. The console keeps a GPU address in the
//    32-bit `muBaseAddress` slot of every buffer header and `D3DVertexBuffer_Lock`
//    RETURNS an `int`; both truncations are baked into the committed faithful call
//    sites (BrnSkyDomeManager.cpp:153, EffectsVertexBufferManager.cpp:71). So the
//    data this leaf hands back must be addressable in 32 bits. It is carved from a
//    VirtualAlloc reservation placed below 4 GB.
//
// 2. THE DRAW rides the WORLD's proven path. renderengine::VertexBufferHeader and
//    IndexBufferHeader are byte-identical to the VertexBufferHeader32 /
//    IndexBufferHeader32 views XenonD3D9Shims.cpp's draw stash already consumes
//    (base @+0x18, size/count @+0x20, index width from bit 31 of muCommon), so the
//    sky's D3DDevice_SetStreamSource / SetIndices / DrawIndexedVertices calls flow
//    straight into WorldDraw_IndexedUP -> DrawIndexedPrimitiveUP. No new draw code.
//
// 3. SHADER CONSTANTS are STAGED, then flushed before the draw. The X360's
//    renderengine::Device::BeginShaderStates opens a 16-byte row in the command
//    buffer that the GPU consumes when the draw is issued; the caller writes the
//    value into the returned cursor AFTER the call returns, so a PC implementation
//    cannot upload at call time. Rows are staged here and pushed with
//    Set{Vertex,Pixel}ShaderConstantF from ImShaderConstants_Flush(), which
//    shadow::Device::FlushVertexProgramState calls -- the same point in the frame
//    the console's rows become visible to the GPU.
//
// 4. PROGRAM BUFFERS are ADOPTED, not rebuilt. See ProgramBufferPC_Adopt below.
//
// ---- THE PC PROGRAM-BUFFER DESCRIPTOR LANES (load-bearing) ------------------
// tools/assets/shaders/shader_transcode.py::build_pc_program_buffer writes each
// variable descriptor as `<IBBBB nameOffset, regIdx, typeByte, regCount, 0>` --
// i.e. +4 is the register INDEX and +5 the data TYPE, which is the transposition of
// what the X360 field names (mu8RegisterSet / mu8RegisterIndex) say. Since
// ProgramBuffer::GetVariableHandleByName copies +4 -> handle+0 and +5 -> handle+1,
// on PC `handle.mu8RegisterSet` holds the REGISTER INDEX. That is independently
// corroborated by BrnIm3d.cpp:335, which reads maStateHandles[9].mu8RegisterSet as
// the cloud sampler's UNIT -- only correct under this lane order.
// =============================================================================

#include "types.hpp"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>

#include "pc/gcm/renderengine/device.h"                                        // renderengine::gDevice
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/IndexBuffer.h"
#include "pc/gcm/renderengine/Xbox2VertexBufferShims.h"
#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/renderstates.h"                                  // renderengine::DepthStencilState
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"                              // the shadow-pass leaf surface
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"       // CgsGraphics::ImRendererBase
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"          // shadow::Device
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/rwcore_structs.h"                                                 // rw::Resource

// programbuffer.h forward-declares the opaque XDK shader-object types; it is safe AFTER
// <d3d9.h> (the reverse order is what XenonD3D9Shims.cpp warns about).
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

namespace
{
    inline IDirect3DDevice9* Dev()
    {
        return renderengine::gDevice;
    }

    void LogLine(const char* lpcMessage)
    {
        CgsDev::Log::WriteToLog(lpcMessage);
    }

    // -------------------------------------------------------------------------
    // 1. The low-4 GB arena.
    // -------------------------------------------------------------------------
    // Reserved once at first use. 4 MB covers everything the immediate-mode 3D path
    // asks for by a wide margin: the main sky dome is 991 x 20 B of vertices plus
    // 2158 x 2 B of indices, the cube dome a tenth of that, and the two adopted
    // program-buffer images total a few KB.
    const SIZE_T KU_ARENA_BYTES = 4u * 1024u * 1024u;
    u8*    spArenaBase = nullptr;
    SIZE_T suArenaUsed = 0;
    bool   sbArenaFailed = false;

    bool EnsureArena()
    {
        if (spArenaBase != nullptr)
            return true;
        if (sbArenaFailed)
            return false;

        // Walk candidate bases upward; any of them keeps the whole reservation inside
        // the first 4 GB, which is what the u32 address slots require.
        static const uintptr_t KAU_BASES[] =
        {
            0x10000000u, 0x20000000u, 0x30000000u, 0x40000000u,
            0x50000000u, 0x60000000u, 0x70000000u, 0x80000000u,
            0x90000000u, 0xA0000000u, 0xB0000000u, 0xC0000000u
        };
        for (u32 lu = 0; lu < sizeof(KAU_BASES) / sizeof(KAU_BASES[0]); ++lu)
        {
            void* lpBlock = ::VirtualAlloc(reinterpret_cast<LPVOID>(KAU_BASES[lu]),
                                           KU_ARENA_BYTES, MEM_RESERVE | MEM_COMMIT,
                                           PAGE_READWRITE);
            if (lpBlock != nullptr)
            {
                spArenaBase = static_cast<u8*>(lpBlock);
                suArenaUsed = 0;
                char lacMsg[128];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[ImLeaf] low-4GB arena reserved at 0x%08X (%u KB)\n",
                              static_cast<unsigned>(reinterpret_cast<uintptr_t>(lpBlock)),
                              static_cast<unsigned>(KU_ARENA_BYTES / 1024u));
                LogLine(lacMsg);
                return true;
            }
        }

        sbArenaFailed = true;
        LogLine("[ImLeaf] FAILED to reserve a low-4GB arena - immediate-mode 3D disabled\n");
        return false;
    }

    void* ArenaAlloc(u32 luSize, u32 luAlign)
    {
        if (!EnsureArena())
            return nullptr;
        if (luAlign < 16u)
            luAlign = 16u;
        SIZE_T luCursor = (suArenaUsed + (luAlign - 1u)) & ~static_cast<SIZE_T>(luAlign - 1u);
        if (luCursor + luSize > KU_ARENA_BYTES)
        {
            LogLine("[ImLeaf] low-4GB arena EXHAUSTED - allocation refused\n");
            return nullptr;
        }
        suArenaUsed = luCursor + luSize;
        u8* const lpBlock = spArenaBase + luCursor;
        std::memset(lpBlock, 0, luSize);
        return lpBlock;
    }

    inline u32 ArenaAddress(const void* lpPointer)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer));
    }

    // -------------------------------------------------------------------------
    // Xenon vertex-format dword -> D3D9 decl type + its byte width.
    // -------------------------------------------------------------------------
    // The same table XenonD3D9Shims.cpp::MapXenonDeclType carries (it is the world
    // path's, measured against the converted TRK_UNIT data); the widths are the D3D9
    // D3DDECLTYPE sizes. Duplicated rather than shared because that table is a TU-local
    // static in the world leaf and this TU must not depend on its linkage.
    bool MapVertexFormat(u32 luXenon, u8* lpu8Type, u32* lpuWidth)
    {
        switch (luXenon)
        {
        case 0x2C83A4: *lpu8Type = D3DDECLTYPE_FLOAT1;    *lpuWidth =  4; return true;
        case 0x2C23A5: *lpu8Type = D3DDECLTYPE_FLOAT2;    *lpuWidth =  8; return true;
        case 0x2A23B9: *lpu8Type = D3DDECLTYPE_FLOAT3;    *lpuWidth = 12; return true;
        case 0x1A23A6: *lpu8Type = D3DDECLTYPE_FLOAT4;    *lpuWidth = 16; return true;
        case 0x182886: *lpu8Type = D3DDECLTYPE_D3DCOLOR;  *lpuWidth =  4; return true;
        case 0x1A2286: *lpu8Type = D3DDECLTYPE_UBYTE4;    *lpuWidth =  4; return true;
        case 0x1A2086: *lpu8Type = D3DDECLTYPE_UBYTE4N;   *lpuWidth =  4; return true;
        case 0x2C2359: *lpu8Type = D3DDECLTYPE_SHORT2;    *lpuWidth =  4; return true;
        case 0x1A235A: *lpu8Type = D3DDECLTYPE_SHORT4;    *lpuWidth =  8; return true;
        case 0x2C2159: *lpu8Type = D3DDECLTYPE_SHORT2N;   *lpuWidth =  4; return true;
        case 0x1A215A: *lpu8Type = D3DDECLTYPE_SHORT4N;   *lpuWidth =  8; return true;
        case 0x2C2059: *lpu8Type = D3DDECLTYPE_USHORT2N;  *lpuWidth =  4; return true;
        case 0x1A205A: *lpu8Type = D3DDECLTYPE_USHORT4N;  *lpuWidth =  8; return true;
        case 0x2A2287: *lpu8Type = D3DDECLTYPE_UDEC3;     *lpuWidth =  4; return true;
        case 0x2A2187: *lpu8Type = D3DDECLTYPE_DEC3N;     *lpuWidth =  4; return true;
        case 0x2C235F: *lpu8Type = D3DDECLTYPE_FLOAT16_2; *lpuWidth =  4; return true;
        case 0x1A235F: *lpu8Type = D3DDECLTYPE_FLOAT16_4; *lpuWidth =  8; return true;
        default: return false;
        }
    }

    // The DecFIGS ELEMENTTYPE_* enum (ps3/gcm/renderengine/vertexdescriptor.h) ->
    // D3DDECLUSAGE, used ONLY when a Parameters record leaves the usage lane at its
    // 0xFF "take the default" sentinel. (The X360 reads a 256-byte defaults table at
    // unk_8214AD50 for this; that table is .rodata with no PC home. Deriving the usage
    // from the element TYPE is the same mapping the table encodes and is self-contained.
    // The sky never takes this path -- both its elements name their usage explicitly.)
    u8 DefaultUsageForElementType(u8 lu8ElementType)
    {
        switch (lu8ElementType)
        {
        case 0:  return D3DDECLUSAGE_POSITION;      // ELEMENTTYPE_XYZ
        case 1:  return D3DDECLUSAGE_BLENDWEIGHT;   // ELEMENTTYPE_WEIGHTS
        case 2:  return D3DDECLUSAGE_NORMAL;        // ELEMENTTYPE_NORMAL
        case 3:  return D3DDECLUSAGE_COLOR;         // ELEMENTTYPE_VERTEXCOLOR
        case 4:  return D3DDECLUSAGE_COLOR;         // ELEMENTTYPE_SPECULAR
        case 7:  return D3DDECLUSAGE_BLENDINDICES;  // ELEMENTTYPE_INDICES
        default: return D3DDECLUSAGE_TEXCOORD;      // ELEMENTTYPE_TEX0..7 (8..15)
        }
    }

    // -------------------------------------------------------------------------
    // 3. The staged shader-constant rows.
    // -------------------------------------------------------------------------
    // One row per BeginShaderStates call since the last flush. 64 bytes each so a
    // 4-register matrix (ImRenderer<V>::SetTransform writes worldViewProj as 64 raw
    // bytes into the returned cursor) fits in a single row.
    const u32 KU_MAX_STAGED_ROWS = 64u;
    const u32 KU_ROW_BYTES       = 64u;

    struct StagedRow
    {
        u8   mau8Register;    // PC lane: handle.mu8RegisterSet
        u8   mau8Count;       // handle.mu8RegisterCount (float4 registers)
        bool mbPixel;         // handle.mu8ShaderType != 0
    };

    StagedRow saStagedRows[KU_MAX_STAGED_ROWS];
    f32       safStagedData[KU_MAX_STAGED_ROWS * (KU_ROW_BYTES / sizeof(f32))];
    u32       suStagedCount = 0;
    // A sink for handles the program did not resolve (mu8RegisterCount == 0) and for
    // overflow, so the caller's write always lands somewhere valid.
    f32       safDiscardRow[KU_ROW_BYTES / sizeof(f32)];

    // -------------------------------------------------------------------------
    // The four sky render-state objects (see gpSkyDome*State below).
    // -------------------------------------------------------------------------
    struct ImBlendState
    {
        BOOL mbBlendEnable;
        DWORD meSrcBlend;
        DWORD meDestBlend;
        BOOL mbAlphaTestEnable;
    };
    struct ImRasterizerState
    {
        DWORD meCullMode;
        DWORD meFillMode;
    };
    struct ImDepthStencilState
    {
        BOOL  mbDepthTestEnable;
        BOOL  mbDepthWriteEnable;
        DWORD meDepthFunction;
    };

    // The MAIN sky pass (X360 dword_83010F38 / F3C / F4C) and the ENV-MAP pass
    // (dword_83010F50). The console's state objects are packed Xenos register blocks
    // whose initialiser values were not recovered; these carry the only configuration
    // the sky pass can have given WHERE it is drawn.
    //
    // The pass order is the X360's: pre-Z -> cars opaque -> world opaque -> SKY ->
    // transparents. Drawing a camera-centred dome AFTER the opaque geometry only works
    // if it depth-tests against what is already there and does not write depth --
    // otherwise it would either be occluded everywhere (no test, drawn over) or would
    // stamp its own depth over the city. So: test on with LESS, write OFF, no blending,
    // no alpha test, and cull NONE (the same shared cull-none rasteriser
    // CgsGuiViewModule.cpp names -- a dome is viewed from the inside, so a culled
    // winding would drop the whole thing).
    // The depth FUNCTION is LESSEQUAL, not LESS: the dome is at the far end of the depth
    // range by construction, so a fragment that lands exactly on the cleared 1.0 (which a
    // 9500-unit dome under a 12000-unit far plane does to within a few ULPs) has to pass.
    ImBlendState        sSkyDomeBlendState        = { FALSE, D3DBLEND_ONE, D3DBLEND_ZERO, FALSE };
    ImRasterizerState   sSkyDomeRasterizerState   = { D3DCULL_NONE, D3DFILL_SOLID };
    ImDepthStencilState sSkyDomeDepthStencilState = { TRUE, FALSE, D3DCMP_LESSEQUAL };
    // The env-map faces are rendered into a freshly cleared face with nothing else in
    // it, so that pass takes the depth test out of the way entirely.
    ImDepthStencilState sSkyDomeEnvMapDepthStencilState = { FALSE, FALSE, D3DCMP_ALWAYS };

    // -------------------------------------------------------------------------
    // The SHADOW-MAP pass depth/stencil state (X360 dword_8301090C).
    // -------------------------------------------------------------------------
    // Unlike the four sky states above this one is a REAL renderengine::DepthStencilState:
    // the console's dword_8301090C is CgsDepthStencilStateFactory::saDepthStencilStates[0],
    // an object built by CgsDepthStencilStateFactory::Construct @0x827EBBA0 out of a
    // DepthStencilState::Parameters block. That factory does not run on this build yet
    // (BrnRendererModule::mDepthStencilStateFactory is still a placeholder), so the object is
    // laid down here statically -- but its SHAPE is the factory's, not an invention:
    // renderstates.h records every word of the attested Parameters block, and the values below
    // are exactly what DepthStencilState::Initialize would have written from it.
    //
    //   maState[4]/[8]  = E_FUNCTION_ALWAYS (the two stencil comparison functions)
    //   maState[11]/[12]/[14]/[15] = -1     (the stencil read/write masks, both faces)
    //   everything else zero.
    //
    // The one value that is a CHOICE is maState[0], the depth comparison function: the shadow
    // pass wants LESSEQUAL. These words carry the XENOS zero-based D3DCMPFUNC encoding
    // (NEVER=0 .. ALWAYS=7), NOT PC Direct3D 9's one-based one -- D3DDevice_SetRenderState_ZFunc
    // in XenonD3D9Shims.cpp adds the 1 -- so LESSEQUAL is 3, which the shim turns into
    // D3DCMP_LESSEQUAL (4). Writing 4 here would silently ask for GREATER.
    renderengine::DepthStencilState MakeShadowDepthStencilState()
    {
        renderengine::DepthStencilState lState = {};

        lState.muZFunc               = 3u;   // Xenos LESSEQUAL -> D3DCMP_LESSEQUAL
        lState.muStencilFunc         = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;
        lState.muCcwStencilFunc      = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;
        lState.muStencilMask         = 0xFFFFFFFFu;
        lState.muStencilWriteMask    = 0xFFFFFFFFu;
        lState.muCcwStencilMask      = 0xFFFFFFFFu;
        lState.muCcwStencilWriteMask = 0xFFFFFFFFu;

        lState.muZEnable             = 1u;   // Parameters::mbDepthTestEnable
        lState.muZWriteEnable        = 1u;   // Parameters::mbDepthWriteEnable
        lState.muStencilEnable       = 0u;

        lState.muInitialised         = 1u;   // Initialize's trailing store
        return lState;
    }

    renderengine::DepthStencilState sShadowDepthStencilState = MakeShadowDepthStencilState();
}

// =============================================================================
// The four sky render-state globals (X360 dword_83010F38 / F3C / F4C / F50).
// BrnSkyDomeManager.cpp declares them `extern void*`; they are opaque to it and are
// only ever handed straight back to the three ImDeviceSet*State appliers below.
// =============================================================================
void* gpSkyDomeBlendState              = &sSkyDomeBlendState;
void* gpSkyDomeRasterizerState         = &sSkyDomeRasterizerState;
void* gpSkyDomeDepthStencilState       = &sSkyDomeDepthStencilState;
void* gpSkyDomeEnvMapDepthStencilState = &sSkyDomeEnvMapDepthStencilState;

// X360 dword_8301090C -- the shadow pass's shared depth/stencil state (see the object above).
renderengine::DepthStencilState* gpShadowDepthStencilState = &sShadowDepthStencilState;

// =============================================================================
// CgsGraphics::ImRendererBase::SetState's three state overloads
// (X360 sub_82276A68 blend / sub_82276B38 rasteriser / sub_82276AD0 depth-stencil,
// each a shadow-cached compare-then-apply into the Xenos register shadow). On PC the
// apply is the matching IDirect3DDevice9::SetRenderState set.
//
// ---- THE TWO DEPTH/STENCIL OVERLOADS (reconciled, shadow-map wave) ----------
// The console has ONE SetState(const DepthStencilState*). This leaf carries TWO
// overloads because the two mounted callers hold two genuinely different kinds of
// object, and collapsing them would mean faking one of the two:
//
//   * BrnSkyDomeManager.cpp passes gpSkyDome{,EnvMap}DepthStencilState. The console's
//     dword_83010F4C / F50 are real state objects whose INITIALISER VALUES were never
//     recovered (they are data, and the IDA exports carry no data section), so the sky
//     mount stood them up as the file-local ImDepthStencilState triples above. They are
//     PC placeholders, not renderengine::DepthStencilState objects -- reinterpreting one
//     as a 0x60-byte engine state would read 0x54 bytes past its end.
//   * BrnShadowMapRenderManager.cpp passes gpShadowDepthStencilState, which IS a real
//     renderengine::DepthStencilState (dword_8301090C == saDepthStencilStates[0], whose
//     shape renderstates.h has fully attested).
//
// So the typed overload below is the FAITHFUL one -- it forwards to the real X360
// depth/stencil applier (shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed
// @0x827E8150, the same body every material bind goes through) -- and the void* overload
// stays as the sky's placeholder path. When the sky's two state objects are recovered as
// real DepthStencilStates, the void* overload and its callers go away and one typed
// applier is left, matching the console exactly.
// =============================================================================
void ImDeviceSetBlendState(void* lpState)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpState == nullptr)
        return;
    const ImBlendState* const lpBlend = static_cast<const ImBlendState*>(lpState);
    lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, lpBlend->mbBlendEnable);
    lpDevice->SetRenderState(D3DRS_SRCBLEND,  lpBlend->meSrcBlend);
    lpDevice->SetRenderState(D3DRS_DESTBLEND, lpBlend->meDestBlend);
    lpDevice->SetRenderState(D3DRS_ALPHATESTENABLE, lpBlend->mbAlphaTestEnable);
    lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                             D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
                             | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
}

void ImDeviceSetRasterizerState(void* lpState)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpState == nullptr)
        return;
    const ImRasterizerState* const lpRaster = static_cast<const ImRasterizerState*>(lpState);
    lpDevice->SetRenderState(D3DRS_CULLMODE, lpRaster->meCullMode);
    lpDevice->SetRenderState(D3DRS_FILLMODE, lpRaster->meFillMode);
    lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    // ⚠ The three states a WORLD technique's rasteriser state can leave behind that would
    // silently annihilate this pass. The sky dome sits at the far end of the depth range
    // (a 9500-unit dome under a 12000-unit far plane lands at ndc z ~0.99999), so ANY
    // residual depth bias pushes it past 1.0 and every fragment fails the depth test --
    // an S_OK draw that produces no pixels. Same for a leftover user clip plane.
    const DWORD KU_FLOAT_ZERO = 0u;   // 0.0f as a DWORD, which is how D3D9 takes these
    lpDevice->SetRenderState(D3DRS_DEPTHBIAS, KU_FLOAT_ZERO);
    lpDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, KU_FLOAT_ZERO);
    lpDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0u);
}

void ImDeviceSetDepthStencilState(void* lpState)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpState == nullptr)
        return;
    const ImDepthStencilState* const lpDepth = static_cast<const ImDepthStencilState*>(lpState);
    lpDevice->SetRenderState(D3DRS_ZENABLE,
                             lpDepth->mbDepthTestEnable ? D3DZB_TRUE : D3DZB_FALSE);
    lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, lpDepth->mbDepthWriteEnable);
    lpDevice->SetRenderState(D3DRS_ZFUNC, lpDepth->meDepthFunction);
    lpDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
}

// X360 sub_82276AD0 == shadow::Device::SetState(const DepthStencilState*) -- NOT
// CgsGraphics::ImRendererBase::SetState, which is sub_82276DA8 (the same block behind the
// mgpActiveRenderer assert at CgsImRenderer.h:732). Corrected: 0x82276AD0 reads r3 as the state
// pointer (`mr r31, r3` @0x82276AE8) and forms the file-scope shadow-block base itself
// (`addi r30, r11, off_83010950@l` @0x82276AFC), so it has no `this` and no renderer.
//
// The console body is: gate on mbDepthStencilStateLocked (@0x82276AEC) -> compare against
// dword_83010A28 (@0x82276B00) -> apply -> cache (@0x82276B1C). The apply is
// shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed (@0x827E8150), which is reconstructed and
// pushes all 23 words through the D3DDevice_SetRenderState_* fast-set surface.
//
// THE COMPARE HALF IS NOW REPRODUCIBLE -- and is deliberately NOT switched on in this pass.
// The old objection read: the shadow device's `mpLastDepthStencilState` belongs to the mesh-dispatch
// cache, so there is nothing here to compare against without inventing a second cache. That was true
// of a cache that should never have existed -- mpLastDepthStencilState was a SECOND host home for
// dword_83010A28 (== StateBlockShadow::m_pDepthStencilState, DWARF shadowingdevice.h:669), deleted in
// the same pass. There is now one slot (shadow::Device::mpDepthStencilState) and one setter over it,
// so forwarding to shadow::Device::SetState would get the console's gate, compare, apply and cache
// write-back, including lbWasUnset == (cached == nullptr), where this shim forces `true` and re-pushes
// all 23 words on every call.
//
// ⚠️ WHY THE BODY STILL FORCE-APPLIES. Making that switch is the ONLY live per-frame behaviour change
// this pass would make in the linked boot exe (ImmediateModePCLeaf.cpp is mounted,
// build_game_exe.bat:214), and it lands on the shadow path -- gpShadowDepthStencilState is a
// renderengine::DepthStencilState*, so BrnShadowMapRenderManager.cpp:70/102/131 bind through exactly
// this typed overload. Today the apply is unconditional; forwarding makes it SKIP whenever the cached
// pointer matches, which is only safe if no direct-D3D depth writer can run between two typed calls
// carrying the same pointer -- the untyped void* overload the sky path uses is the candidate that
// could. That argument has not been made, and this pass ships alongside the anti-alias bracket, so a
// regression here would be indistinguishable from a bracket regression. The switch belongs to its own
// boot-verified change, on its own, where it can be attributed. See the same reasoning that withdrew
// the ResetShadowing redirect from the G3 pass.
void ImDeviceSetDepthStencilState(renderengine::DepthStencilState* lpState)
{
    if (lpState == nullptr)
        return;
    shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed(lpState, true);
}

// =============================================================================
// X360 sub_82B61D78 -- clear the bound depth/stencil surface.
//
// The console body is `D3DDevice_Clear(device, 0, 0, p->flags, 0, p->depth, ..., p->stencil, 0)`
// (with a BeginTiling fast path when the engine is mid-predicated-tiling, which has no PC
// counterpart). The one thing that does NOT forward verbatim is the flags word: the Xenon
// D3DCLEAR_* mask reserves bits 0..3 for its four colour targets and puts ZBUFFER at 0x10 /
// STENCIL at 0x20, whereas PC Direct3D 9 uses TARGET 0x1 / ZBUFFER 0x2 / STENCIL 0x4. The
// shadow pass's 0x30 is therefore Z | STENCIL, not "targets 4 and 5".
//
// FLAG PC-platform leaf: D3D9 rejects the whole Clear with D3DERR_INVALIDCALL when
// D3DCLEAR_STENCIL is asked for on a depth surface that has no stencil channel (a D16 shadow
// map is the normal case). The console's EDRAM surface always has one. Rather than lose the
// depth clear with it, the stencil bit is dropped and the clear retried.
// =============================================================================
void DeviceClearDepthStencil(const renderengine::ClearDepthStencilParameters* lpParameters)
{
    IDirect3DDevice9* const lpDevice = Dev();
    if (lpDevice == nullptr || lpParameters == nullptr)
        return;

    const u32 luXenonFlags = lpParameters->mu32Flags;
    DWORD luFlags = 0;
    if ((luXenonFlags & 0x0Fu) != 0u) luFlags |= D3DCLEAR_TARGET;    // Xenon TARGET0..3
    if ((luXenonFlags & 0x10u) != 0u) luFlags |= D3DCLEAR_ZBUFFER;
    if ((luXenonFlags & 0x20u) != 0u) luFlags |= D3DCLEAR_STENCIL;
    if (luFlags == 0)
        return;

    HRESULT lhr = lpDevice->Clear(0, nullptr, luFlags, D3DCOLOR_ARGB(0, 0, 0, 0),
                                  lpParameters->mfDepth, lpParameters->mu32Stencil);
    bool lbDroppedStencil = false;
    if (FAILED(lhr) && (luFlags & D3DCLEAR_STENCIL) != 0)
    {
        luFlags &= ~static_cast<DWORD>(D3DCLEAR_STENCIL);
        lbDroppedStencil = true;
        lhr = lpDevice->Clear(0, nullptr, luFlags, D3DCOLOR_ARGB(0, 0, 0, 0),
                              lpParameters->mfDepth, lpParameters->mu32Stencil);
    }

    // ⚠ VALUE-LATCHED, not a `static bool` one-shot. The old one-shots here could only ever
    // say "this happened at least once, some time"; with the shadow pass calling this three
    // times a frame that is not enough to answer the question that matters -- DID THE Z CLEAR
    // ACTUALLY HAPPEN, every frame? "Cleared to 1.0" and "not cleared at all" are the
    // difference between LESSEQUAL passing everything and LESSEQUAL rejecting everything, and
    // the depth surface cannot be read back to check. The latch is
    // (flags, hr, stencil-dropped, the depth value), so a clear that starts failing -- or
    // starts clearing to a different Z -- reprints instead of hiding behind the first line.
    {
        struct ClearLatch { DWORD muFlags; HRESULT mhr; u32 muDropped; f32 mfDepth; };
        static ClearLatch sLast = { 0xFFFFFFFFu, 0x7FFFFFFF, 0xFFFFFFFFu, -1.0e30f };
        const ClearLatch  lNow  = { luFlags, lhr, lbDroppedStencil ? 1u : 0u, lpParameters->mfDepth };
        if (lNow.muFlags != sLast.muFlags || lNow.mhr != sLast.mhr
            || lNow.muDropped != sLast.muDropped || lNow.mfDepth != sLast.mfDepth)
        {
            sLast = lNow;
            char lacMessage[192];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[shadow-fetch] depth clear: xenonFlags=0x%X d3dFlags=0x%X z=%.3f"
                          " stencilDropped=%d hr=0x%08X %s\n",
                          (unsigned)luXenonFlags, (unsigned)luFlags,
                          (double)lpParameters->mfDepth, lbDroppedStencil ? 1 : 0,
                          (unsigned)lhr, SUCCEEDED(lhr) ? "OK" : "FAILED");
            LogLine(lacMessage);
        }
    }
}

// =============================================================================
// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- open one
// shader-constant row and return the write cursor (X360 r3).
//
// The argument is a renderengine::ProgramVariableHandle*: BrnIm3d.cpp's
// PushShaderConstant passes &maStateHandles[i] directly, and ImRenderer<V>::
// SetTransform passes &maShaderStateBlocks[slot], which BrnIm3d.cpp:180 established
// IS that slot's "worldViewProj" handle.
//
// See the file banner for why the row is staged rather than uploaded here (the value
// is written by the caller AFTER this returns) and for the PC handle lane order.
// =============================================================================
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut)
{
    if (lppShaderStateOut != nullptr)
        *lppShaderStateOut = safDiscardRow;

    if (lpShaderStateBlock == nullptr)
        return nullptr;

    const renderengine::ProgramVariableHandle* const lpHandle =
        static_cast<const renderengine::ProgramVariableHandle*>(lpShaderStateBlock);

    // mu8RegisterCount == 0 is GetVariableHandleByName's "not found" answer, and a
    // SAMPLER handle carries no float rows either -- both go to the discard row.
    if (lpHandle->mu8RegisterCount == 0u || suStagedCount >= KU_MAX_STAGED_ROWS)
        return safDiscardRow;

    u32 luCount = lpHandle->mu8RegisterCount;
    if (luCount > KU_ROW_BYTES / 16u)
        luCount = KU_ROW_BYTES / 16u;

    const u32 luRow = suStagedCount++;
    saStagedRows[luRow].mau8Register = lpHandle->mu8RegisterSet;    // PC lane: register index
    saStagedRows[luRow].mau8Count    = static_cast<u8>(luCount);
    saStagedRows[luRow].mbPixel      = (lpHandle->mu8ShaderType != 0u);

    f32* const lpfRow = &safStagedData[luRow * (KU_ROW_BYTES / sizeof(f32))];
    if (lppShaderStateOut != nullptr)
        *lppShaderStateOut = lpfRow;
    return lpfRow;
}

namespace renderengine
{
    // [DIAG] the world leaf's immediate-mode draw marker (XenonD3D9Shims.cpp).
    void WorldDraw_MarkImmediateMode();

    // Push every staged row to the D3D9 constant file, then reset the stage. Called
    // from shadow::Device::FlushVertexProgramState -- the point in the frame where the
    // console's command-buffer rows become visible to the GPU.
    void ImShaderConstants_Flush()
    {
        if (suStagedCount == 0u)
            return;
        WorldDraw_MarkImmediateMode();

        IDirect3DDevice9* const lpDevice = Dev();
        // [DIAG one-shot] the first staged batch, as it reaches the constant file.
        static bool sbDiagFlush = false;
        const bool  lbDiag = !sbDiagFlush;
        if (lbDiag)
        {
            sbDiagFlush = true;
            char lacMsg[128];
            std::snprintf(lacMsg, sizeof(lacMsg), "[ImLeaf] constant flush: %u rows, device=%d\n",
                          static_cast<unsigned>(suStagedCount), (int)(lpDevice != nullptr));
            LogLine(lacMsg);
        }
        if (lpDevice != nullptr)
        {
            for (u32 luRow = 0; luRow < suStagedCount; ++luRow)
            {
                const StagedRow& lrRow = saStagedRows[luRow];
                const f32* const lpfData = &safStagedData[luRow * (KU_ROW_BYTES / sizeof(f32))];
                if (lbDiag)
                {
                    char lacMsg[192];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                                  "[ImLeaf]   %s c%u x%u = %.4f %.4f %.4f %.4f\n",
                                  lrRow.mbPixel ? "ps" : "vs",
                                  static_cast<unsigned>(lrRow.mau8Register),
                                  static_cast<unsigned>(lrRow.mau8Count),
                                  lpfData[0], lpfData[1], lpfData[2], lpfData[3]);
                    LogLine(lacMsg);
                }
                if (lrRow.mbPixel)
                    lpDevice->SetPixelShaderConstantF(lrRow.mau8Register, lpfData, lrRow.mau8Count);
                else
                    lpDevice->SetVertexShaderConstantF(lrRow.mau8Register, lpfData, lrRow.mau8Count);
            }
        }
        suStagedCount = 0u;
    }

    // -------------------------------------------------------------------------
    // 4. ProgramBufferPC_Adopt -- take a pre-built platform-4 ShaderProgramBuffer
    //    image and make it a live ProgramBufferData.
    // -------------------------------------------------------------------------
    // The X360 route (ProgramBuffer::GetResourceDescriptor -> allocator Create ->
    // ProgramBuffer::Initialize) CANNOT run on this backend: both of those bodies call
    // XGGetMicrocodeShaderParts, whose PC stub returns 0 without writing *lpParts, and
    // then read that uninitialised ProgramMicrocodeParts for the microcode size and
    // feed a truncated 64-bit function pointer to Xbox2CreateConstantTable. That is a
    // crash, not a cosmetic gap.
    //
    // There is nothing for them to do here anyway: the converter's blob ALREADY IS a
    // ProgramBufferData -- shader type at +0, variable count at +4, microcode size at
    // +8, the D3D9 bytecode at +0x14, and the variable descriptors at
    // +0x14+microcodeSize. Adoption is therefore a copy into the low-4 GB arena plus
    // ONE fix-up: the converter stores each descriptor's name as a FILE offset, but
    // ProgramBuffer::GetVariableHandleByName dereferences it as an absolute address
    // (and the field is a u32, which is the other reason the copy has to be low).
    //
    // Returns nullptr if the blob is not a PC program buffer, leaving the caller free
    // to take the console path.
    ProgramBufferData* ProgramBufferPC_Adopt(const void* lpBlob, u32 luBlobSize, u32 luShaderType)
    {
        if (lpBlob == nullptr || luBlobSize < 0x18u)
            return nullptr;

        const u8* const lpSource = static_cast<const u8*>(lpBlob);
        u32 luSourceType    = 0;
        u16 lu16NumVariables = 0;
        u32 luMicrocodeSize  = 0;
        std::memcpy(&luSourceType,     lpSource + 0x00, 4);
        std::memcpy(&lu16NumVariables, lpSource + 0x04, 2);
        std::memcpy(&luMicrocodeSize,  lpSource + 0x08, 4);

        if (luSourceType != luShaderType)
        {
            LogLine("[ImLeaf] program blob shader type does not match the requested stage\n");
            return nullptr;
        }
        if (luMicrocodeSize == 0u || 0x14u + luMicrocodeSize > luBlobSize)
        {
            LogLine("[ImLeaf] program blob microcode size is out of range\n");
            return nullptr;
        }

        // The payload must be D3D9 SM3 bytecode (0xFFFE03xx vertex / 0xFFFF03xx pixel);
        // this is the same gate XenonD3D9Shims.cpp::LooksLikeD3D9Bytecode applies before
        // Create{Vertex,Pixel}Shader, and it is what makes the adoption safe.
        u32 luToken = 0;
        std::memcpy(&luToken, lpSource + 0x14, 4);
        const u32 luExpected = (luShaderType != 0u) ? 0xFFFF0000u : 0xFFFE0000u;
        if ((luToken & 0xFFFF0000u) != luExpected || (luToken & 0x0000FF00u) > 0x0300u)
        {
            LogLine("[ImLeaf] program blob payload is not D3D9 SM3 bytecode\n");
            return nullptr;
        }

        u8* const lpImage = static_cast<u8*>(ArenaAlloc(luBlobSize, 16u));
        if (lpImage == nullptr)
            return nullptr;
        std::memcpy(lpImage, lpSource, luBlobSize);

        // Rebase the descriptors' name offsets to absolute addresses in the copy.
        ProgramVariableDescriptor* const lpDescriptors =
            reinterpret_cast<ProgramVariableDescriptor*>(lpImage + 0x14 + luMicrocodeSize);
        for (u32 luVariable = 0; luVariable < lu16NumVariables; ++luVariable)
        {
            const u32 luNameOffset = lpDescriptors[luVariable].muNameOffset;
            if (luNameOffset >= luBlobSize)
            {
                LogLine("[ImLeaf] program blob variable name offset out of range - adoption refused\n");
                return nullptr;
            }
            lpDescriptors[luVariable].muNameOffset = ArenaAddress(lpImage + luNameOffset);
        }

        {
            char lacMsg[192];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[ImLeaf] adopted PC program buffer: stage=%u bytes=%u microcode=%u vars=%u\n",
                          static_cast<unsigned>(luShaderType), static_cast<unsigned>(luBlobSize),
                          static_cast<unsigned>(luMicrocodeSize),
                          static_cast<unsigned>(lu16NumVariables));
            LogLine(lacMsg);
        }
        return reinterpret_cast<ProgramBufferData*>(lpImage);
    }

    // =========================================================================
    // renderengine::VertexDescriptor::Initialize(rw::Resource*, const Parameters*)
    // X360 @0x82B63060 (the Parameters-typed overload).
    //
    // A PORT, not an invention: the element decode below is the one
    // XenonD3D9Shims.cpp:812-880 (WorldVd32_GetDeclaration) already runs on every world
    // mesh -- +0 stream, +2 offset, +4 Xenon format, +9 usage, +0xA usage index -- and
    // that path is empirically correct (REAL=4096 of 4096 world meshes, fallback flat 0).
    // What is added here is what the world's serialised images already carry pre-baked:
    // the auto-pack of an unset offset, the per-stream stride accumulation, and the
    // stream / type-2 masks.
    //
    // Reads Parameters through the GROUND-TRUTH byte lanes documented in
    // VertexDescriptor.h (the member NAMES are shifted one lane; two live fields are
    // called "pad"):
    //     +0x0 stream   +0x2 offset  +0x4 format(-1 = empty)  +0x8 method
    //     +0x9 usage    +0xA usageIndex  +0xB elementType     +0xC elementClass
    //
    // ⚠ x64: the descriptor object is NOT built in the rw resource block. The console's
    // GetResourceDescriptor sizes it at `17 * validElements + 16` (a 16-byte header, a
    // 16-byte element record each, and one trailing stride byte each) -- 50 bytes for
    // the sky's two elements -- while the x64 VertexDescriptorData is 0x124 because the
    // declaration pointer widened. Changing the sizer is not an option (it also sizes
    // SERIALISED bundle descriptors, which must stay console-exact), so the object is
    // carved from this leaf's arena and the resource block is left unused.
    // =========================================================================
    VertexDescriptorData* VertexDescriptor::Initialize(rw::Resource* /*lpResource*/,
                                                       const Parameters* lpParameters)
    {
        if (lpParameters == nullptr)
            return nullptr;

        VertexDescriptorData* const lpData =
            static_cast<VertexDescriptorData*>(ArenaAlloc(sizeof(VertexDescriptorData), 16u));
        if (lpData == nullptr)
            return nullptr;

        std::memset(lpData, 0, sizeof(VertexDescriptorData));

        u32 lauStreamStride[16] = { 0 };
        u32 luValidCount = 0;
        D3DVERTEXELEMENT9 laDecl[17];

        for (u32 luRecord = 0; luRecord < 16u; ++luRecord)
        {
            const Parameters::Element& lrIn = lpParameters->maElements[luRecord];

            const u32 luFormat = static_cast<u32>(lrIn.miOffset);      // lane +0x4
            if (luFormat == 0xFFFFFFFFu)
                continue;                                              // the empty-slot sentinel

            const u32 luStream = lrIn.mu16Stream;                      // lane +0x0
            if (luStream >= 16u)
                continue;

            u8  lu8DeclType = 0;
            u32 luWidth     = 0;
            if (!MapVertexFormat(luFormat, &lu8DeclType, &luWidth))
            {
                LogLine("[ImLeaf] VertexDescriptor: unknown vertex format code - element dropped\n");
                continue;
            }

            u8 lu8Usage      = lrIn.mu8Pad1;                           // lane +0x9
            u8 lu8UsageIndex = lrIn.mu8Usage;                          // lane +0xA
            const u8 lu8ElementType = lrIn.mu8UsageIndex;              // lane +0xB
            if (lu8Usage == 0xFFu)
                lu8Usage = DefaultUsageForElementType(lu8ElementType);
            if (lu8UsageIndex == 0xFFu)
                lu8UsageIndex = 0u;

            // The offset lane doubles as an auto-pack request: >= 0x100 means "append
            // me to this stream" (the X360 takes the accumulator as the offset and then
            // advances it); anything smaller is an explicit in-stream byte offset that
            // only grows the accumulator.
            u16 lu16Offset = lrIn.mu16Pad0;                            // lane +0x2
            if (lu16Offset >= 0x100u)
            {
                lu16Offset = static_cast<u16>(lauStreamStride[luStream]);
                lauStreamStride[luStream] += luWidth;
            }
            else if (static_cast<u32>(lu16Offset) + luWidth > lauStreamStride[luStream])
            {
                lauStreamStride[luStream] = static_cast<u32>(lu16Offset) + luWidth;
            }

            VertexElement& lrOut = lpData->maElements[luValidCount];
            lrOut.muStream     = static_cast<u16>(luStream);
            lrOut.muOffset     = lu16Offset;
            lrOut.muType       = luFormat;
            lrOut.mauTail[0]   = lrIn.mu8Type;                         // lane +0x8 (D3DDECLMETHOD)
            lrOut.mauTail[1]   = lu8Usage;
            lrOut.mauTail[2]   = lu8UsageIndex;
            lrOut.mauTail[3]   = lu8ElementType;
            lrOut.muReserved0C = static_cast<u32>(lrIn.mu8Enabled);    // lane +0xC (element class)

            lpData->muStreamMask |= (1u << luStream);
            if (lrIn.mu8Enabled == 2u)                                 // ELEMENTCLASS_PERINSTANCE
                lpData->muType2Mask |= static_cast<u16>(1u << luStream);

            // The matching D3D9 declaration record. (The X360's CreateD3DObject marshals
            // the element table into the XENON 12-byte D3DVERTEXELEMENT form and hands it
            // to D3DDevice_CreateVertexDeclaration; that body lives in the unmounted
            // VertexDescriptor.cpp and translating back out of the packed form would only
            // re-derive what is already decoded right here. Same records either way.)
            laDecl[luValidCount].Stream     = static_cast<WORD>(luStream);
            laDecl[luValidCount].Offset     = lu16Offset;
            laDecl[luValidCount].Type       = lu8DeclType;
            laDecl[luValidCount].Method     = lrIn.mu8Type;            // lane +0x8 (D3DDECLMETHOD)
            laDecl[luValidCount].Usage      = lu8Usage;
            laDecl[luValidCount].UsageIndex = lu8UsageIndex;

            ++luValidCount;
        }

        lpData->muElementCount = static_cast<u16>(luValidCount);

        // The per-element stride byte table. The X360 packs it immediately after the
        // USED elements (at 0x10 * (count + 1)); on x64 that address lands inside the
        // widened element array, so it is written -- and read -- BY NAME. A non-zero
        // per-element flag byte in the Parameters block overrides the computed stride.
        for (u32 luElement = 0; luElement < luValidCount; ++luElement)
        {
            const u8 lu8Override = lpParameters->maElementFlags[luElement];
            lpData->mauStreamStride[luElement] =
                (lu8Override != 0u)
                    ? lu8Override
                    : static_cast<u8>(lauStreamStride[lpData->maElements[luElement].muStream]);
        }

        lpData->muInitialised = 1u;

        IDirect3DDevice9* const lpDevice = Dev();
        if (lpDevice != nullptr && luValidCount != 0u)
        {
            const D3DVERTEXELEMENT9 lEnd = D3DDECL_END();
            laDecl[luValidCount] = lEnd;
            IDirect3DVertexDeclaration9* lpDeclaration = nullptr;
            if (SUCCEEDED(lpDevice->CreateVertexDeclaration(laDecl, &lpDeclaration)))
                lpData->mpDeclaration = reinterpret_cast<D3DVertexDeclaration*>(lpDeclaration);
        }

        {
            char lacMsg[192];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[ImLeaf] vertex descriptor built: elements=%u stride0=%u decl=%s\n",
                          static_cast<unsigned>(luValidCount),
                          static_cast<unsigned>(lpData->mauStreamStride[0]),
                          (lpData->mpDeclaration != nullptr) ? "ok" : "FAILED");
            LogLine(lacMsg);
        }
        return lpData;
    }

    // =========================================================================
    // renderengine::VertexDescriptor::Release(VertexDescriptorData*)
    // X360 @0x82B63250 -- the other end of the Initialize above, and it lives HERE for the
    // same reason Initialize does: this leaf is what created the D3D declaration, so
    // this leaf is what releases it. The console reconstruction of the whole class
    // (pc/gcm/renderengine/VertexDescriptor.cpp) carries a Release too, but that TU also
    // carries the console Initialize -- mounting it beside this leaf is a duplicate symbol
    // -- and its Release goes through two Xenon helpers (VertexDescriptor_PreRelease
    // @0x82B61F90, then D3DResource_Release on the declaration) that resolve, on D3D9, to
    // exactly one thing: the COM Release of the IDirect3DVertexDeclaration9. Same shape as
    // the console body store for store: test the pointer, release, null it, return lpData.
    //
    // The VertexDescriptorData block itself is arena memory (see Initialize's ⚠ x64 note)
    // and the arena has no free, so -- as for every other object this leaf carves -- the
    // block is not reclaimed; only the device object is. Callers: BrnPostFxShader::Destruct
    // and BrnPostFxBloom::Destruct (each once, at teardown).
    // =========================================================================
    VertexDescriptorData* VertexDescriptor::Release(VertexDescriptorData* lpData)
    {
        if (lpData != nullptr && lpData->mpDeclaration != nullptr)      // if (*result)
        {
            IDirect3DVertexDeclaration9* const lpDeclaration =
                reinterpret_cast<IDirect3DVertexDeclaration9*>(lpData->mpDeclaration);
            lpDeclaration->Release();                                     // PreRelease + D3DResource_Release
            lpData->mpDeclaration = nullptr;                              // *v1 = 0
        }
        return lpData;
    }

    // =========================================================================
    // The Xenon vertex/index buffer surface.
    //
    // On the X360 these stamp a GPU resource header in place (XGSetVertexBufferHeader)
    // over memory the rw resource allocator carved, and Lock hands back the GPU-visible
    // address. On this backend the header IS the object (its layout is exactly what the
    // world draw stash reads) and the bytes live in the low-4 GB arena, so Lock is just
    // "hand back muBaseAddress".
    //
    // ⚠ The rw resource for these carries slot 0 ONLY -- BrnResourceAllocator's
    // HeapResourceAllocator::Allocate serves pool 0 from the general heap and leaves
    // m_baseResources[1..3] null -- so the DATA block (descriptor slot 2) does not
    // exist. That is the second reason the arena is needed, independently of the u32
    // address slot.
    // =========================================================================
    VertexBufferHeader* VertexBuffer::Initialize(Wrapper* lpWrapper, const Parameters* lpParams,
                                                 int /*liArg3*/, int /*liArg4*/)
    {
        if (lpWrapper == nullptr || lpParams == nullptr)
            return nullptr;

        // The wrapper is the rw::Resource the allocator filled: +0x00 is the header
        // block (pool 0). Fall back to the arena when the pool did not serve it.
        VertexBufferHeader* lpHeader = lpWrapper->mpHeader;
        if (lpHeader == nullptr)
            lpHeader = static_cast<VertexBufferHeader*>(ArenaAlloc(sizeof(VertexBufferHeader), 16u));
        if (lpHeader == nullptr)
            return nullptr;

        void* const lpBytes = ArenaAlloc(lpParams->muLength, 16u);
        if (lpBytes == nullptr)
            return nullptr;

        std::memset(lpHeader, 0, sizeof(VertexBufferHeader));
        lpHeader->muCommon         = 0u;
        lpHeader->muReferenceCount = 1u;
        lpHeader->muUnused14       = 0xFFFF0000u;
        lpHeader->muBaseAddress    = ArenaAddress(lpBytes);   // 4-byte aligned; the low 2 bits are the
                                                              // memory-class flags the readers mask off
        lpHeader->muSize           = lpParams->muLength;
        lpHeader->muFormat         = lpParams->muFormat;
        return lpHeader;
    }

    IndexBufferHeader* IndexBuffer::Initialize(IndexBufferWrapper* lpWrapper,
                                               const IndexBufferParameters* lpParams)
    {
        if (lpWrapper == nullptr || lpParams == nullptr)
            return nullptr;

        IndexBufferHeader* lpHeader = lpWrapper->mpHeader;
        if (lpHeader == nullptr)
            lpHeader = static_cast<IndexBufferHeader*>(ArenaAlloc(sizeof(IndexBufferHeader), 16u));
        if (lpHeader == nullptr)
            return nullptr;

        const u32 luBytesPerIndex = (lpParams->muFormat == 32u) ? 4u : 2u;
        const u32 luSizeRounded   = (luBytesPerIndex * lpParams->muCount + 15u) & ~15u;

        void* const lpBytes = ArenaAlloc(luSizeRounded, 16u);
        if (lpBytes == nullptr)
            return nullptr;

        std::memset(lpHeader, 0, sizeof(IndexBufferHeader));
        // muCommon carries the index-format tag; bit 31 selects the width, which is what
        // both the X360 GetParameters and the PC draw stash read.
        lpHeader->muCommon         = (luBytesPerIndex == 4u) ? 0xC0000002u : 0x20000002u;
        lpHeader->muReferenceCount = 1u;
        lpHeader->muField14        = 0xFFFF0000u;
        lpHeader->muBaseAddress    = ArenaAddress(lpBytes);
        lpHeader->muSizeRounded    = luSizeRounded;
        lpHeader->muIndexCount     = lpParams->muCount;
        return lpHeader;
    }

    int IndexBuffer::Lock(IndexBufferHeader* lpBuffer, char /*liFlags*/,
                          IndexBufferLockInfo* lpLockInfoOut)
    {
        if (lpBuffer == nullptr || lpLockInfoOut == nullptr || lpBuffer->muBaseAddress == 0u)
            return 0;
        lpLockInfoOut->mpBits = reinterpret_cast<void*>(
            static_cast<uintptr_t>(lpBuffer->muBaseAddress & ~3u));
        // The X360 reports 1 for 16-bit indices and 6 for 32-bit (the stride code the
        // lock descriptor carries); bit 31 of muCommon is the width selector.
        lpLockInfoOut->muStride = ((lpBuffer->muCommon & 0x80000000u) != 0u) ? 6u : 1u;
        return 1;
    }

    int IndexBuffer::Unlock(IndexBufferHeader* /*lpBuffer*/, IndexBufferLockInfo* /*lpLockInfo*/)
    {
        return 1;   // arena memory stays mapped; nothing to release
    }

    int D3DVertexBuffer_Lock(VertexBufferHeader* lpThis, int /*liOffsetToLock*/,
                             int /*liSizeToLock*/, int /*liFlags*/)
    {
        if (lpThis == nullptr)
            return 0;
        return static_cast<int>(lpThis->muBaseAddress & ~3u);
    }

    void D3DVertexBuffer_Unlock(VertexBufferHeader* /*lpThis*/)
    {
    }

    void VertexBuffer_Xbox2UnSet(VertexBufferHeader* /*lpBuffer*/)
    {
        // X360: drop the buffer's GPU-bound state before a CPU lock. Nothing on this
        // backend holds a binding on the arena memory (the draw path copies through
        // DrawIndexedPrimitiveUP), so there is nothing to unset.
    }
}

namespace shadow
{
    // The three free-function program binders the immediate-mode TUs call
    // (CgsIm3dSkyDome.cpp declares the first two; the third closes the gap those TUs
    // left -- see the note below). Each forwards to the shadow device's own typed
    // entry point so the module shadow the X360 keeps in the SAME .data block
    // (off_83010950..) stays coherent with what is actually bound.
    void DeviceSetVertexProgramInternal(void* lpVertexProgram)
    {
        Device::SetVertexProgram(
            static_cast<const renderengine::ProgramBufferData*>(lpVertexProgram));
    }

    void DeviceSetPixelProgram(void* lpPixelProgram)
    {
        Device::SetPixelProgram(
            static_cast<const renderengine::ProgramBufferData*>(lpPixelProgram));
    }

    // ⭐ The X360 module statics CgsIm3dSkyDome.cpp models as three TU-local variables
    // (off_83010958 "last vertex descriptor", dword_8301095C "last vertex program",
    // byte_83010A34 "vertex program state dirty") are NOT TU-local: they are
    // shadow::Device's OWN fields -- mpVertexDescriptor / mpVertexProgramShadow /
    // mbVertexProgramStateDirty sit at exactly those addresses in the off_83010950
    // block (see shadowingdevice.h). Modelling them separately left
    // Device::mpVertexDescriptor null, and Device::FlushVertexProgramState
    // unconditionally dereferences it (`mpVertexDescriptor->mpDeclaration`) -- so the
    // sky's own flush would have been a null dereference. This is the missing binder.
    void DeviceSetVertexDescriptor(void* lpVertexDescriptor)
    {
        Device::SetVertexDescriptor(
            static_cast<const renderengine::VertexDescriptorData*>(lpVertexDescriptor));
    }
}

namespace CgsGraphics
{
    // X360 dword_83010F9C. The immediate-mode layer's "which renderer owns the device
    // right now" static; BeginRendering asserts it is null and claims it, EndRendering
    // asserts it is this and releases it.
    ImRendererBase* ImRendererBase::mgpActiveRenderer = nullptr;

    // Build the shared immediate-mode render-state library exactly once (X360 home
    // CgsImRenderer.cpp, called behind a module-static guard from ImRenderer<V>::
    // Construct).
    //
    // FLAG PC-platform leaf: the library's twenty entries are renderengine BlendState /
    // RasterizerState / DepthStencilState / SamplerState / TextureState objects built
    // through the rw resource pipeline, and that pipeline is not reconstructed on this
    // backend (pc/gcm/renderengine/DepthStencilState.cpp and RasterizerState.cpp do not
    // even compile against the committed renderstates.h -- `maState` / `mauFlagWords`
    // are not members). Nothing in the mounted set READS the library: the sky binds its
    // own four state objects through the ImDeviceSet*State appliers above, which take
    // the state pointer directly. So this is a no-op until the state-object wave lands,
    // and deliberately NOT an assert -- it runs on the first immediate-mode Construct.
    //
    // FLAG PC-platform leaf: the renderengine state-object resource pipeline this builds
    // through has no PC backend yet, and nothing in the linked set reads the library.
    void ImRendererBase::ConstructOnceOnly(rw::IResourceAllocator* /*lpAllocator*/)
    {
    }
}
