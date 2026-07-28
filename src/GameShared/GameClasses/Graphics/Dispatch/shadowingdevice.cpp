#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"

#include <cstddef>
#include <cstdint>

#include <cstring>                                                          // memcpy (serialised-slot reads)

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // the quiet one-shot gates
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"        // RenderableMesh (mesh-dispatch seam)
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // MaterialTechniqueView
#include "pc/gcm/renderengine/Device.h"

struct IDirect3DDevice9;

extern IDirect3DDevice9* gpD3DDevice;

extern "C" void D3DDevice_DrawIndexedVertices(IDirect3DDevice9* lpDevice,
                                               u32 lePrimitiveType,
                                               u32 luBaseVertexIndex,
                                               u32 luMinVertexIndex,
                                               u32 luNumVertices);

// --- Xbox 360 D3D fast-path binders the shadow device drives -----------------------------------
// These are the Xenon D3DDevice_* thunks the immediate-mode renderers bind through. No project TU
// homes them; declared here as the minimal extern surface the shadow cache calls. Signatures match
// the XDK d3d9 fast-set API and the precedent in CgsImRenderer.cpp.
extern "C"
{
    void D3DDevice_SetVertexShader(IDirect3DDevice9* lpDevice, void* lpShader);
    void D3DDevice_SetPixelShader(IDirect3DDevice9* lpDevice, void* lpShader);
    void D3DDevice_SetVertexDeclaration(IDirect3DDevice9* lpDevice, void* lpDecl);
    void D3DDevice_SetStreamSource(IDirect3DDevice9* lpDevice, u32 luStreamNumber,
                                   const void* lpStreamData, u32 luOffsetInBytes,
                                   u32 luStride, u32 luFlags);
    unsigned int D3DDevice_SetTexture(IDirect3DDevice9* lpDevice, u32 luSampler,
                                      void* lpTexture, unsigned int luFlags);

    void D3DDevice_SetBlendState(IDirect3DDevice9* lpDevice, u32 luRenderTargetIndex, u32 luBlendState);
    void D3DDevice_SetRenderState_ColorWriteEnable(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_ColorWriteEnable1(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_ColorWriteEnable2(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_ColorWriteEnable3(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_BlendFactor(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_AlphaToMaskEnable(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_AlphaToMaskOffsets(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_HighPrecisionBlendEnable(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_HighPrecisionBlendEnable1(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_HighPrecisionBlendEnable2(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_HighPrecisionBlendEnable3(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_AlphaTestEnable(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_AlphaRef(IDirect3DDevice9* lpDevice, u32 luValue);
    void D3DDevice_SetRenderState_AlphaFunc(IDirect3DDevice9* lpDevice, u32 luValue);
}

// The D3D "current vertex program state" global (X360 dword_832716C8): the last vertex program
// state object FlushVertexProgramState bound. Lives in the renderengine device layer; modelled by
// name here as the flush's write target.
renderengine::VertexProgramState* gpCurrentVertexProgramState = nullptr;

// The low-level sampler-state setter (X360 sub_827E8950) SetState forwards to. It is an external
// renderengine helper with no project home and no recovered asm for this TU; declared here so the
// shadow setter can call it by name. Returns the device/result pointer (X360 r3 passthrough).
extern "C" void* SetSamplerStateLowLevel(void* lpState, u32 luSamplerId, bool lbWasUnset);

// ---- The PC mesh-dispatch leaf hooks (pc/gcm/renderengine/XenonD3D9Shims.cpp) -----------------
// The Xenon-side geometry stash + fallback world shader. The dispatch walk binds through these;
// the shim TU owns the D3D9 objects (shader cache, vertex-declaration cache, draw stash).
namespace renderengine
{
    // Bind the FLAGGED fallback world shader (compiled once at first use); returns false when the
    // device/compiler is unavailable (the draw is skipped rather than issued undefined).
    bool WorldFallbackShader_Bind();
    // Upload the row-vector WVP (4x float4) for the fallback shader.
    void WorldFallbackShader_SetWvp(const f32* lpWvpRows16);
    // Bind a material's own sampler textures (the DATA half of the X360 technique bind --
    // DispatchAllMeshes' lpaInternalSamplers loop). Returns true when a texture was bound.
    bool WorldMaterialSamplers_Bind(const void* lpMaterialAssembly);
    // Pick the fallback pair (flat vs textured) for the mesh whose declaration was just
    // resolved -- see the note on Vd32Cached::mbHasTexcoord0 in the shim TU.
    void WorldFallbackShader_SelectForMesh();
    // Resolve + cache a D3D9 vertex declaration and the stream-0 stride from the 32-bit
    // serialised VertexDescriptor image the converted world data carries.
    void* WorldVd32_GetDeclaration(const void* lpVdImage, u32* lpuStride);
    // ---- the REAL per-technique shader path ----
    // Create-once + bind the technique's two D3D9 programs (each argument is a
    // ProgramBufferData + 0x14 payload). False = nothing bound, keep the fallback.
    bool  WorldPrograms_Bind(const void* lpVertexPayload, const void* lpPixelPayload);
    // Forget the real programs (the fallback pair owns the device again).
    void  WorldShader_ClearRealPrograms();
    // Whether the last technique bind left the real programs on the device.
    bool  WorldShader_RealProgramsBound();
    // Upload luNumRegisters float4s at shader-constant register luRegister.
    void  WorldShaderConstants_Set(bool lbPixel, u32 luRegister, const void* lpData, u32 luNumRegisters);
    // Bind one texture + the world sampler set at a D3D sampler unit.
    bool  WorldShader_BindTextureUnit(u32 luUnit, const void* lpRaster);
    // Per-technique alpha test (from the technique's own mu16Flags bit 3).
    void  WorldShader_SetAlphaTest(bool lbEnabled);
    // The draw stash the D3DDevice_* shims fill (index/vertex source for the UP draw path).
    void  WorldDraw_SetIndexSource(const void* lpIndexBufferHeader);
    void  WorldDraw_SetVertexSource(const void* lpVertexBufferHeader, u32 luStride);
    void  WorldDraw_IndexedUP(u32 luPrimTypeXenon, u32 luBaseVertexIndex,
                              u32 luStartIndex, u32 luIndexCount);
}

namespace
{
    // The two alternative bound-geometry stream descriptors FlushVertexProgramState walks. Their
    // byte layout is recovered from the X360 flush asm (no DWARF/leak source for these vendor
    // blocks); only the fields the flush reads are named, the rest is reserved padding.

    // mpStreamArray (X360 off_83010950): the stream-source pointer table. The flush reads the
    // active stream count from the byte at +0x26 and the per-stream data pointers from +0x2C on.
    // The X360 build stores 4-byte (guest) pointers; modelled as u32 words so the recovered byte
    // offsets stay exact (the data words are re-typed to const void* at the call site). Packed so
    // the LLP64 host does not realign the word table away from +0x2C.
#pragma pack(push, 1)
    struct StreamArray
    {
        u8 mauReserved00[0x26];        // +0x00
        u8 muStreamCount;              // +0x26  (byte; X360 lbz 0x26)
        u8 mauReserved27[0x05];        // +0x27  (pad up to the +0x2C data table)
        u32 mauStreamData[8];          // +0x2C  per-stream data pointers (guest 4-byte words)
    };
#pragma pack(pop)

    // mpIndexStreamArray (X360 off_83010954): a word-array descriptor. Word[1] is the stream count;
    // the per-stream data pointers live at word (word[0] + 2 + streamIndex).
    struct IndexStreamArray
    {
        u32 mauWords[32];              // word[0]=base index, word[1]=stream count, ...
    };

    // X360 stream-source flag: (1<<63) >> (((21846*(95-stream))>>16)+32), taken as its low 32 bits.
    inline u32 MakeStreamSourceFlag(u32 luStream)
    {
        const u32 luShift = ((21846u * (95u - luStream)) >> 16) + 32u;
        return static_cast<u32>((1ull << 63) >> luShift);
    }

    static_assert(offsetof(StreamArray, muStreamCount) == 0x26,
                  "StreamArray stream-count byte at +0x26");
    static_assert(offsetof(StreamArray, mauStreamData) == 0x2C,
                  "StreamArray data-pointer table at +0x2C");
}

namespace shadow
{
    renderengine::VertexProgramState Device::mVertexProgramState = {};
    renderengine::VertexProgramState* Device::mapVertexProgramStateSlot[5] = {};
    bool Device::mbVertexProgramStateDirty = false;

    const void* Device::mpStreamArray = nullptr;
    const void* Device::mpIndexStreamArray = nullptr;
    const renderengine::VertexDescriptorData* Device::mpVertexDescriptor = nullptr;

    const renderengine::ProgramBufferData* Device::mpVertexProgramShadow = nullptr;
    const renderengine::ProgramBufferData* Device::mpPixelProgramShadow = nullptr;
    u32 Device::muUnused64 = 0;

    u32 Device::mauSamplerDirty[KU_MAX_TEXTURE_STATES] = {};
    void* Device::mapSamplerState[KU_MAX_TEXTURE_STATES] = {};
    void* Device::mapSamplerTexture[KU_MAX_TEXTURE_STATES] = {};

    u32 Device::muMisc28 = 0;
    u32 Device::muMisc2C = 0;
    u32 Device::muMisc30 = 0;
    bool Device::mbForceStencilWrite = false;
    u32 Device::muStencilValueToWrite = 0;
    bool Device::mbRasteriserStateLocked = false;

    u32 Device::mauLowLevelStateShadow[18] = {};

    bool Device::Initialize()
    {
        const bool lbInitialised = renderengine::Device::Initialize();
        // @0x827ED6E0: the slot is reset to { &mVertexProgramState, 0, 0, 0, 0 }.
        mapVertexProgramStateSlot[0] = &mVertexProgramState;
        mapVertexProgramStateSlot[1] = nullptr;
        mapVertexProgramStateSlot[2] = nullptr;
        mapVertexProgramStateSlot[3] = nullptr;
        mapVertexProgramStateSlot[4] = nullptr;
        return lbInitialised;
    }

    renderengine::VertexProgramState* Device::GetVertexProgramState(
        const renderengine::ProgramBufferData* lpVertexProgram,
        const renderengine::VertexDescriptorData* lpVertexDescriptor)
    {
        // @0x827E7998: a two-entry Parameters block { program, descriptor } is
        // passed alongside the state slot; InitializeNoBindX360 dereferences the
        // slot to fill the live state object and returns it.
        const void* const lapParameters[] =
        {
            lpVertexProgram,
            lpVertexDescriptor
        };

        renderengine::VertexProgramState* lpVertexProgramState =
            renderengine::VertexProgramState::InitializeNoBindX360(
                mapVertexProgramStateSlot, lapParameters);
        CGS_ASSERT(lpVertexProgramState, "lpVertexProgramState");
        return lpVertexProgramState;
    }

    void Device::SetVertexProgramInternal()
    {
        mbVertexProgramStateDirty = true;
    }

    void Device::DrawIndexedMultipleStreams_Custom(
        const DrawIndexedParameters& lrParameters)
    {
        D3DDevice_DrawIndexedVertices(gpD3DDevice,
                                      lrParameters.mePrimitiveType,
                                      lrParameters.muBaseVertexIndex,
                                      lrParameters.muMinVertexIndex,
                                      lrParameters.muNumVertices);
    }

    // @0x82276970: clear the whole shadow cache.
    void Device::ResetShadowing()
    {
        mpStreamArray = nullptr;
        mpIndexStreamArray = nullptr;
        mpVertexDescriptor = nullptr;
        mpVertexProgramShadow = nullptr;
        // dword_83010960 is reset to -1 (the "no pixel program bound" sentinel).
        mpPixelProgramShadow = reinterpret_cast<const renderengine::ProgramBufferData*>(
            static_cast<uintptr_t>(~0u));
        muUnused64 = 0;

        for (u32 luSampler = 0; luSampler < KU_MAX_TEXTURE_STATES; ++luSampler)
        {
            mauSamplerDirty[luSampler] = 0;
            mapSamplerState[luSampler] = nullptr;
            mapSamplerTexture[luSampler] = nullptr;
        }

        muMisc28 = 0;
        muMisc2C = 0;
        muMisc30 = 0;
        mbVertexProgramStateDirty = true;
    }

    // @0x82276BA0: bind a vertex program through the shadow.
    bool Device::SetVertexProgram(const renderengine::ProgramBufferData* lpVertexProgram)
    {
        if (mpVertexProgramShadow == lpVertexProgram)
        {
            return false;
        }
        SetVertexProgramInternal();
        mpVertexProgramShadow = lpVertexProgram;
        return true;
    }

    // @0x82276C00: bind a pixel program through the shadow. The X360 build keeps the compiled
    // D3D pixel-shader header embedded in the program buffer at +0x14 (immediately after the
    // ProgramBufferData fields); the binder passes &that, or null when no program is bound.
    bool Device::SetPixelProgram(const renderengine::ProgramBufferData* lpPixelProgram)
    {
        if (mpPixelProgramShadow == lpPixelProgram)
        {
            return false;
        }

        void* lpShader = nullptr;
        if (lpPixelProgram != nullptr)
        {
            lpShader = const_cast<u8*>(reinterpret_cast<const u8*>(lpPixelProgram)) + 0x14;
        }
        D3DDevice_SetPixelShader(gpD3DDevice, lpShader);
        mpPixelProgramShadow = lpPixelProgram;
        return true;
    }

    // @0x822769E0: bind a sampler-state object through the shadow.
    void* Device::SetState(void* lpState, u32 luSamplerId)
    {
        CGS_ASSERT(luSamplerId < KU_MAX_TEXTURE_STATES, "luSamplerId < MaxTextureStates");

        void* lpResult = lpState;
        void* const lpPrevious = mapSamplerState[luSamplerId];
        if (lpPrevious != lpState)
        {
            // X360 passes (lpPrevious == 0) as the "no previous state" flag to the sub-setter.
            lpResult = SetSamplerStateLowLevel(lpState, luSamplerId, lpPrevious == nullptr);
            mapSamplerState[luSamplerId] = lpState;
            mauSamplerDirty[luSamplerId] = 0;
        }
        return lpResult;
    }

    // @0x82276C70: bind a texture (resource) through the shadow.
    void* Device::SetResource(void* lpTexture, u32 luSamplerId)
    {
        CGS_ASSERT(luSamplerId < KU_MAX_TEXTURE_STATES, "luSamplerId < MaxTextureStates");

        void* lpResult = lpTexture;
        if (mapSamplerTexture[luSamplerId] != lpTexture)
        {
            // The bind flag is (1<<63) >> (luSamplerId + 32), taken as its low 32 bits: for sampler
            // 0 this is 0x80000000 (the Xenon "set sampler N" mask).
            const u32 luFlags =
                static_cast<u32>((1ull << 63) >> (luSamplerId + 32));
            D3DDevice_SetTexture(gpD3DDevice, luSamplerId, lpTexture, luFlags);
            mapSamplerTexture[luSamplerId] = lpTexture;
            mauSamplerDirty[luSamplerId] = 0;
            lpResult = lpTexture;
        }
        return lpResult;
    }

    // @0x827E7A10: rebind the dirty vertex-program state + the active stream sources to D3D.
    void Device::FlushVertexProgramState()
    {
        if (mbVertexProgramStateDirty)
        {
            renderengine::VertexProgramState* lpState =
                GetVertexProgramState(mpVertexProgramShadow, mpVertexDescriptor);
            CGS_ASSERT(lpState != nullptr, "lVertexProgramState != NULL");

            D3DDevice_SetVertexShader(gpD3DDevice, lpState->GetD3DVertexShader());
            gpCurrentVertexProgramState = lpState;
            D3DDevice_SetVertexDeclaration(gpD3DDevice, mpVertexDescriptor->mpDeclaration);
            mbVertexProgramStateDirty = false;
        }

        // Build a per-stream stride scratch table from the active vertex descriptor: each element
        // maps its descriptor stream-stride into the slot of its (asserted < 8) D3D stream.
        u32 lauStreamStride[8];

        if (mpStreamArray != nullptr)
        {
            for (u32 i = 0; i < 8; ++i)
            {
                lauStreamStride[i] = 0;
            }

            const u32 luElementCount = mpVertexDescriptor->muElementCount;
            for (u32 luElement = 0; luElement < luElementCount; ++luElement)
            {
                const u16 luStream = mpVertexDescriptor->maElements[luElement].muStream;
                CGS_ASSERT(luStream < 8, "lpElement->stream < 8");
                lauStreamStride[luStream] = mpVertexDescriptor->mauStreamStride[luElement];
            }

            const StreamArray* lpStreams = static_cast<const StreamArray*>(mpStreamArray);
            const u32 luStreamCount = lpStreams->muStreamCount;
            u32 luOffsetInBytes = 0;
            for (u32 luStream = 0; luStream < luStreamCount; ++luStream)
            {
                const u32 luStride = lauStreamStride[luStream];
                const void* lpStreamData = reinterpret_cast<const void*>(
                    static_cast<uintptr_t>(lpStreams->mauStreamData[luStream]));
                D3DDevice_SetStreamSource(gpD3DDevice, luStream, lpStreamData, luOffsetInBytes,
                                          luStride, MakeStreamSourceFlag(luStream));
                luOffsetInBytes += luStride;
            }
        }
        else if (mpIndexStreamArray != nullptr)
        {
            for (u32 i = 0; i < 8; ++i)
            {
                lauStreamStride[i] = 0;
            }

            const u32 luElementCount = mpVertexDescriptor->muElementCount;
            for (u32 luElement = 0; luElement < luElementCount; ++luElement)
            {
                const u16 luStream = mpVertexDescriptor->maElements[luElement].muStream;
                CGS_ASSERT(luStream < 8, "lpElement->stream < 8");
                lauStreamStride[luStream] = mpVertexDescriptor->mauStreamStride[luElement];
            }

            const IndexStreamArray* lpStreams =
                static_cast<const IndexStreamArray*>(mpIndexStreamArray);
            const u32 luStreamCount = lpStreams->mauWords[1];
            u32 luOffsetInBytes = 0;
            for (u32 luStream = 0; luStream < luStreamCount; ++luStream)
            {
                const u32 luStride = lauStreamStride[luStream];
                // The data pointers begin at word (mauWords[0] + 2 + luStream).
                const void* lpStreamData = reinterpret_cast<const void*>(
                    static_cast<uintptr_t>(
                        lpStreams->mauWords[lpStreams->mauWords[0] + 2 + luStream]));
                D3DDevice_SetStreamSource(gpD3DDevice, luStream, lpStreamData, luOffsetInBytes,
                                          luStride, MakeStreamSourceFlag(luStream));
                luOffsetInBytes += luStride;
            }
        }
    }

    // @0x823F3250: open a force-stencil-write window.
    void* Device::BeginForceStencilWrite(u32 luStencilValueToWrite)
    {
        void* lpResult = reinterpret_cast<void*>(static_cast<uintptr_t>(luStencilValueToWrite));
        CGS_ASSERT(!mbForceStencilWrite, "!mbForceStencilWrite");
        CGS_ASSERT(luStencilValueToWrite <= 255, "luStencilValueToWrite <= 255");
        muStencilValueToWrite = luStencilValueToWrite;
        mbForceStencilWrite = true;
        muMisc28 = 0;
        return lpResult;
    }

    // @0x823F32E0: close a force-stencil-write window.
    void* Device::EndForceStencilWrite()
    {
        CGS_ASSERT(mbForceStencilWrite, "mbForceStencilWrite");
        mbForceStencilWrite = false;
        muMisc28 = 0;
        return nullptr;
    }

    // @0x823F3190: lock the rasteriser state.
    void* Device::LockRasteriserState()
    {
        CGS_ASSERT(false == mbRasteriserStateLocked, "false == mbRasteriserStateLocked");
        mbRasteriserStateLocked = true;
        return nullptr;
    }

    // @0x823F31F0: unlock the rasteriser state.
    void* Device::UnlockRasteriserState()
    {
        CGS_ASSERT(true == mbRasteriserStateLocked, "true == mbRasteriserStateLocked");
        mbRasteriserStateLocked = false;
        return nullptr;
    }

    // @0x827E7D10: push the low-level blend/colour-write/alpha render-state block through the
    // shadow. When lbWasUnset is true the whole block is force-set; otherwise each render state is
    // bound only when its shadowed value differs.
    void* Device::Xbox2SetStateLowLevelShadowed(void* lpState, bool lbWasUnset)
    {
        const u32* const lpu = static_cast<const u32*>(lpState);
        IDirect3DDevice9* const lpDevice = gpD3DDevice;

        if (lbWasUnset)
        {
            for (u32 i = 0; i < 18; ++i)
            {
                mauLowLevelStateShadow[i] = lpu[i];
            }

            D3DDevice_SetBlendState(lpDevice, 0, lpu[0]);
            D3DDevice_SetBlendState(lpDevice, 1, lpu[1]);
            D3DDevice_SetBlendState(lpDevice, 2, lpu[2]);
            D3DDevice_SetBlendState(lpDevice, 3, lpu[3]);
            D3DDevice_SetRenderState_ColorWriteEnable(lpDevice, lpu[4]);
            D3DDevice_SetRenderState_ColorWriteEnable1(lpDevice, lpu[5]);
            D3DDevice_SetRenderState_ColorWriteEnable2(lpDevice, lpu[6]);
            D3DDevice_SetRenderState_ColorWriteEnable3(lpDevice, lpu[7]);
            D3DDevice_SetRenderState_BlendFactor(lpDevice, lpu[9]);
            D3DDevice_SetRenderState_AlphaToMaskEnable(lpDevice, lpu[10]);
            D3DDevice_SetRenderState_AlphaToMaskOffsets(lpDevice, lpu[8]);
            D3DDevice_SetRenderState_HighPrecisionBlendEnable(lpDevice, lpu[11]);
            D3DDevice_SetRenderState_HighPrecisionBlendEnable1(lpDevice, lpu[12]);
            D3DDevice_SetRenderState_HighPrecisionBlendEnable2(lpDevice, lpu[13]);
            D3DDevice_SetRenderState_HighPrecisionBlendEnable3(lpDevice, lpu[14]);
            D3DDevice_SetRenderState_AlphaTestEnable(lpDevice, lpu[16]);
            D3DDevice_SetRenderState_AlphaRef(lpDevice, lpu[17]);
            D3DDevice_SetRenderState_AlphaFunc(lpDevice, lpu[15]);
        }
        else
        {
            if (mauLowLevelStateShadow[0] != lpu[0])
            {
                mauLowLevelStateShadow[0] = lpu[0];
                D3DDevice_SetBlendState(lpDevice, 0, lpu[0]);
            }
            if (mauLowLevelStateShadow[1] != lpu[1])
            {
                mauLowLevelStateShadow[1] = lpu[1];
                D3DDevice_SetBlendState(lpDevice, 1, lpu[1]);
            }
            if (mauLowLevelStateShadow[2] != lpu[2])
            {
                mauLowLevelStateShadow[2] = lpu[2];
                D3DDevice_SetBlendState(lpDevice, 2, lpu[2]);
            }
            if (mauLowLevelStateShadow[3] != lpu[3])
            {
                mauLowLevelStateShadow[3] = lpu[3];
                D3DDevice_SetBlendState(lpDevice, 3, lpu[3]);
            }
            if (mauLowLevelStateShadow[4] != lpu[4])
            {
                mauLowLevelStateShadow[4] = lpu[4];
                D3DDevice_SetRenderState_ColorWriteEnable(lpDevice, lpu[4]);
            }
            // NOTE: the X360 incremental path compares slot 5 but binds word 6 here (verbatim
            // asm @0x827E7F88, lwz r4,0x18); preserved as-is rather than "corrected".
            if (mauLowLevelStateShadow[5] != lpu[5])
            {
                mauLowLevelStateShadow[5] = lpu[5];
                D3DDevice_SetRenderState_ColorWriteEnable1(lpDevice, lpu[6]);
            }
            if (mauLowLevelStateShadow[6] != lpu[6])
            {
                mauLowLevelStateShadow[6] = lpu[6];
                D3DDevice_SetRenderState_ColorWriteEnable2(lpDevice, lpu[6]);
            }
            if (mauLowLevelStateShadow[7] != lpu[7])
            {
                mauLowLevelStateShadow[7] = lpu[7];
                D3DDevice_SetRenderState_ColorWriteEnable3(lpDevice, lpu[7]);
            }
            if (mauLowLevelStateShadow[8] != lpu[8])
            {
                mauLowLevelStateShadow[8] = lpu[8];
                D3DDevice_SetRenderState_AlphaToMaskOffsets(lpDevice, lpu[8]);
            }
            if (mauLowLevelStateShadow[9] != lpu[9])
            {
                mauLowLevelStateShadow[9] = lpu[9];
                D3DDevice_SetRenderState_BlendFactor(lpDevice, lpu[9]);
            }
            if (mauLowLevelStateShadow[10] != lpu[10])
            {
                mauLowLevelStateShadow[10] = lpu[10];
                D3DDevice_SetRenderState_AlphaToMaskEnable(lpDevice, lpu[10]);
            }
            if (mauLowLevelStateShadow[11] != lpu[11])
            {
                mauLowLevelStateShadow[11] = lpu[11];
                D3DDevice_SetRenderState_HighPrecisionBlendEnable(lpDevice, lpu[11]);
            }
            if (mauLowLevelStateShadow[12] != lpu[12])
            {
                mauLowLevelStateShadow[12] = lpu[12];
                D3DDevice_SetRenderState_HighPrecisionBlendEnable1(lpDevice, lpu[12]);
            }
            if (mauLowLevelStateShadow[13] != lpu[13])
            {
                mauLowLevelStateShadow[13] = lpu[13];
                D3DDevice_SetRenderState_HighPrecisionBlendEnable2(lpDevice, lpu[13]);
            }
            if (mauLowLevelStateShadow[14] != lpu[14])
            {
                mauLowLevelStateShadow[14] = lpu[14];
                D3DDevice_SetRenderState_HighPrecisionBlendEnable3(lpDevice, lpu[14]);
            }
            if (mauLowLevelStateShadow[16] != lpu[16])
            {
                mauLowLevelStateShadow[16] = lpu[16];
                D3DDevice_SetRenderState_AlphaTestEnable(lpDevice, lpu[16]);
            }
            if (mauLowLevelStateShadow[15] != lpu[15])
            {
                mauLowLevelStateShadow[15] = lpu[15];
                D3DDevice_SetRenderState_AlphaFunc(lpDevice, lpu[15]);
            }
            if (mauLowLevelStateShadow[17] != lpu[17])
            {
                mauLowLevelStateShadow[17] = lpu[17];
                D3DDevice_SetRenderState_AlphaRef(lpDevice, lpu[17]);
            }
        }
        return lpState;
    }

    // ============================================================================
    // The mesh-dispatch flush seam (DispatchList::DispatchAllMeshes).
    // ============================================================================

    // X360 walk prologue: dword_8301095C = 0 (vertex program shadow) and
    // dword_83010960 = -1 (pixel program shadow).
    void Device::ResetProgramShadows()
    {
        mpVertexProgramShadow = nullptr;
        mpPixelProgramShadow  = reinterpret_cast<const renderengine::ProgramBufferData*>(
            static_cast<uintptr_t>(~0u));
    }

    // =====================================================================================
    // The technique bind, as the X360 inlines it into DispatchList::DispatchAllMeshes
    // @0x827F2718. It is split in two on PC because the console body is one giant inlined
    // block: SetMeshTechniquePC is the ON-TECHNIQUE-CHANGE half, SetMeshObjectConstantsPC
    // (below) the PER-MESH half. Order and content follow the asm exactly:
    //
    //   technique change:  state triple
    //                      vertex program   -> if it CHANGED: external block B (ST+0x2C)
    //                      internal vertex constants (technique+0x18 list, +0x20 count)
    //                      pixel program    -> if it CHANGED: external block D (ST+0x60)
    //                      internal pixel constants (technique+0x1C list, +0x21 count)
    //                      technique samplers (technique+0x22 count, +0x24 index list)
    //   per mesh:          external block A (ST+0x1C), external block C (ST+0x50)
    //
    // The scratch pointer table the constants come from was gathered at AddToBin time by
    // AddShaderTechniqueConstantsToDispatchBin in the order [A][C][B][D].
    //
    // FLAG PC-platform leaf, three items:
    //   * the Xenos direct constant-memory writes become Set{Vertex,Pixel}ShaderConstantF;
    //   * the render-state TRIPLE is still not bound from data (the MaterialState porter +
    //     the x64 state-object seam are open) -- the renderer's per-pass defaults stand;
    //   * the shadow-compare fast path (skip a constant whose source pointer already sits in
    //     maConstants[slot].maShaderState[stage].mpLatestCopyInPushBuffer) is not modelled:
    //     every listed constant is uploaded. That is the console's "shader changed" branch,
    //     i.e. always correct, only redundant.
    // =====================================================================================
    namespace
    {
        // Quiet one-shot gates (never traps) for the two per-draw conditions the console
        // cannot reach.
        void LogUnsetShaderConstantOnce()
        {
            static bool sbLogged = false;
            if (!sbLogged && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "lpShaderConstantsToApply is NULL. This is probably due to an external"
                       " constant not getting set -- constant skipped [FLAG PC bring-up:"
                       " the world producer does not publish the whole engine constant set]\n";
            }
        }

        void LogFallbackTechniqueOnce()
        {
            static bool sbLogged = false;
            if (!sbLogged && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "SetMeshTechniquePC: no usable technique programs -- the flagged bring-up"
                       " fallback shader drew this technique [FLAG PC bring-up]\n";
            }
        }

        // A streamed ShaderConstantsExternal block: {count, indices*, names*, handles*}.
        // The handle is the measured 4-byte renderengine::ProgramVariableHandle
        // (FORMAT_MAP.md section 3): [0] register index, [1] data type, [2] shader type,
        // [3] register count.
        void DispatchExternalBlock(const u32* lpBlock, void* const* lppSources, bool lbPixel)
        {
            const u32 luCount = lpBlock[0];
            if (luCount == 0 || lppSources == 0)
                return;
            const u8* const lpaHandles =
                reinterpret_cast<const u8*>(static_cast<uintptr_t>(lpBlock[3]));
            if (lpaHandles == 0)
                return;

            for (u32 luConstant = 0; luConstant < luCount; ++luConstant)
            {
                const void* const lpSource = lppSources[luConstant];
                const u8* const   lpHandle = lpaHandles + 4 * luConstant;
                if (lpSource == 0 || lpHandle[3] == 0)
                {
                    // X360 CgsShaderConstants.cpp:394 asserts here ("lpShaderConstantsToApply is
                    // NULL. This is probably due to an external constant not getting set").
                    // On PC the bring-up producer publishes only the constants it can compute,
                    // so an unset engine constant is expected; skipping leaves the register at
                    // whatever the previous draw put there. Quiet one-shot, never a trap.
                    LogUnsetShaderConstantOnce();
                    continue;
                }
                renderengine::WorldShaderConstants_Set(lbPixel, lpHandle[0], lpSource, lpHandle[3]);
            }
        }

        // The material's per-stage ShaderConstantsInternal block {count, sizes*, data*,
        // hashes*, handles*} driven through the TECHNIQUE's binding list, whose 4-byte
        // entries MaterialResourceType::PostFixUpShaderConstants filled in:
        // [0..1] u16 register index, [2] index into the material block, [3] register count.
        void DispatchInternalBlock(const u8* lpBindingList, u32 luCount,
                                   const u32* lpMaterialBlock, bool lbPixel)
        {
            if (luCount == 0 || lpBindingList == 0 || lpMaterialBlock == 0)
                return;
            const u32* const lpaData =
                reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpMaterialBlock[2]));
            if (lpaData == 0)
                return;

            for (u32 luConstant = 0; luConstant < luCount; ++luConstant)
            {
                const u8* const lpEntry = lpBindingList + 4 * luConstant;
                u16 lu16Register;
                std::memcpy(&lu16Register, lpEntry, 2);
                const u32 luBlockIndex   = lpEntry[2];
                const u32 luNumRegisters = lpEntry[3];
                if (luNumRegisters == 0 || luBlockIndex >= lpMaterialBlock[0])
                    continue;

                const void* const lpSource =
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(lpaData[luBlockIndex]));
                if (lpSource == 0)
                {
                    LogUnsetShaderConstantOnce();
                    continue;
                }
                renderengine::WorldShaderConstants_Set(lbPixel, lu16Register, lpSource, luNumRegisters);
            }
        }
    }

    void Device::SetMeshTechniquePC(const CgsGraphics::MaterialTechniqueView* lpTechnique,
                                    const void* lpMaterialAssembly,
                                    void* const* lppConstScratch, bool lbZOnly)
    {
        const u8* const lpTech = reinterpret_cast<const u8*>(lpTechnique);
        const u32 luShaderTechnique = (lpTech != 0) ? *reinterpret_cast<const u32*>(lpTech) : 0u;
        const u8* const lpST =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(luShaderTechnique));

        const renderengine::ProgramBufferData* lpVertexProgram = 0;
        const renderengine::ProgramBufferData* lpPixelProgram  = 0;
        if (lpST != 0)
        {
            // X360 asserts both (CgsDispatcherCommands.cpp:2257 / :2258).
            lpVertexProgram = reinterpret_cast<const renderengine::ProgramBufferData*>(
                static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpST + 0)));   // serialised blob
            lpPixelProgram = reinterpret_cast<const renderengine::ProgramBufferData*>(
                static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpST + 4)));   // serialised blob
        }

        // ---- render states -------------------------------------------------------------
        // FLAG PC bring-up: the X360 binds the technique's render-state TRIPLE (blend /
        // depth-stencil / rasteriser objects at technique+0x04) through the shadowed
        // low-level setters. Those objects are packed Xenos GPU register blocks whose field
        // semantics are not recovered (renderengine::BlendStateParameters' members are still
        // muState4..muState17), so the triple stays unbound and the renderer's per-pass
        // defaults stand -- with ONE exception taken from data that IS attested:
        // MaterialTechniqueResourceType::PostFixUp derives the technique's mu16Flags from the
        // real BlendState (bit 0 = alpha blend, bit 3 = alpha test, bit 4 = hw instancing), so
        // alpha TEST can be driven per technique. Without it every 1-bit-alpha technique
        // (foliage, fences, cruciform billboards) draws its cut-out texels opaque black.
        // DELETE when the state triple is unpacked.
        if (lpTech != 0)
        {
            u16 lu16Flags;
            std::memcpy(&lu16Flags, lpTech + 0x08, 2);
            renderengine::WorldShader_SetAlphaTest((lu16Flags & 8u) != 0);
        }

        // ---- programs ------------------------------------------------------------------
        // The program payload is ProgramBufferData + 0x14 (VertexProgramState::
        // GetD3DVertexShader / SetPixelProgram use the same expression).
        bool lbRealBound = false;
        if (lpVertexProgram != 0 && lpPixelProgram != 0)
        {
            lbRealBound = renderengine::WorldPrograms_Bind(
                reinterpret_cast<const u8*>(lpVertexProgram) + 0x14,
                reinterpret_cast<const u8*>(lpPixelProgram) + 0x14);
        }
        const bool lbVertexProgramChanged = SetVertexProgram(lpVertexProgram);
        const bool lbPixelProgramChanged  = SetPixelProgram(lpPixelProgram);

        if (!lbRealBound)
        {
            // FLAG PC bring-up: no usable technique programs (SHADERS.BNDL absent, an
            // unresolved import, or a payload that is not D3D9 bytecode) -> the documented
            // last-resort fallback shader. DELETE-when: every technique resolves.
            renderengine::WorldShader_ClearRealPrograms();
            renderengine::WorldFallbackShader_Bind();
            renderengine::WorldMaterialSamplers_Bind(lpMaterialAssembly);
            LogFallbackTechniqueOnce();
            return;
        }

        // ---- the technique's GLOBAL constant blocks + the material's internal ones ------
        const u32* const lpBlockA = reinterpret_cast<const u32*>(lpST + 0x1C);
        const u32* const lpBlockB = reinterpret_cast<const u32*>(lpST + 0x2C);
        const u32* const lpBlockC = reinterpret_cast<const u32*>(lpST + 0x50);
        const u32* const lpBlockD = reinterpret_cast<const u32*>(lpST + 0x60);

        // Scratch order [A][C][B][D]; the pixel halves are absent on the z-only walk.
        void* const* lppB = lppConstScratch + lpBlockA[0] + (lbZOnly ? 0u : lpBlockC[0]);
        void* const* lppD = lppB + lpBlockB[0];

        if (lbVertexProgramChanged && lppConstScratch != 0)
        {
            DispatchExternalBlock(lpBlockB, lppB, false);
        }

        const u8* const lpAssembly = static_cast<const u8*>(lpMaterialAssembly);
        if (lpAssembly != 0)
        {
            const u8* const lpVertexList = reinterpret_cast<const u8*>(
                static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpTech + 0x18)));      // serialised blob
            const u32* const lpVertexBlock = reinterpret_cast<const u32*>(
                static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpAssembly + 0x10)));  // serialised blob
            DispatchInternalBlock(lpVertexList, lpTech[0x20], lpVertexBlock, false);
        }

        if (!lbZOnly)
        {
            if (lbPixelProgramChanged && lppConstScratch != 0)
            {
                DispatchExternalBlock(lpBlockD, lppD, true);
            }
            if (lpAssembly != 0)
            {
                const u8* const lpPixelList = reinterpret_cast<const u8*>(
                    static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpTech + 0x1C)));      // serialised blob
                const u32* const lpPixelBlock = reinterpret_cast<const u32*>(
                    static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpAssembly + 0x14)));  // serialised blob
                DispatchInternalBlock(lpPixelList, lpTech[0x21], lpPixelBlock, true);
            }
        }

        // ---- samplers ------------------------------------------------------------------
        // X360: `for i < technique[0x22]: sampler = &materialSamplers[20 * indexList[i]]`,
        // where the index list (technique+0x24) was built by MaterialResourceType::PostFixUp
        // by matching each material sampler's id against the SHADER TECHNIQUE's own sampler
        // table. Only the samplers this technique actually names are bound -- unlike the
        // bring-up path, which binds every material-scope sampler because the fallback
        // pixel shader only has s0.
        BindTechniqueSamplers(lpTech, lpAssembly);
    }

    // [PC leaf] The PER-MESH half of the same inlined X360 block: the technique's two
    // OBJECT-scope external constant blocks (the world matrix and whatever else is per
    // draw). Their sources are the head of the scratch table, [A] then [C].
    void Device::SetMeshObjectConstantsPC(const CgsGraphics::MaterialTechniqueView* lpTechnique,
                                          void* const* lppConstScratch, bool lbZOnly)
    {
        if (!renderengine::WorldShader_RealProgramsBound())
            return;
        const u8* const lpTech = reinterpret_cast<const u8*>(lpTechnique);
        const u32 luShaderTechnique = (lpTech != 0) ? *reinterpret_cast<const u32*>(lpTech) : 0u;
        if (luShaderTechnique == 0 || lppConstScratch == 0)
            return;
        const u8* const lpST =
            reinterpret_cast<const u8*>(static_cast<uintptr_t>(luShaderTechnique));

        const u32* const lpBlockA = reinterpret_cast<const u32*>(lpST + 0x1C);
        const u32* const lpBlockC = reinterpret_cast<const u32*>(lpST + 0x50);

        DispatchExternalBlock(lpBlockA, lppConstScratch, false);
        if (!lbZOnly)
        {
            DispatchExternalBlock(lpBlockC, lppConstScratch + lpBlockA[0], true);
        }
    }

    // The technique's sampler selection (see SetMeshTechniquePC's tail).
    void Device::BindTechniqueSamplers(const u8* lpTech, const u8* lpAssembly)
    {
        if (lpTech == 0 || lpAssembly == 0)
            return;
        const u32 luNumBindings = lpTech[0x22];
        const u8* const lpIndexList = reinterpret_cast<const u8*>(
            static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpTech + 0x24)));      // serialised blob
        const s32 liNumSamplers =
            static_cast<s32>(*reinterpret_cast<const s8*>(lpAssembly + 0x09));          // serialised blob
        const u8* const lpSamplers = reinterpret_cast<const u8*>(
            static_cast<uintptr_t>(*reinterpret_cast<const u32*>(lpAssembly + 0x0C)));  // serialised blob
        if (luNumBindings == 0 || lpIndexList == 0 || liNumSamplers <= 0 || lpSamplers == 0)
            return;

        // [DIAG] one-shot tally over the first 4096 sampler bindings: how many resolve all the
        // way to a live D3D texture, and where the rest stop.
        static u32 suSeen = 0u, suBound = 0u, suNoState = 0u, suNoRaster = 0u, suNoTexture = 0u;

        for (u32 luBinding = 0; luBinding < luNumBindings; ++luBinding)
        {
            const u32 luSamplerIndex = lpIndexList[luBinding];
            if (luSamplerIndex >= static_cast<u32>(liNumSamplers))
                continue;
            const u8* const lpSampler = lpSamplers + 20 * luSamplerIndex;

            u16 lu16Unit;
            u32 luStateSlot;
            std::memcpy(&lu16Unit,    lpSampler + 0x08, 2);
            std::memcpy(&luStateSlot, lpSampler + 0x10, 4);

            const bool lbTally = (suSeen < 4096u);
            if (lbTally) ++suSeen;

            if (luStateSlot == 0)
            {
                if (lbTally) ++suNoState;
                continue;
            }
            // TextureState +0x20 -> renderengine::Texture* (both console u32 slots).
            const u8* const lpState = reinterpret_cast<const u8*>(static_cast<uintptr_t>(luStateSlot));
            u32 luRasterSlot;
            std::memcpy(&luRasterSlot, lpState + 0x20, 4);
            if (luRasterSlot == 0 || luRasterSlot == 0xFFFFFFFFu)
            {
                if (lbTally) ++suNoRaster;
                continue;
            }

            if (renderengine::WorldShader_BindTextureUnit(
                    lu16Unit, reinterpret_cast<const void*>(static_cast<uintptr_t>(luRasterSlot))))
            {
                if (lbTally) ++suBound;
            }
            else if (lbTally)
            {
                ++suNoTexture;
            }
        }

        if (suSeen >= 4096u && suSeen != 0xFFFFFFFFu)
        {
            const u32 luSeen = suSeen;
            suSeen = 0xFFFFFFFFu;   // report once
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[WorldSamplers] technique sampler tally: bound=" << suBound
                    << " no-TextureState=" << suNoState
                    << " TextureState-with-null-raster=" << suNoRaster
                    << " raster-with-no-D3D-texture=" << suNoTexture
                    << " of " << luSeen << "\n";
            }
        }
    }

    // [PC bring-up shim] Per-object WVP for the fallback shader.
    void Device::SetObjectTransformPC(const f32* lpWvpRows16)
    {
        renderengine::WorldFallbackShader_SetWvp(lpWvpRows16);
    }

    // [PC leaf] Bind the mesh geometry: index buffer, stream-0 vertex buffer and
    // the technique's vertex declaration. The mesh buffer table (renderablemesh.h
    // maBuffers, widened x64 image) holds [0] = IndexBufferHeader*,
    // [1..numVB] = VertexBufferHeader*s, then the per-technique serialised
    // vertex-descriptor pointers. The Xenon stream-shadow dance collapses into
    // direct binds through the D3DDevice_* seam (single-stream world data --
    // the converter hard-fails multi-stream descriptors upstream).
    void Device::SetMeshBuffersPC(const RenderableMesh* lpMesh, u32 luTechniqueIndex)
    {
        const void* const* lppBuffers = lpMesh->maBuffers;
        const u32 luNumVb = lpMesh->mu8NumVertexBuffers;

        // The technique's vertex descriptor (32-bit serialised image).
        const void* lpVdImage = lppBuffers[1u + luNumVb + luTechniqueIndex];
        u32 luStride = 0;
        void* lpDeclaration = renderengine::WorldVd32_GetDeclaration(lpVdImage, &luStride);
        D3DDevice_SetVertexDeclaration(gpD3DDevice, lpDeclaration);

        renderengine::WorldDraw_SetIndexSource(lppBuffers[0]);
        renderengine::WorldDraw_SetVertexSource(luNumVb != 0 ? lppBuffers[1] : 0, luStride);

        // [PC bring-up shim] The fallback pair can only be chosen once this mesh's
        // declaration is known (a vs_3_0 input the declaration does not supply makes the
        // draw fail on D3D9), so the technique bind above leaves the choice to here.
        renderengine::WorldFallbackShader_SelectForMesh();
    }

    // [PC leaf] Issue the indexed draw from the mesh's DrawIndexedParameters
    // (X360: FlushVertexProgramState + D3DDevice_DrawIndexedVertices).
    void Device::DrawIndexedMeshPC(const RenderableMesh* lpMesh)
    {
        renderengine::WorldDraw_IndexedUP(lpMesh->mDrawIndexedParameters.mePrimitiveType,
                                          lpMesh->mDrawIndexedParameters.muBaseVertexIndex,
                                          lpMesh->mDrawIndexedParameters.muMinVertexIndex,
                                          lpMesh->mDrawIndexedParameters.muNumVertices);
    }
}
