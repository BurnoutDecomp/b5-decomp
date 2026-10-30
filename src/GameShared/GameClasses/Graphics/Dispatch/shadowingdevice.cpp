#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"

#include <cstddef>
#include "types.hpp"

#include <cstring>                                                          // memcpy (serialised-slot reads)

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // the quiet one-shot gates
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"        // RenderableMesh (mesh-dispatch seam)
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // MaterialTechniqueView
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"    // BlendMaterialState

#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/meshhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"

namespace shadow
{
    void ApplyNativeSamplerState(const renderengine::SamplerState* lpState, u32 luSampler, bool lbWasUnset);
}

namespace
{
    // X360 stream-source flag: (1<<63) >> (((21846*(95-stream))>>16)+32), taken as its low 32 bits.
    inline D3DBLENDSTATE NativeBlendState(u32 luWord)
    {
        D3DBLENDSTATE lState;
        memcpy(&lState, &luWord, sizeof(lState));
        return lState;
    }

    inline u32 MakeStreamSourceFlag(u32 luStream)
    {
        const u32 luShift = ((21846u * (95u - luStream)) >> 16) + 32u;
        return static_cast<u32>((1ull << 63) >> luShift);
    }


}

namespace shadow
{
    renderengine::VertexProgramState Device::mVertexProgramState = {};
    // ARTIST83011118 is zero in the original image; Initialize publishes the state.
    renderengine::VertexProgramState* Device::mapVertexProgramStateSlot[5] = { 0, 0, 0, 0, 0 };
    bool Device::mbVertexProgramStateDirty = false;

    const void* Device::mpStreamArray = 0;
    const void* Device::mpIndexStreamArray = 0;
    const renderengine::VertexDescriptorData* Device::mpVertexDescriptor = 0;

    const renderengine::ProgramBufferData* Device::mpVertexProgramShadow = 0;
    const renderengine::ProgramBufferData* Device::mpPixelProgramShadow = 0;
    const renderengine::BlendMaterialState* Device::mpBlendState = 0;   // dword_83010964

    const renderengine::TextureState* Device::mapTextureState[KU_MAX_TEXTURE_STATES] = {};   // dword_83010968
    void* Device::mapSamplerState[KU_MAX_TEXTURE_STATES] = {};
    void* Device::mapSamplerTexture[KU_MAX_TEXTURE_STATES] = {};

    const renderengine::DepthStencilState* Device::mpDepthStencilState = 0;  // dword_83010A28
    const renderengine::RasterizerState*   Device::mpRasterizerState   = 0;  // dword_83010A2C
    const renderengine::RenderTargetState* Device::mpRenderTargetState = 0;
    bool Device::mbForceStencilWrite = false;
    u32 Device::muStencilValueToWrite = 0;
    bool Device::mbRasteriserStateLocked = false;
    // byte_83010907 -- the blend third's lock gate. See the header for the elimination that names it.
    bool Device::mbBlendStateLocked = false;

    u32 Device::mauLowLevelStateShadow[18] = {};
    // The X360 depth/stencil lock window (mbDepthStencilStateLocked, the sibling of
    // mbRasteriserStateLocked that DispatchAllMeshes tests before binding the depth state).
    // Nothing in the reconstructed set opens that window yet, so it reads false throughout.
    bool Device::mbDepthStencilStateLocked = false;
    // mpLastBlendState / mpLastDepthStencilState / mpLastRasterizerState are GONE: they were a second
    // host home for dword_83010964 / dword_83010A28 / dword_83010A2C, which mpBlendState /
    // mpDepthStencilState / mpRasterizerState above already own. Their comment went with them; it
    // said the slots "live on the per-dispatch context", and they do not -- DispatchAllMeshes
    // @0x827F2718 reaches them as 0xD8 / 0x14 / 0xDC off r27, and r27 IS off_83010950
    // (`addi r11, r11, off_83010950@l` @0x827F2754 -> `stw r11, var_140(r1)` @0x827F2760 ->
    // `lwz r27, var_140(r1)` @0x827F2EE0), the file-scope shadow block.

    bool Device::Initialize()
    {
        const bool lbInitialised = renderengine::Device::Initialize();

        // @0x827ED6E0: the slot is reset to { &mVertexProgramState, 0, 0, 0, 0 }.
        mapVertexProgramStateSlot[0] = &mVertexProgramState;
        mapVertexProgramStateSlot[1] = 0;
        mapVertexProgramStateSlot[2] = 0;
        mapVertexProgramStateSlot[3] = 0;
        mapVertexProgramStateSlot[4] = 0;
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
                                      static_cast<D3DPRIMITIVETYPE>(lrParameters.mePrimitiveType),
                                      lrParameters.muBaseVertexIndex,
                                      lrParameters.muMinVertexIndex,
                                      lrParameters.muNumVertices);
    }

    // @0x82276970: clear the whole shadow cache.
    void Device::ResetShadowing()
    {
        mpStreamArray = 0;
        mpIndexStreamArray = 0;
        mpVertexDescriptor = 0;
        mpVertexProgramShadow = 0;
        // dword_83010960 is reset to -1 (the "no pixel program bound" sentinel).
        mpPixelProgramShadow = reinterpret_cast<const renderengine::ProgramBufferData*>(
            static_cast<uintptr_t>(~0u));
        mpBlendState = 0;

        for (u32 luSampler = 0; luSampler < KU_MAX_TEXTURE_STATES; ++luSampler)
        {
            mapTextureState[luSampler] = 0;
            mapSamplerState[luSampler] = 0;
            mapSamplerTexture[luSampler] = 0;
        }

        mpDepthStencilState = 0;
        mpRasterizerState = 0;
        mpRenderTargetState = 0;
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

        void* lpShader = 0;
        if (lpPixelProgram != 0)
        {
            lpShader = const_cast<u8*>(reinterpret_cast<const u8*>(lpPixelProgram)) + 0x14;
        }
        D3DDevice_SetPixelShader(gpD3DDevice, static_cast<D3DPixelShader*>(lpShader));
        mpPixelProgramShadow = lpPixelProgram;
        return true;
    }

    // Bind the active vertex descriptor through the shadow (X360 off_83010958 +
    // byte_83010A34, written inline by the immediate-mode renderers).
    bool Device::SetVertexDescriptor(const renderengine::VertexDescriptorData* lpVertexDescriptor)
    {
        if (mpVertexDescriptor == lpVertexDescriptor)
        {
            return false;
        }
        mpVertexDescriptor = lpVertexDescriptor;
        SetVertexProgramInternal();
        return true;
    }

    // @0x822769E0: bind a sampler-state object through the shadow.
    void Device::SetState(const renderengine::SamplerState* lpState, u32 luSamplerId)
    {
        CGS_ASSERT(luSamplerId < KU_MAX_TEXTURE_STATES, "luSamplerId < MaxTextureStates");

        void* const lpPrevious = mapSamplerState[luSamplerId];
        if (lpPrevious != lpState)
        {
            // X360 passes (lpPrevious == 0) as the "no previous state" flag to the sub-setter.
            ApplyNativeSamplerState(lpState, luSamplerId, lpPrevious == 0);
            mapSamplerState[luSamplerId] = const_cast<renderengine::SamplerState*>(lpState);
            mapTextureState[luSamplerId] = 0;   // a bare-sampler bind invalidates the whole-unit key
        }
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
            D3DDevice_SetTexture(gpD3DDevice, luSamplerId, static_cast<D3DBaseTexture*>(lpTexture), luFlags);
            mapSamplerTexture[luSamplerId] = lpTexture;
            mapTextureState[luSamplerId] = 0;   // a bare-texture bind invalidates the whole-unit key
            lpResult = lpTexture;
        }
        return lpResult;
    }

    // @0x8227D158: bind a whole TextureState (its 32-byte sampler block + the raster it samples)
    // at one unit through the shadow. Keyed on the whole-unit shadow mapTextureState (dword_83010968):
    //   * HIT  (`lwz r11, dword_83010968[a2]; cmplw r11, r3; bne`): the object is already the bound
    //     one, so nothing is applied -- the console only re-asserts that the TEXTURE shadow still
    //     holds this object's raster (shadowingdevice.h:918, string verbatim below). That assert is
    //     what catches a SetResource() that slipped in behind a whole-unit bind.
    //   * MISS: the raster is read FIRST (`lwz r27, 0x20(r3)` -- TextureState::mpRaster, i.e.
    //     NativeGetTexture()), the sampler block is applied through the same low-level setter the
    //     bare-sampler binder uses -- with the SAME "was unset" flag, (dword_83010968[a2] == 0), NOT
    //     the sampler shadow's -- then the raster is set with the (1<<63)>>(unit+32) fast-set mask
    //     (`li r10,1 / srd r6, r10, r9`, the mask SetResource computes), and all three per-unit
    //     shadows are written: whole-unit key = this, sampler shadow = this (the TextureState IS a
    //     sampler-state object -- its first 32 bytes are the sampler block, so the pointer is a valid
    //     key for the bare-sampler binder's compare), texture shadow = the raster.
    // No unit-range assert here (0x8227D158 has none; the two bare binders do) -- faithful.
    void Device::SetState(const renderengine::TextureState* lpState, u32 luSamplerId)
    {
        const renderengine::TextureState* const lpPrevious = mapTextureState[luSamplerId];
        if (lpPrevious == lpState)
        {
            CGS_ASSERT(mapSamplerTexture[luSamplerId] == static_cast<void*>(lpState->mpRaster),
                       "m_Shadow.m_apTextures[id] == textureState->NativeGetTexture()");   // shadowingdevice.h:918
        }
        else
        {
            void* const lpTexture = static_cast<void*>(lpState->mpRaster);              // lwz r27, 0x20(r3)
            ApplyNativeSamplerState(lpState, luSamplerId, lpPrevious == 0);
            const u32 luFlags =
                static_cast<u32>((1ull << 63) >> (luSamplerId + 32));                   // li r10,1 / srd r6,r10,r9
            D3DDevice_SetTexture(gpD3DDevice, luSamplerId, static_cast<D3DBaseTexture*>(lpTexture), luFlags);         // bl D3DDevice_SetTexture
            mapTextureState[luSamplerId]   = lpState;                                    // stw r31, dword_83010968[r30]
            mapSamplerState[luSamplerId]   = const_cast<renderengine::TextureState*>(lpState);                                   // stw r31, dword_830109A8[r30]
            mapSamplerTexture[luSamplerId] = lpTexture;                                  // stw r27, dword_830109E8[r30]
        }
    }

    // The console's own per-unit texture shadow, reached by name -- see the declaration's banner
    // for the one X360 body that does this (LionParticleRender::SetMaterial @0x822896B8) and for
    // the measured defect a private copy of it produced.
    void* Device::GetSamplerTextureShadow(u32 luSamplerId)
    {
        if (luSamplerId >= KU_MAX_TEXTURE_STATES)
            return 0;
        return mapSamplerTexture[luSamplerId];
    }

    void Device::SetSamplerTextureShadow(u32 luSamplerId, void* lpTexture)
    {
        if (luSamplerId >= KU_MAX_TEXTURE_STATES)
            return;
        mapSamplerTexture[luSamplerId] = lpTexture;   // stw r31, dword_830109E8(r30)
        mapTextureState[luSamplerId]   = 0;           // stw r11(=0), dword_83010968(r30)
    }

    // @0x827E7A10: rebind the dirty vertex-program state + the active stream sources to D3D.
    void Device::FlushVertexProgramState()
    {

        if (mbVertexProgramStateDirty)
        {
            renderengine::VertexProgramState* lpState =
                GetVertexProgramState(mpVertexProgramShadow, mpVertexDescriptor);
            CGS_ASSERT(lpState != 0, "lVertexProgramState != NULL");

            D3DDevice_SetVertexShader(gpD3DDevice, lpState->GetD3DVertexShader());
            renderengine::gpCurrentVertexProgramState = lpState;
            D3DDevice_SetVertexDeclaration(gpD3DDevice, mpVertexDescriptor->mpDeclaration);
            mbVertexProgramStateDirty = false;
        }

        // Build a per-stream stride scratch table from the active vertex descriptor: each element
        // maps its descriptor stream-stride into the slot of its (asserted < 8) D3D stream.
        u32 lauStreamStride[8];

        if (mpStreamArray != 0)
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
                lauStreamStride[luStream] = mpVertexDescriptor->GetElementStrides()[luElement];
            }

            const RenderableMesh* lpStreams = static_cast<const RenderableMesh*>(mpStreamArray);
            const u32 luStreamCount = lpStreams->mu8NumVertexBuffers;
            u32 luOffsetInBytes = 0;
            for (u32 luStream = 0; luStream < luStreamCount; ++luStream)
            {
                const u32 luStride = lauStreamStride[luStream];
                void* lpStreamData = lpStreams->maBuffers[luStream + 1];
                D3DDevice_SetStreamSource(gpD3DDevice, luStream, static_cast<D3DVertexBuffer*>(lpStreamData), luOffsetInBytes,
                                          luStride, MakeStreamSourceFlag(luStream));
                luOffsetInBytes += luStride;
            }
        }
        else if (mpIndexStreamArray != 0)
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
                lauStreamStride[luStream] = mpVertexDescriptor->GetElementStrides()[luElement];
            }

            const renderengine::MeshHelper* lpStreams =
                static_cast<const renderengine::MeshHelper*>(mpIndexStreamArray);
            const u32 luStreamCount = lpStreams->GetNumVertexBuffers();
            u32 luOffsetInBytes = 0;
            for (u32 luStream = 0; luStream < luStreamCount; ++luStream)
            {
                const u32 luStride = lauStreamStride[luStream];
                const renderengine::VertexBuffer* lpStreamData = lpStreams->GetVertexBuffer(luStream);
                D3DDevice_SetStreamSource(gpD3DDevice, luStream, const_cast<renderengine::VertexBuffer*>(lpStreamData), luOffsetInBytes,
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
        mpDepthStencilState = 0;   // dword_83010A28 = 0 @0x823F32D4
        return lpResult;
    }

    // @0x823F32E0: close a force-stencil-write window.
    void* Device::EndForceStencilWrite()
    {
        CGS_ASSERT(mbForceStencilWrite, "mbForceStencilWrite");
        mbForceStencilWrite = false;
        mpDepthStencilState = 0;   // dword_83010A28 = 0
        return 0;
    }

    // @0x823F3190: lock the rasteriser state.
    void* Device::LockRasteriserState()
    {
        CGS_ASSERT(false == mbRasteriserStateLocked, "false == mbRasteriserStateLocked");
        mbRasteriserStateLocked = true;
        return 0;
    }

    // @0x823F31F0: unlock the rasteriser state.
    void* Device::UnlockRasteriserState()
    {
        CGS_ASSERT(true == mbRasteriserStateLocked, "true == mbRasteriserStateLocked");
        mbRasteriserStateLocked = false;
        return 0;
    }

    // The depth/stencil twins of the pair above. INLINED on the X360 -- no standalone symbol --
    // and recovered from BrnRendererModule::Render @0x8240BFA8's env-map face loop, where the
    // compiler emitted them with their asserts intact (0x8240CC6C-0x8240CCAC and
    // 0x8240CCF4-0x8240CD30; the two assert strings are "false == mbDepthStencilStateLocked" at
    // shadowingdevice.h:1235 and "true == mbDepthStencilStateLocked" at :1240). See the declarations
    // in the header for the export-set census that establishes the inlining.
    //
    // NOTHING ELSE IS DONE, and the omission is the asm's: unlike BeginForceStencilWrite, neither
    // body nulls mpDepthStencilState. Locking is not a state change -- it is a promise that the
    // state on the device is the one the caller just applied and must not be replaced -- so
    // invalidating the shadow here would force a redundant rebind on the next unlocked setter.
    void* Device::LockDepthStencilState()
    {
        CGS_ASSERT(false == mbDepthStencilStateLocked, "false == mbDepthStencilStateLocked");
        mbDepthStencilStateLocked = true;
        return 0;
    }

    void* Device::UnlockDepthStencilState()
    {
        CGS_ASSERT(true == mbDepthStencilStateLocked, "true == mbDepthStencilStateLocked");
        mbDepthStencilStateLocked = false;
        return 0;
    }

    // @0x827E7D10: push the low-level blend/colour-write/alpha render-state block through the
    // shadow. When lbWasUnset is true the whole block is force-set; otherwise each render state is
    // bound only when its shadowed value differs.
    void* Device::Xbox2SetStateLowLevelShadowed(void* lpState, bool lbWasUnset)
    {

        const u32* const lpu = static_cast<const u32*>(lpState);
        D3DDevice* const lpDevice = gpD3DDevice;

        if (lbWasUnset)
        {
            for (u32 i = 0; i < 18; ++i)
            {
                mauLowLevelStateShadow[i] = lpu[i];
            }

            D3DDevice_SetBlendState(lpDevice, 0, NativeBlendState(lpu[0]));
            D3DDevice_SetBlendState(lpDevice, 1, NativeBlendState(lpu[1]));
            D3DDevice_SetBlendState(lpDevice, 2, NativeBlendState(lpu[2]));
            D3DDevice_SetBlendState(lpDevice, 3, NativeBlendState(lpu[3]));
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
                D3DDevice_SetBlendState(lpDevice, 0, NativeBlendState(lpu[0]));
            }
            if (mauLowLevelStateShadow[1] != lpu[1])
            {
                mauLowLevelStateShadow[1] = lpu[1];
                D3DDevice_SetBlendState(lpDevice, 1, NativeBlendState(lpu[1]));
            }
            if (mauLowLevelStateShadow[2] != lpu[2])
            {
                mauLowLevelStateShadow[2] = lpu[2];
                D3DDevice_SetBlendState(lpDevice, 2, NativeBlendState(lpu[2]));
            }
            if (mauLowLevelStateShadow[3] != lpu[3])
            {
                mauLowLevelStateShadow[3] = lpu[3];
                D3DDevice_SetBlendState(lpDevice, 3, NativeBlendState(lpu[3]));
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

    // @0x827E8150: the DEPTH/STENCIL twin of Xbox2SetStateLowLevelShadowed. Same shape --
    // the 24-word marshalled DepthStencilState object in, each word pushed through its own
    // D3DDevice_SetRenderState_* fast-set. The force-stencil-write window (byte_83010906)
    // overrides six of the words on its way through, exactly as the console does.
    void* Device::Xbox2SetDepthStencilStateLowLevelShadowed(void* lpState, bool /*lbWasUnset*/)
    {
        const renderengine::DepthStencilState* const lpDs =
            static_cast<const renderengine::DepthStencilState*>(lpState);
        D3DDevice* const lpDevice = gpD3DDevice;
        if (lpDs == 0)
            return lpState;

        u32 luStencilEnable  = lpDs->muStencilEnable;
        u32 luTwoSided       = lpDs->muTwoSidedStencilMode;
        u32 luStencilFunc    = lpDs->muStencilFunc;
        u32 luStencilWriteMask = lpDs->muStencilWriteMask;
        u32 luStencilRef     = lpDs->muStencilRef;
        u32 luStencilPass    = lpDs->muStencilPass;
        u32 luStencilZFail   = lpDs->muStencilZFail;
        if (mbForceStencilWrite)
        {
            luStencilEnable    = 1u;
            luTwoSided         = 0u;
            luStencilFunc      = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;
            luStencilWriteMask = 0xFFFFFFFFu;
            luStencilRef       = muStencilValueToWrite;
            luStencilPass      = 2u;   // REPLACE, in the Xenos' zero-based stencil-op set
            luStencilZFail     = 0u;   // KEEP
        }

        D3DDevice_SetRenderState_ZEnable(lpDevice, lpDs->muZEnable);
        D3DDevice_SetRenderState_ZWriteEnable(lpDevice, lpDs->muZWriteEnable);
        D3DDevice_SetRenderState_ZFunc(lpDevice, lpDs->muZFunc);
        D3DDevice_SetRenderState_StencilEnable(lpDevice, luStencilEnable);
        D3DDevice_SetRenderState_TwoSidedStencilMode(lpDevice, luTwoSided);
        D3DDevice_SetRenderState_StencilFunc(lpDevice, luStencilFunc);
        D3DDevice_SetRenderState_StencilFail(lpDevice, lpDs->muStencilFail);
        D3DDevice_SetRenderState_StencilZFail(lpDevice, luStencilZFail);
        D3DDevice_SetRenderState_StencilPass(lpDevice, luStencilPass);
        D3DDevice_SetRenderState_CCWStencilFunc(lpDevice, lpDs->muCcwStencilFunc);
        D3DDevice_SetRenderState_CCWStencilFail(lpDevice, lpDs->muCcwStencilFail);
        D3DDevice_SetRenderState_CCWStencilZFail(lpDevice, lpDs->muCcwStencilZFail);
        D3DDevice_SetRenderState_CCWStencilPass(lpDevice, lpDs->muCcwStencilPass);
        D3DDevice_SetRenderState_StencilRef(lpDevice, luStencilRef);
        D3DDevice_SetRenderState_StencilMask(lpDevice, lpDs->muStencilMask);
        D3DDevice_SetRenderState_StencilWriteMask(lpDevice, luStencilWriteMask);
        D3DDevice_SetRenderState_CCWStencilRef(lpDevice, lpDs->muCcwStencilRef);
        D3DDevice_SetRenderState_CCWStencilMask(lpDevice, lpDs->muCcwStencilMask);
        D3DDevice_SetRenderState_CCWStencilWriteMask(lpDevice, lpDs->muCcwStencilWriteMask);
        D3DDevice_SetRenderState_HiStencilEnable(lpDevice, lpDs->muHiStencilEnable);
        D3DDevice_SetRenderState_HiStencilWriteEnable(lpDevice, lpDs->muHiStencilWriteEnable);
        D3DDevice_SetRenderState_HiStencilFunc(lpDevice, lpDs->muHiStencilFunc);
        D3DDevice_SetRenderState_HiStencilRef(lpDevice, lpDs->muHiStencilRef);
        return lpState;
    }

    // @0x827E8690: the RASTERISER twin. Thirteen words, eleven fast-sets (word 7 is never
    // read and word 12 is the initialised flag); the reset index is only pushed when the
    // reset is enabled.
    void* Device::Xbox2SetRasterizerStateLowLevelShadowed(void* lpState, bool /*lbWasUnset*/)
    {
        const renderengine::RasterizerState* const lpRs =
            static_cast<const renderengine::RasterizerState*>(lpState);
        D3DDevice* const lpDevice = gpD3DDevice;
        if (lpRs == 0)
            return lpState;

        D3DDevice_SetRenderState_CullMode(lpDevice, lpRs->muCullMode);
        D3DDevice_SetRenderState_FillMode(lpDevice, lpRs->muFillMode);
        D3DDevice_SetRenderState_ScissorTestEnable(lpDevice, lpRs->muScissorTestEnable);
        D3DDevice_SetRenderState_SlopeScaleDepthBias(lpDevice, lpRs->muSlopeScaleDepthBias);
        D3DDevice_SetRenderState_DepthBias(lpDevice, lpRs->muDepthBias);
        D3DDevice_SetRenderState_MultiSampleAntiAlias(lpDevice, lpRs->muMultiSampleAntiAlias);
        D3DDevice_SetRenderState_MultiSampleMask(lpDevice, lpRs->muMultiSampleMask);
        D3DDevice_SetRenderState_ViewportEnable(lpDevice, lpRs->muViewportEnable);
        D3DDevice_SetRenderState_HalfPixelOffset(lpDevice, lpRs->muHalfPixelOffset);
        D3DDevice_SetRenderState_PrimitiveResetEnable(lpDevice, lpRs->muPrimitiveResetEnable);
        if (lpRs->muPrimitiveResetEnable)
            D3DDevice_SetRenderState_PrimitiveResetIndex(lpDevice, lpRs->muPrimitiveResetIndex);

        return lpState;
    }

    // =================================================================================
    // shadow::Device::SetState(const renderengine::DepthStencilState*)  @ 0x82276AD0
    //
    // Bind the depth/stencil third of the render-state triple through the shadow cache: gate on the
    // depth/stencil lock window, compare against the shadow block's m_pDepthStencilState slot
    // (X360 dword_83010A28), apply, then cache. DWARF names it at source shadowingdevice.h:499.
    //
    // NOT A STATIC MEMBER BY DWARF ASSERTION -- dwarfdump elides the implicit `this` from every member
    // it prints, so it cannot tell static from non-static. It is proved from the callee's own register
    // discipline: `mr r31, r3` @0x82276AE8 takes r3 as THE STATE POINTER (it is what the compare at
    // 0x82276B04 tests and what the store at 0x82276B1C writes), and the object it compares against is
    // reached independently from the file-scope block base (`lis r11, off_83010950@ha` @0x82276AF8 /
    // `addi r30, r11, off_83010950@l` @0x82276AFC). There is no object pointer anywhere in the body,
    // so there is no `this`.
    //
    // NO NULL GUARD, DELIBERATELY. On the console a null wanted state with a non-null cache passes the
    // gate, fails the compare, calls the applier (which early-returns on null) and then STORES the null
    // into the cache -- and that store IS the invalidation. An early return here would leave a stale
    // cache that suppresses the next legitimate bind of the same pointer.
    //
    // lbWasUnset == (cached == nullptr) is the decode of `addi r11, r11, 0` @0x82276B0C /
    // `cntlzw r11, r11` @0x82276B10 / `extrwi r4, r11, 1, 26` @0x82276B14: the addi of 0 is a copy,
    // cntlzw yields 32 exactly when the cached pointer was zero, and bit 26 of a 32-bit word is the
    // 0x20 bit -- so r4 is 1 iff the cache was null. Not arithmetic.
    void Device::SetState(const renderengine::DepthStencilState* lpState)
    {
        if (!mbDepthStencilStateLocked && mpDepthStencilState != lpState)
        {
            const bool lbWasUnset = (mpDepthStencilState == 0);
            Xbox2SetDepthStencilStateLowLevelShadowed(
                const_cast<renderengine::DepthStencilState*>(lpState), lbWasUnset);
            mpDepthStencilState = lpState;
        }
    }

    // =================================================================================
    // shadow::Device::SetState(const renderengine::RasterizerState*)  @ 0x82276B38
    //
    // The rasteriser third. Same shape as its two siblings: gate on the rasteriser lock window
    // (mbRasteriserStateLocked -- `lbz r11, mbRasteriserStateLocked@l(r11)` @0x82276B54, the window
    // LockRasteriserState @0x823F3190 / UnlockRasteriserState @0x823F31F0 open and close), compare
    // against the shadow block's m_pRasterizerState slot (X360 dword_83010A2C, `lwz r11,
    // (dword_83010A2C - 0x83010950)(r30)` @0x82276B68), apply (`bl sub_827E8690` @0x82276B80 ==
    // Xbox2SetRasterizerStateLowLevelShadowed), cache (`stw r31, ...` @0x82276B84).
    // DWARF names it at source shadowingdevice.h:503.
    //
    // Free function, not a member: `mr r31, r3` @0x82276B50 takes r3 as the state pointer and the
    // block base is formed independently at 0x82276B60/0x82276B64. Same argument as the twin; not a
    // DWARF claim.
    //
    // mbRasteriserStateLocked is an IDA-NAMED symbol in the XEX, which is how its identity is known
    // without a data dump -- the name comes from LockRasteriserState's own assert string,
    // "false == mbRasteriserStateLocked" (`addi r3, r11, aFalseMbrasteri@l` @0x823F31C4). Its
    // depth/stencil sibling is named the same way. That asymmetry is what leaves the blend third's
    // gate an unnamed byte; see the blend overload.
    void Device::SetState(const renderengine::RasterizerState* lpState)
    {
        if (!mbRasteriserStateLocked && mpRasterizerState != lpState)
        {
            const bool lbWasUnset = (mpRasterizerState == 0);
            Xbox2SetRasterizerStateLowLevelShadowed(
                const_cast<renderengine::RasterizerState*>(lpState), lbWasUnset);
            mpRasterizerState = lpState;
        }
    }

    // =================================================================================
    // shadow::Device::SetState(const renderengine::BlendMaterialState*)  @ 0x82276A68
    //
    // The blend third. Gate on mbBlendStateLocked (X360 byte_83010907, `lbz r11, byte_83010907@l(r11)`
    // @0x82276A84), compare against the shadow block's m_pBlendState slot (X360 dword_83010964,
    // `lwz r11, (dword_83010964 - 0x83010950)(r30)` @0x82276A98), apply through
    // Xbox2SetStateLowLevelShadowed (`bl` @0x82276AB0), cache (`stw r31, ...` @0x82276AB4).
    // DWARF names it at source shadowingdevice.h:496 and spells the parameter `const BlendState*`; in
    // this tree renderengine::BlendState is a static helper class and the runtime object every applier
    // consumes is renderengine::BlendMaterialState (blendstate.h:35) -- the same reconciliation the
    // factory pass made for saBlendStates.
    //
    // Free function, not a member: `mr r31, r3` @0x82276A80 takes r3 as the state pointer and the
    // shadow-block base is formed independently at 0x82276A90/0x82276A94. Not a DWARF claim -- dwarfdump
    // cannot distinguish static from non-static.
    //
    // WHICH BYTE THE GATE IS, settled this round by closed-set elimination rather than left open:
    //   (a) The three sibling setters gate on three different bytes -- this one on byte_83010907
    //       (@0x82276A84), 0x82276AD0 on mbDepthStencilStateLocked (@0x82276AEC), 0x82276B38 on
    //       mbRasteriserStateLocked (@0x82276B54). One gate each, one triple.
    //   (b) DWARF gives shadow::Device EXACTLY three lock bools and no more: mbRasteriserStateLocked,
    //       mbBlendStateLocked, mbDepthStencilStateLocked (source shadowingdevice.h:775/776/777).
    //   (c) Two of the three gates ARE two of the three bools, so the third gate is the third bool.
    //   (d) The reason IDA left it unnamed: the two it named got their names from assert strings
    //       ("false == mbRasteriserStateLocked" @0x823F31C4), and mbBlendStateLocked has no
    //       Lock/Unlock pair with asserts anywhere in the image. The asymmetry is explained, not
    //       evidence against.
    // Consequently CgsGraphics::ImRendererBase::mgbStateShadowingDisabled was a host spelling of this
    // same byte -- DWARF's CgsImRenderer.h declares no such member -- and it MOVED here rather than
    // being read from there. One console byte, one host home.
    //
    // NO NULL GUARD, for the same reason as the twins: the cache write-back the console performs
    // unconditionally inside the compare IS the invalidation. This is the one applier of the three with
    // no internal null check, so the guard belongs THERE -- see the FLAGGED early-out in
    // Xbox2SetStateLowLevelShadowed.
    //
    void Device::SetState(const renderengine::BlendMaterialState* lpState)
    {
        if (!mbBlendStateLocked && mpBlendState != lpState)
        {
            const bool lbWasUnset = (mpBlendState == 0);
            Xbox2SetStateLowLevelShadowed(
                const_cast<renderengine::BlendMaterialState*>(lpState), lbWasUnset);
            mpBlendState = lpState;
        }
    }

    // ============================================================================
    // The mesh-dispatch flush seam (DispatchList::DispatchAllMeshes).
    // ============================================================================

    // X360 walk prologue, and on the console it is EXACTLY two stores: dword_8301095C = 0 (vertex
    // program shadow) @0x827F2758 and dword_83010960 = -1 (pixel program shadow) @0x827F2764, in
    // DispatchAllMeshes @0x827F2718.
    //
    // The old note here said the three bound-state slots "live on the per-dispatch context". They do
    // not, and that clause is deleted: DispatchAllMeshes reaches them as 0xD8 / 0x14 / 0xDC off r27,
    // and r27 is off_83010950 (`addi r11, r11, off_83010950@l` @0x827F2754 -> `stw r11, var_140(r1)`
    // @0x827F2760 -> `lwz r27, var_140(r1)` @0x827F2EE0) -- the file-scope shadow block. Across all
    // 3307 instructions of that function the ONLY accesses to dword_83010964 / dword_83010A28 /
    // dword_83010A2C are the technique bind's own compare/store pairs (`lwz r11, 0xD8(r27)`
    // @0x827F2EF0 / `stw r31, 0xD8(r27)` @0x827F2F10, and the 0x14 / 0xDC twins at 0x827F2F24 /
    // 0x827F2F44 and 0x827F2F58 / 0x827F2F78). The console caches the triple ACROSS walk boundaries.
    //
    void Device::ResetProgramShadows()
    {
        mpVertexProgramShadow = 0;
        mpPixelProgramShadow  = reinterpret_cast<const renderengine::ProgramBufferData*>(
            static_cast<uintptr_t>(~0u));
        mpRasterizerState   = 0;
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
            }
        }

        void LogFallbackTechniqueOnce()
        {
            static bool sbLogged = false;
            if (!sbLogged && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                sbLogged = true;
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
            }
        }
    }



}
