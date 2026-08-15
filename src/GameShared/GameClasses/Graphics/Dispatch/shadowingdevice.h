#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/VertexProgramState.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

struct RenderableMesh;                                   // renderablemesh.h
namespace CgsGraphics { struct MaterialTechniqueView; }  // CgsDispatcherCommands.h

// The three render-state objects the triple binds. Pointer-only use in this header, so these are
// forward declarations rather than includes (the documented cascade-avoidance exception) --
// renderstates.h already forward-declares two of the three itself the same way. Class KEYS match the
// definitions exactly: `class DepthStencilState` (pc/gcm/renderengine/renderstates.h),
// `class RasterizerState` (same file), `struct BlendMaterialState`
// (SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h:35). The .cpp includes both real
// headers already, so the definitions are complete where the bodies need them.
namespace renderengine
{
    class  DepthStencilState;
    class  RasterizerState;
    struct BlendMaterialState;
    class  TextureState;      // pc/gcm/renderengine/renderstates.h -- the whole-unit bind (SetState overload)
}

namespace shadow
{
    class Device
    {
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
        static renderengine::VertexProgramState* GetVertexProgramState(
            const renderengine::ProgramBufferData* lpVertexProgram,
            const renderengine::VertexDescriptorData* lpVertexDescriptor);
        static void SetVertexProgramInternal();
        static void DrawIndexedMultipleStreams_Custom(
            const DrawIndexedParameters& lrParameters);

        // Reset the entire shadow cache (X360 0x82276970): clears the bound stream/index/descriptor
        // pointers, the vertex/pixel program shadows, the per-sampler state/texture caches and marks
        // the vertex-program state dirty so the next flush rebinds.
        static void ResetShadowing();

        // Bind a vertex program through the shadow cache (X360 0x82276BA0). Returns true when the
        // binding actually changed (the shadow differed and a flush was requested).
        static bool SetVertexProgram(const renderengine::ProgramBufferData* lpVertexProgram);

        // Bind a pixel program through the shadow cache (X360 0x82276C00). Returns true when the
        // binding actually changed and a D3D pixel shader was bound.
        static bool SetPixelProgram(const renderengine::ProgramBufferData* lpPixelProgram);

        // Bind the active vertex descriptor through the shadow cache. The X360 writes
        // off_83010958 (this class's mpVertexDescriptor) and sets byte_83010A34 -- the
        // immediate-mode renderers do it inline, which is why no standalone symbol
        // survives; FlushVertexProgramState is the sole consumer and dereferences the
        // pointer, so a path that binds a program without also binding its descriptor is
        // a null dereference. Returns true when the binding actually changed.
        static bool SetVertexDescriptor(const renderengine::VertexDescriptorData* lpVertexDescriptor);

        // Bind a sampler state object through the shadow cache (X360 0x822769E0).
        static void* SetState(void* lpState, u32 luSamplerId);

        // Bind a texture (resource) through the shadow cache (X360 0x82276C70).
        static void* SetResource(void* lpTexture, u32 luSamplerId);

        // Bind a WHOLE TextureState (sampler block + its raster) at one unit through the shadow
        // cache (X360 0x8227D158; the DecFIGS DWARF names it SetState(const TextureState*, u32),
        // shadowingdevice.h:918 is its one assert). Keyed on mapTextureState[unit]: a hit only
        // re-asserts that the texture shadow still agrees; a miss applies the sampler block, sets
        // the raster, and writes ALL THREE per-unit shadows. Callers: BrnPostFxShader::Render
        // (the 3D tint volume + scene depth), BrnPostFxBloom, BrnSunCorona, Im3dBlend, the corona
        // renderer. No unit-range assert in this one -- the console has none (faithful).
        static void* SetState(const renderengine::TextureState* lpState, u32 luSamplerId);

        // Rebind the dirty vertex-program / stream sources to D3D (X360 0x827E7A10).
        static void FlushVertexProgramState();

        // Force-stencil-write window (X360 0x823F3250 / 0x823F32E0).
        static void* BeginForceStencilWrite(u32 luStencilValueToWrite);
        static void* EndForceStencilWrite();

        // Rasteriser-state lock window (X360 0x823F3190 / 0x823F31F0).
        static void* LockRasteriserState();
        static void* UnlockRasteriserState();

        static void FlushDepthStencilState();
        static void FlushRasterizerState();

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
        static void ResetProgramShadows();

        // [PC leaf] Bind everything the current technique implies: the render-state
        // triple, the vertex/pixel programs and the technique shader constants.
        // On the bring-up path (converted SHADERS bundle not yet loaded / constant
        // dispatch not yet reconstructed) this binds the FLAGGED fallback world
        // shader and per-pass default states instead -- see shadowingdevice.cpp.
        // lpMaterialAssembly is the walk's MaterialAssembly (the serialised Material blob
        // the technique came out of) -- the X360 reaches the same object through the
        // register it already holds; the PC seam takes it explicitly because the sampler
        // table (+0x0C) lives on the assembly, not the technique.
        static void SetMeshTechniquePC(const CgsGraphics::MaterialTechniqueView* lpTechnique,
                                       const void* lpMaterialAssembly,
                                       void* const* lppConstScratch, bool lbZOnly);

        // Bind the technique's render-state triple (blend / depth-stencil / rasteriser
        // objects reached through the technique's +0x04 MaterialState slot).
        //
        // lbZOnly selects the DEPTH-ONLY variant the Z-only interpreter uses
        // (DrawRenderableMeshZOnly::Interpret @0x827F5AC8): the console binds the same
        // depth-stencil and rasteriser objects out of the technique's MaterialState but
        // replaces the BLEND object with one of two engine-wide Z-only blend states
        // (dword_83010F8C / dword_83010F90, selected by the technique's alpha-test flag).
        static void SetMaterialRenderStatesPC(const CgsGraphics::MaterialTechniqueView* lpTechnique,
                                              bool lbZOnly);

        // [PC leaf] The PER-MESH half of the same inlined X360 block: the technique's two
        // OBJECT-scope external constant blocks (ShaderTechnique +0x1C vertex, +0x50 pixel),
        // whose sources are the head of the same scratch table. Called for every mesh, right
        // after the technique bind, exactly where the X360 body runs them.
        static void SetMeshObjectConstantsPC(const CgsGraphics::MaterialTechniqueView* lpTechnique,
                                             void* const* lppConstScratch, bool lbZOnly);

        // [PC bring-up shim] Upload the per-object world-view-projection carried in
        // the PC mesh command (see CgsDispatcherCommands.cpp header note).
        static void SetObjectTransformPC(const f32* lpWvpRows16);

        // [PC leaf] Bind the mesh's index/vertex buffers + the vertex declaration
        // for the given technique index (the X360 shadow::Device::SetMeshBuffers
        // @0x827E68A0, whose export was not decompiled; PC condenses the Xenon
        // stream-shadow dance into direct binds through the D3DDevice_* seam).
        static void SetMeshBuffersPC(const RenderableMesh* lpMesh, u32 luTechniqueIndex);

        // [PC leaf] Issue the mesh's indexed draw (mDrawIndexedParameters).
        static void DrawIndexedMeshPC(const RenderableMesh* lpMesh);

    private:
        // [PC leaf] Bind the samplers the TECHNIQUE names: technique+0x22 binding count,
        // +0x24 index list (built by MaterialResourceType::PostFixUp) into the material
        // assembly's own 20-byte sampler array at +0x0C.
        static void BindTechniqueSamplers(const u8* lpTechnique, const u8* lpMaterialAssembly);

        // The X360 build keeps the live vertex-program binding as a small static
        // slot (5 dwords @ dword_83011118) whose first entry points at the real
        // state object (@ unk_83010920); InitializeNoBindX360 dereferences the
        // slot to reach it. Both are modelled here by name.
        static renderengine::VertexProgramState mVertexProgramState;
        static renderengine::VertexProgramState* mapVertexProgramStateSlot[5];
        // The vertex-program-state dirty flag (X360 byte_83010A34): ResetShadowing sets it,
        // SetVertexProgramInternal sets it, FlushVertexProgramState consumes & clears it.
        static bool mbVertexProgramStateDirty;

        // --- Bound geometry sources (X360 off_83010950 block) -------------------------------
        // mpStreamArray / mpIndexStreamArray are the two alternative stream-layout descriptors the
        // flush walks (the first non-null one wins); mpVertexDescriptor is the active vertex
        // descriptor used to map element streams to D3D stream slots.
        static const void* mpStreamArray;        // off_83010950
        static const void* mpIndexStreamArray;   // off_83010954
        static const renderengine::VertexDescriptorData* mpVertexDescriptor; // off_83010958

        // --- Program shadows (X360 dword_8301095C / dword_83010960) -------------------------
        static const renderengine::ProgramBufferData* mpVertexProgramShadow; // dword_8301095C
        static const renderengine::ProgramBufferData* mpPixelProgramShadow;  // dword_83010960 (init -1)
        // dword_83010964 == StateBlockShadow::m_pBlendState (DWARF source shadowingdevice.h:665).
        // RENAMED AND RETYPED from `u32 muUnused64` -- it was never unused: the blend third of the
        // render-state triple (SetState @0x82276A68) compares and stores it, and so does the
        // immediate-mode setter ImRendererBase::SetState @0x82276D08 (`lwz r11, (dword_83010964 -
        // 0x83010950)(r31)` @0x82276D68 / `stw r30, ...` @0x82276D88). The address is fixed by walking
        // StateBlockShadow's thirteen DWARF members from the block base off_83010950: they land on
        // 950/954/958/95C/960/964, 968..9A4, 9A8..9E4, 9E8..A24, A28, A2C, A30, A34 -- thirteen for
        // thirteen.
        static const renderengine::BlendMaterialState* mpBlendState;   // dword_83010964

        // --- Per-sampler caches (X360 16-entry arrays) --------------------------------------
        // RENAMED AND RETYPED from `u32 mauSamplerDirty` (2026-08-14). dword_83010968 is not a dirty
        // flag: it is the WHOLE-TextureState shadow (DWARF StateBlockShadow::m_apTextureStates), a
        // pointer per unit. SetState(const TextureState*, u32) @0x8227D158 compares the incoming
        // object against it (`lwz r11, dword_83010968[a2]; cmplw r11, r3`) and stores it on a miss;
        // the bare-sampler binder @0x822769E0 and the bare-texture binder @0x82276C70 both write it
        // to ZERO -- binding half of a unit invalidates the whole-unit key, which is exactly why a
        // flag reading "looked right": every writer this tree had was a writer of zero. On the host
        // a u32 cannot hold the pointer the console stores, so the type follows the console.
        static const renderengine::TextureState* mapTextureState[KU_MAX_TEXTURE_STATES]; // dword_83010968
        static void* mapSamplerState[KU_MAX_TEXTURE_STATES]; // dword_830109A8
        static void* mapSamplerTexture[KU_MAX_TEXTURE_STATES];// dword_830109E8

        // --- Force-stencil / lock window state ----------------------------------------------
        // dword_83010A28 / dword_83010A2C == StateBlockShadow::m_pDepthStencilState /
        // m_pRasterizerState (DWARF source shadowingdevice.h:669 / :670), renamed and retyped from
        // `u32 muMisc28` / `muMisc2C`. Zeroing them IS the triple's invalidation, which is why
        // ResetShadowing @0x82276970 (`stw r10(=0), (dword_83010A28 - 0x83010950)(r11)` @0x822769C8 /
        // ...A2C @0x822769CC) and the force-stencil window (@0x823F32D4) write them -- and why the
        // frame bracket's triple has to compare against THESE and not a parallel copy.
        static const renderengine::DepthStencilState* mpDepthStencilState;  // dword_83010A28
        static const renderengine::RasterizerState*   mpRasterizerState;    // dword_83010A2C
        // dword_83010A30 == StateBlockShadow::m_pRenderTargetState (DWARF source
        // shadowingdevice.h:671) by the same thirteen-member walk. Left as `u32 muMisc30` because
        // nothing in this pass reads or writes it beyond ResetShadowing's zero (@0x822769D4); rename
        // it when a reader lands, not on spec.
        static u32 muMisc30;                      // dword_83010A30
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
        // mbRasteriserStateLocked (its own Lock/Unlock pair is not reconstructed yet).
        static bool mbDepthStencilStateLocked;

        // (mpLastBlendState / mpLastDepthStencilState / mpLastRasterizerState DELETED.)
        // They were a SECOND host home for dword_83010964 / dword_83010A28 / dword_83010A2C, which
        // mpBlendState / mpDepthStencilState / mpRasterizerState above already own. The comment that
        // stood here -- "DispatchAllMeshes v64[5] / v64[54] / v64[55] ... the per-dispatch context" --
        // was also wrong: in DispatchAllMeshes @0x827F2718 those slots are reached as 0xD8 / 0x14 /
        // 0xDC off r27, and r27 IS off_83010950 (`addi r11, r11, off_83010950@l` @0x827F2754 ->
        // `stw r11, var_140(r1)` @0x827F2760 -> `lwz r27, var_140(r1)` @0x827F2EE0). They are the
        // file-scope shadow block, not per-walk state. The consequence of the duplication was live,
        // not cosmetic: ResetShadowing and BeginForceStencilWrite/EndForceStencilWrite zero the
        // muMisc*/muUnused64 copy while SetMaterialRenderStatesPC compared the mpLast* copy, so a
        // console invalidation was invisible to the dispatch walk and vice versa. Every former
        // mpLast* reader now uses the single slot.
        // FLAG PC-platform leaf: the console's blend compare is against two DIFFERENT
        // objects (the material's own state for the colour passes, an engine-wide Z-only
        // state for the depth-only pass), so the object pointer alone identifies the
        // binding. The PC leaf derives the Z-only blend from the material's own state
        // (see SetMaterialRenderStatesPC), so the same pointer can stand for two
        // different bindings and the cache has to remember which one it was.
        static bool  mbLastBlendZOnly;
    };
}
