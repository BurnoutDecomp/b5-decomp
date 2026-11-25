#pragma once

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/meshhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"

struct RenderableMesh;                                   // renderablemesh.h
namespace CgsGraphics { struct MaterialTechniqueView; struct ImRendererBase; }  // CgsDispatcherCommands.h

// The three render-state objects the triple binds. Pointer-only use in this header, so these are
// forward declarations rather than includes (the documented cascade-avoidance exception) --
// renderstates.h already forward-declares two of the three itself the same way. Class KEYS match the
// definitions exactly: `class DepthStencilState` (pc/gcm/renderengine/renderstates.h),
// `class RasterizerState` (same file), `struct BlendMaterialState`
// (SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h:35). The .cpp includes both real
// headers already, so the definitions are complete where the bodies need them.
namespace renderengine
{
    class  VertexProgramState;
    struct VertexDescriptorData;
    class  DepthStencilState;
    class  RasterizerState;
    class BlendState; typedef BlendState BlendMaterialState;
    class SamplerState;
    class  Texture;
    class  TextureState;      // pc/gcm/renderengine/renderstates.h -- the whole-unit bind (SetState overload)
}

namespace shadow
{
    class Device
    {
        friend struct CgsGraphics::ImRendererBase;
    public:
        struct DrawIndexedParameters
        {
            u32 mePrimitiveType;
            u32 muBaseVertexIndex;
            u32 muMinVertexIndex;
            u32 muNumVertices;
        };

        // The number of texture sampler / sampler-state slots the shadow cache mirrors
        // (X360 asserts luSamplerId < MaxTextureStates with 0x10).
        static const u32 KU_MAX_TEXTURE_STATES = 16;

        static bool Initialize();
        static bool FrameBegin();
        static void FrameEnd();
        static renderengine::VertexProgramState* GetVertexProgramState(
            const renderengine::ProgramBufferData* lpVertexProgram,
            const renderengine::VertexDescriptorData* lpVertexDescriptor);
        static void SetMeshBuffers(const RenderableMesh* lpMesh);
        static void DrawInstancedIndexedPrimitive_Custom(renderengine::DrawIndexedParameters& lrParameters,
            s32 luInstanceCount, s32 luInstanceMax);
        static void DrawIndexedMultipleStreams_Custom(
            const DrawIndexedParameters& lrParameters);

        // Reset the entire shadow cache (X360 0x82276970): clears the bound stream/index/descriptor
        // pointers, the vertex/pixel program shadows, the per-sampler state/texture caches and marks
        // the vertex-program state dirty so the next flush rebinds.
        static void ResetShadowing();

        // Bind a vertex program through the shadow cache (X360 0x82276BA0). Returns true when the
        // binding actually changed (the shadow differed and a flush was requested).
        static bool SetVertexProgram(const renderengine::ProgramBufferData* lpVertexProgram)
        {
            if (m_Shadow.m_pVertexProgramBuffer == lpVertexProgram)
                return false;

            SetVertexProgramInternal(lpVertexProgram);
            m_Shadow.m_pVertexProgramBuffer = lpVertexProgram;
            return true;
        }

        // Bind a pixel program through the shadow cache (X360 0x82276C00). Returns true when the
        // binding actually changed and a D3D pixel shader was bound.
        static bool SetPixelProgram(const renderengine::ProgramBufferData* lpPixelProgram);

        // Bind the active vertex descriptor through the shadow cache. The X360 writes
        // off_83010958 (this class's m_Shadow.m_pVertexDescriptor) and sets byte_83010A34 -- the
        // immediate-mode renderers do it inline, which is why no standalone symbol
        // survives; FlushVertexProgramState is the sole consumer and dereferences the
        // pointer, so a path that binds a program without also binding its descriptor is
        // a null dereference. Returns true when the binding actually changed.
        static bool SetVertexDescriptor(const renderengine::VertexDescriptorData* lpVertexDescriptor);

        // Bind a sampler state object through the shadow cache (X360 0x822769E0).
        static void SetState(const renderengine::SamplerState* lpState, u32 luSamplerId);

        // Bind a texture (resource) through the shadow cache (X360 0x82276C70).
        static void SetResource(renderengine::Texture* lpTexture, u32 luSamplerId);

        // Bind a WHOLE TextureState (sampler block + its raster) at one unit through the shadow
        // cache (X360 0x8227D158; the DecFIGS DWARF names it SetState(const TextureState*, u32),
        // shadowingdevice.h:918 is its one assert). Keyed on m_Shadow.m_apTextureStates[unit]: a hit only
        // re-asserts that the texture shadow still agrees; a miss applies the sampler block, sets
        // the raster, and writes ALL THREE per-unit shadows. Callers: BrnPostFxShader::Render
        // (the 3D tint volume + scene depth), BrnPostFxBloom, BrnSunCorona, Im3dBlend, the corona
        // renderer. No unit-range assert in this one -- the console has none (faithful).
        static void SetState(const renderengine::TextureState* lpState, u32 luSamplerId);

        // DecFIGS shadowingdevice.h:484; ARTIST 823FE530 inlines this
        // StateBlockShadow::m_pMeshState transition over the existing two homes.
        static void SetState(const renderengine::MeshHelper* lpMeshState)
        {
            if (m_Shadow.m_pMeshState != lpMeshState)
            {
                lpMeshState->Dispatch<renderengine::Device>(0);
                m_Shadow.m_pMeshState = lpMeshState;
                m_Shadow.m_pMesh = 0;
            }
        }

        // ARTIST 823F9284..92A4; StateBlockShadow::m_pRenderTargetState.
        static void SetState(const renderengine::RenderTargetState* lpState)
        {
            if (m_Shadow.m_pRenderTargetState != lpState)
            {
                renderengine::Device::SetState(lpState);
                m_Shadow.m_pRenderTargetState = lpState;
            }
        }

        // ⭐ THE PER-UNIT TEXTURE SHADOW, REACHED BY NAME (2026-09-05).
        // On the X360 dword_830109E8 / dword_83010968 are FILE-SCOPE GLOBALS, so a body outside
        // this class can read and write them directly -- and exactly one does:
        //     BrnParticle::LionParticleRender::SetMaterial @0x822896B8
        //       0x82289744  lwz r11, dword_830109E8(r30)   ; the CACHE TEST
        //       0x82289748  cmplw cr6, r11, r31            ; ... against the texture it resolved
        //       0x82289768  bl   D3DDevice_SetTexture      ; on a miss, bind unit 0 directly
        //       0x82289770  stw  r31, dword_830109E8(r30)  ; m_Shadow.m_apTextures[0] = the texture
        //       0x82289774  stw  r11(=0), dword_83010968(r30) ; m_Shadow.m_apTextureStates[0]  = NULL
        // On the host those two arrays are private statics of this class, so without this pair
        // that body has to keep a PRIVATE COPY of the cache -- and a private copy of shared state
        // is only correct until something else binds the same unit between two of its draws.
        // MEASURED, and it is why this pair exists: with the quarter-res particle chain live,
        // BrnRendererMemory::BlitDepth binds the scene DEPTH texture at unit 0 between frames, the
        // forked copy never noticed, SetMaterial skipped its bind, and every boost-plume quad
        // sampled the DEPTH BUFFER -- a hard-edged block carrying the car's own silhouette, with
        // nothing in any log.
        static void* GetSamplerTextureShadow(u32 luSamplerId);
        // The console's PAIR of stores, together: m_Shadow.m_apTextures[unit] = lpTexture and
        // m_Shadow.m_apTextureStates[unit] = NULL (binding half of a unit invalidates the whole-unit key --
        // see the m_Shadow.m_apTextureStates banner below). Call it immediately after a direct
        // D3DDevice_SetTexture, exactly where SetMaterial's two `stw`s are.
        static void  SetSamplerTextureShadow(u32 luSamplerId, void* lpTexture);

        // Rebind the dirty vertex-program / stream sources to D3D (X360 0x827E7A10).
        static void FlushVertexProgramState();

        // Force-stencil-write window (X360 0x823F3250 / 0x823F32E0).
        static void BeginForceStencilWrite(u32 luStencilValueToWrite);
        static void EndForceStencilWrite();

        // Rasteriser-state lock window (X360 0x823F3190 / 0x823F31F0).
        static void LockRasteriserState();
        static void UnlockRasteriserState();

        // Depth/stencil-state lock window. DWARF-declared beside the rasteriser pair
        // (shadowingdevice.h:556/557, dwarfdump file lines 363/366:
        // `void LockDepthStencilState(); void UnlockDepthStencilState();`) and INLINED by the X360
        // compiler at every call site, so unlike the rasteriser pair neither carries a standalone
        // symbol -- the census is the whole export set:
        //     $ python -c "...select name from func where name like 'shadow::Device::%Lock%'"
        //     ('shadow::Device::LockRasteriserState',   'class:shadow::Device', '0x823F3190')
        //     ('shadow::Device::UnlockRasteriserState', 'class:shadow::Device', '0x823F31F0')
        // The bodies are recovered from the CALLER instead, BrnRendererModule::Render @0x8240BFA8's
        // env-map face loop, where the compiler emitted them inline with their asserts:
        //     Lock    0x8240CC6C-0x8240CCAC  `lbz mbDepthStencilStateLocked` / assert
        //                                    "false == mbDepthStencilStateLocked"
        //                                    (shadowingdevice.h:1235) / `stb 1`
        //     Unlock  0x8240CCF4-0x8240CD30  `lbz` / assert "true == mbDepthStencilStateLocked"
        //                                    (shadowingdevice.h:1240) / `stb 0`
        static void LockDepthStencilState();
        static void UnlockDepthStencilState();

        // (FlushDepthStencilState / FlushRasterizerState DELETED -- they were INVENTED.) Neither
        // name exists in the X360 export set: scanning all 30,095 exported function names for a
        // shadow::Device name containing "Flush" returns exactly ONE symbol,
        // shadow::Device::FlushVertexProgramState @0x827E7A10. Neither appears anywhere in the
        // DecFIGS DWARF either (`grep -rn "FlushDepthStencilState\|FlushRasterizerState"
        // references/DecFIGS/dwarfdump/` -> no output; the class outline in
        // dwarfdump/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h has only
        // FlushBeforeRendering and FlushVertexProgramState). Their ONE caller,
        // BrnPostFxBloom::Render, stood for the two INLINED SetState calls the console makes at
        // 0x82402B50-0x82402BAC and 0x82402BB0-0x82402BEC -- the depth/stencil and rasteriser
        // thirds of the render-state triple, applied to dword_83010910 / dword_83010A40, i.e. the
        // same two objects and the same two setters BrnPostFx::Render already uses by name
        // (BrnPostFx.cpp:716-717). A second host spelling of a setter this class already owns is
        // the split-brain the mpLast* note further down records the cost of, so the caller was
        // moved onto SetState and these two were removed rather than bodied.

        // ---- The render-state TRIPLE setters, one per third -------------------------------------
        // Three STANDALONE X360 functions, not a pattern: 0x82276AD0, 0x82276B38, 0x82276A68. DWARF
        // names them shadow::Device::SetState(const DepthStencilState*) / (const RasterizerState*) /
        // (const BlendState*) (source shadowingdevice.h:499 / :503 / :496). Each body is exactly:
        // lock gate -> pointer compare against its own slot of the file-scope shadow block
        // off_83010950 -> low-level apply with lbWasUnset == (cached == nullptr) -> cache the wanted
        // state. The cache write-back sits INSIDE the compare, exactly as the asm's `stw` follows the
        // `bl` and both skip branches jump past it.
        //
        // These are what BrnRendererModule::EndRenderPostFx @0x823F65B0, EndRenderAntiAliased
        // @0x82408B00, BeginQuarterResBuffer @0x82408C38 and BeginRenderEnvironmentMapFace
        // @0x823F63E0 expand three at a time, and what CgsGraphics::ImRendererBase::SetState
        // @0x82276D08 / 0x82276DA8 / 0x82276E48 expand one at a time behind their mgpActiveRenderer
        // assert.
        //
        // Return type is void (DWARF). The standalone bodies appear to return r3 because the tail
        // applier leaves something there; no caller reads it and the expanded forms discard it.
        // Bodies: shadowingdevice.cpp, immediately after Xbox2SetRasterizerStateLowLevelShadowed.
        static void SetState(const renderengine::DepthStencilState* lpState);   // @0x82276AD0
        static void SetState(const renderengine::RasterizerState* lpState);     // @0x82276B38
        static void SetState(const renderengine::BlendMaterialState* lpState);  // @0x82276A68

        // Set a low-level render state through the shadow cache (the immediate-mode SetState path,
        // X360 0x82276D08 calls this). lbWasUnset is true when no state had been set yet (the X360
        // passes (last == 0) so the device can take the full-set path). Returns the device/result
        // pointer (X360 r3 passthrough). Body is the X360 shadow device's low-level setter.
        static void* Xbox2SetStateLowLevelShadowed(void* lpState, bool lbWasUnset);

        // The depth/stencil and rasteriser twins of the setter above (X360 @0x827E8150 and
        // @0x827E8690): the other two thirds of the render-state triple DispatchAllMeshes
        // binds on every technique change.
        static void* Xbox2SetDepthStencilStateLowLevelShadowed(void* lpState, bool lbWasUnset);
        static void* Xbox2SetRasterizerStateLowLevelShadowed(void* lpState, bool lbWasUnset);

        // ---- The mesh-dispatch flush seam (DispatchList::DispatchAllMeshes) ----
        // The X360 walk resets the program shadows inline at each walk start
        // (dword_8301095C = 0 / dword_83010960 = -1); reproduced as this named reset.
        // ARTIST 827EEA04, 827F2754 and 827F7690 reset the program pair.
        static void ResetShadowingForShaders()
        {
            m_Shadow.m_pVertexProgramBuffer = 0;
            m_Shadow.m_pPixelProgramBuffer = reinterpret_cast<const renderengine::ProgramBufferData*>(~uintptr_t(0));
        }

    private:
        static void SetVertexProgramInternal(const renderengine::ProgramBufferData* lpVertexProgram);

        // DecFIGS shadowingdevice.h:657–672/771. ARTIST ResetShadowing
        // at 0x82276970 accesses the one thirteen-member block at 0x83010950.
        struct StateBlockShadow
        {
            const RenderableMesh* m_pMesh;
            const renderengine::MeshHelper* m_pMeshState;
            const renderengine::VertexDescriptorData* m_pVertexDescriptor;
            const renderengine::ProgramBufferData* m_pVertexProgramBuffer;
            const renderengine::ProgramBufferData* m_pPixelProgramBuffer;
            const renderengine::BlendState* m_pBlendState;
            const renderengine::TextureState* m_apTextureStates[KU_MAX_TEXTURE_STATES];
            const renderengine::SamplerState* m_apSamplerStates[KU_MAX_TEXTURE_STATES];
            const renderengine::Texture* m_apTextures[KU_MAX_TEXTURE_STATES];
            const renderengine::DepthStencilState* m_pDepthStencilState;
            const renderengine::RasterizerState* m_pRasterizerState;
            const renderengine::RenderTargetState* m_pRenderTargetState;
            bool mbVertexProgramStateDirty;
        };

        CGS_STATIC_ASSERT((sizeof(StateBlockShadow) == 0xE8), "ARTIST state shadow size");
        CGS_STATIC_ASSERT((offsetof(StateBlockShadow, m_apTextureStates) == 0x18), "ARTIST texture-state cache");
        CGS_STATIC_ASSERT((offsetof(StateBlockShadow, m_apSamplerStates) == 0x58), "ARTIST sampler-state cache");
        CGS_STATIC_ASSERT((offsetof(StateBlockShadow, m_apTextures) == 0x98), "ARTIST texture cache");
        CGS_STATIC_ASSERT((offsetof(StateBlockShadow, m_pDepthStencilState) == 0xD8), "ARTIST depth shadow");
        CGS_STATIC_ASSERT((offsetof(StateBlockShadow, m_pRenderTargetState) == 0xE0), "ARTIST render-target shadow");
        CGS_STATIC_ASSERT((offsetof(StateBlockShadow, mbVertexProgramStateDirty) == 0xE4), "ARTIST dirty flag");

        static StateBlockShadow m_Shadow;

        static bool mbForceStencilWrite;          // byte_83010906
        static u32 muStencilValueToWrite;         // dword_83010908
        static bool mbRasteriserStateLocked;      // mbRasteriserStateLocked
        // byte_83010907. The blend third's lock gate (`lbz r11, byte_83010907@l(r11)` @0x82276A84 in
        // SetState @0x82276A68, and the same byte @0x82276D54 in ImRendererBase::SetState
        // @0x82276D08). Identified by closed-set elimination: the three sibling setters gate on three
        // bytes, IDA names two of them mbDepthStencilStateLocked (@0x82276AEC) and
        // mbRasteriserStateLocked (@0x82276B54), and DWARF gives shadow::Device exactly three lock
        // bools -- mbRasteriserStateLocked / mbBlendStateLocked / mbDepthStencilStateLocked (source
        // shadowingdevice.h:775/776/777). The third gate is therefore the third bool. It stayed
        // unnamed in the XEX because the two IDA named got their names from assert strings and this
        // one has no Lock/Unlock pair with asserts anywhere in the image.
        // MOVED here from CgsGraphics::ImRendererBase::mgbStateShadowingDisabled, which was a host
        // spelling of the same byte (DWARF's CgsImRenderer.h declares no such member). See edits
        // 04/05 -- a MOVE, never a second declaration.
        static bool mbBlendStateLocked;           // byte_83010907

        // --- Low-level blend/colour-write/alpha render-state shadow (X360 dword_83010730..774).
        // 18 contiguous dwords mirroring the D3D render-state block Xbox2SetStateLowLevelShadowed
        // pushes; modelled as a named array so the setter compares & stores by index.
        static u32 mauLowLevelStateShadow[18];    // dword_83010730 .. dword_83010774

        // The depth/stencil lock window DispatchAllMeshes tests alongside
        // mbRasteriserStateLocked (its Lock/UnlockDepthStencilState pair @0x823F3250/@0x823F32B0
        // is reconstructed below, reflections step 1).
        static bool mbDepthStencilStateLocked;

        // (mpLastBlendState / mpLastDepthStencilState / mpLastRasterizerState DELETED.)
        // They were a SECOND host home for dword_83010964 / dword_83010A28 / dword_83010A2C, which
        // m_Shadow.m_pBlendState / m_Shadow.m_pDepthStencilState / m_Shadow.m_pRasterizerState above already own. The comment that
        // stood here -- "DispatchAllMeshes v64[5] / v64[54] / v64[55] ... the per-dispatch context" --
        // was also wrong: in DispatchAllMeshes @0x827F2718 those slots are reached as 0xD8 / 0x14 /
        // 0xDC off r27, and r27 IS off_83010950 (`addi r11, r11, off_83010950@l` @0x827F2754 ->
        // `stw r11, var_140(r1)` @0x827F2760 -> `lwz r27, var_140(r1)` @0x827F2EE0). They are the
        // file-scope shadow block, not per-walk state. The consequence of the duplication was live,
        // not cosmetic: ResetShadowing and BeginForceStencilWrite/EndForceStencilWrite zero the
        // muMisc*/muUnused64 copy while SetMaterialRenderStatesPC compared the mpLast* copy, so a
        // console invalidation was invisible to the dispatch walk and vice versa. Every former
        // mpLast* reader now uses the single slot.
    };
}
