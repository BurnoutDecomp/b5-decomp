#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/VertexProgramState.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

struct RenderableMesh;                                   // renderablemesh.h
namespace CgsGraphics { struct MaterialTechniqueView; }  // CgsDispatcherCommands.h

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

        // Bind a sampler state object through the shadow cache (X360 0x822769E0).
        static void* SetState(void* lpState, u32 luSamplerId);

        // Bind a texture (resource) through the shadow cache (X360 0x82276C70).
        static void* SetResource(void* lpTexture, u32 luSamplerId);

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

        // Set a low-level render state through the shadow cache (the immediate-mode SetState path,
        // X360 0x82276D08 calls this). lbWasUnset is true when no state had been set yet (the X360
        // passes (last == 0) so the device can take the full-set path). Returns the device/result
        // pointer (X360 r3 passthrough). Body is the X360 shadow device's low-level setter.
        static void* Xbox2SetStateLowLevelShadowed(void* lpState, bool lbWasUnset);

        // ---- The mesh-dispatch flush seam (DispatchList::DispatchAllMeshes) ----
        // The X360 walk resets the program shadows inline at each walk start
        // (dword_8301095C = 0 / dword_83010960 = -1); reproduced as this named reset.
        static void ResetProgramShadows();

        // [PC leaf] Bind everything the current technique implies: the render-state
        // triple, the vertex/pixel programs and the technique shader constants.
        // On the bring-up path (converted SHADERS bundle not yet loaded / constant
        // dispatch not yet reconstructed) this binds the FLAGGED fallback world
        // shader and per-pass default states instead -- see shadowingdevice.cpp.
        static void SetMeshTechniquePC(const CgsGraphics::MaterialTechniqueView* lpTechnique,
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
        static u32 muUnused64;                    // dword_83010964 (reset to 0, otherwise unread here)

        // --- Per-sampler caches (X360 16-entry arrays) --------------------------------------
        static u32 mauSamplerDirty[KU_MAX_TEXTURE_STATES];   // dword_83010968
        static void* mapSamplerState[KU_MAX_TEXTURE_STATES]; // dword_830109A8
        static void* mapSamplerTexture[KU_MAX_TEXTURE_STATES];// dword_830109E8

        // --- Force-stencil / lock window state ----------------------------------------------
        static u32 muMisc28;                      // dword_83010A28 (cleared by stencil window ops)
        static u32 muMisc2C;                      // dword_83010A2C
        static u32 muMisc30;                      // dword_83010A30
        static bool mbForceStencilWrite;          // byte_83010906
        static u32 muStencilValueToWrite;         // dword_83010908
        static bool mbRasteriserStateLocked;      // mbRasteriserStateLocked

        // --- Low-level blend/colour-write/alpha render-state shadow (X360 dword_83010730..774).
        // 18 contiguous dwords mirroring the D3D render-state block Xbox2SetStateLowLevelShadowed
        // pushes; modelled as a named array so the setter compares & stores by index.
        static u32 mauLowLevelStateShadow[18];    // dword_83010730 .. dword_83010774
    };
}
