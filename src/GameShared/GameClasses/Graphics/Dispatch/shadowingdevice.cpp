#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"

#include <cstddef>
#include <cstdint>

#include "GameShared/GameClasses/Core/CgsAssert.h"
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
}
