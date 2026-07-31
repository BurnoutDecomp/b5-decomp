#include "GameSource/Graphics/ImmediateMode/BrnIm3d.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint (the one-shot PC gate)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device
#include "rw/rwcore_structs.h"                                         // rw::IResourceAllocator

#include <cmath>     // sqrtf
#include <cstring>   // std::memcpy / std::memset

// =============================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGraphics::Im3dSkyDome::Construct     @ 0x82406598
//   BrnGraphics::Im3dSkyDome::SetConstants  @ 0x823FFEF0
//
// Construct builds the ImRenderer<Im3dSkyDomeVertex> with ONE program pair (the sky
// dome vertex + pixel programs), then resolves sixteen named shader constants on it and
// creates the clouds sampler state. Every name below is the literal string the X360
// passes to renderengine::ProgramBuffer::GetVariableHandleByName -- none is guessed.
//
// SetConstants pushes one frame's worth of those constants. The X360 passes the vectors
// in v1..v13 plus one stack vector and the two textures in r4/r5; the register order maps
// one-to-one onto the DecFIGS mangled signature
//   SetConstants(Vector3Plus, Vector3, Vector4, Vector4, Vector4, Vector3,
//                const Texture*, const Texture*, Vector4 x8)
// and onto the handle table in the order Construct resolved it.
//
// The ONE piece of hand-VMX in SetConstants is the "KeyLightDirAndXZLength" pack:
//     vspltisw v13,-1 / vslw v11,v13,v13      -> the 0x80000000 sign-mask broadcast
//     lvx      v7, unk_82CDA450               -> a lane-gather permute mask
//     vperm    v12, v0, v0, v7                -> gather two lanes of the direction
//     vxor     v0, v0, v11                    -> negate all four lanes
//     vmulfp   v13, v12, v12 / vspltw+vaddfp  -> perm.w0^2 + perm.w1^2
//     fsqrts                                  -> the packed w component
//
// unk_82CDA450 shows as an UNVALUED .rdata symbol in the function exports (it is the same
// mask the ledger records as blocking BrnEnvironmentUtil.cpp's ComputeSkyColour /
// ComputeIrradianceRigFromSky). Its 16 bytes were read out of the ARTIST database with
// headless IDA 9.3 and it is NOT opaque:
//     00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03
// With both vperm sources set to the same register, that selects source words
// {0, 2, 0, 0} -- i.e. it gathers (X, Z, X, X). So
//     w = sqrt(perm.w0^2 + perm.w1^2) = sqrt(dir.x^2 + dir.z^2)
// which is exactly the XZ-plane length the destination constant's own name promises.
// The mask value and the constant name agree independently.
// =============================================================================

// The two converted sky-dome program images (pc/gcm/renderengine/SkyDomeProgramsPC.cpp --
// a generated PC-platform leaf). Declared here as the minimal external surface, the same
// convention the other PC-leaf seams in this tree use.
namespace renderengine
{
    extern const u8  gauSkyDomeVertexProgramPC[];
    extern const u32 guSkyDomeVertexProgramPCSize;
    extern const u8  gauSkyDomePixelProgramPC[];
    extern const u32 guSkyDomePixelProgramPCSize;
}

namespace
{
    // ---- the two sky-dome program binaries --------------------------------------------
    // The X360 Construct passes &unk_8203D278 / 1312 bytes as the vertex program and
    // &unk_8203D798 / 780 bytes as the pixel program -- guest .data Xenos microcode with
    // no PC counterpart, so BOTH the addresses and the sizes are console-only.
    //
    // FLAG PC-platform leaf: the PC build supplies the SAME TWO TECHNIQUES, recompiled
    // from their TUB HLSL (nushaders/Reference/TUB/Executable/28.fx and 29.fx -- pinned
    // to this renderer by an exact 10 + 7 match against the seventeen constant names
    // resolved below) with the shader converter's own fxc line, then wrapped into
    // platform-4 ShaderProgramBuffer images. They are module data, not bundle
    // resources, exactly as on the console. Sizes are the WRAPPED image sizes, not the
    // console's microcode sizes: ImRenderer<V>::AddProgram's PC seam adopts the image
    // whole (see CgsIm3dSkyDome.cpp).
    const u32 KU_SKYDOME_VERTEX_PROGRAM_SIZE = renderengine::guSkyDomeVertexProgramPCSize;
    const u32 KU_SKYDOME_PIXEL_PROGRAM_SIZE  = renderengine::guSkyDomePixelProgramPCSize;

    // ---- the clouds sampler-state parameter block (X360 Construct tail, store-for-store)
    // Sixteen words + five trailing flag bytes, exactly the renderengine sampler parameter
    // block CgsResource::Font::CreateTextureState builds (renderstates.h TextureState::
    // Parameters). Every value is an asm immediate.
    struct SamplerStateParameters
    {
        u32 muAddressU;        // +0x00  0
        u32 muAddressV;        // +0x04  0
        u32 muAddressW;        // +0x08  0
        u32 muMagFilter;       // +0x0C  1
        u32 muMinFilter;       // +0x10  1
        u32 muMipFilter;       // +0x14  1
        u32 muField6;          // +0x18  0
        u32 muField7;          // +0x1C  0
        u32 muMaxAnisotropy;   // +0x20  13
        u32 muField9;          // +0x24  0
        u32 muField10;         // +0x28  1
        f32 mfMipLodBias;      // +0x2C  0.0
        f32 mfField12;         // +0x30  0.0
        u32 muField13;         // +0x34  0
        u32 muField14;         // +0x38  0
        u32 muField15;         // +0x3C  0
        u8  mu8Field40;        // +0x40  0
        u8  mu8Field41;        // +0x41  0
        u8  mu8Field42;        // +0x42  0
        u8  mu8Field43;        // +0x43  1
        u8  mu8Field44;        // +0x44  1
        u8  mau8Pad45[3];      // +0x45  align
    };

    // Copy a 16-byte vector into a scratch 4-float array (the X360 spills each Get* result
    // onto the stack before loading it into a vector register; the lane order is untouched).
    inline void Lanes(const void* lpVector, f32 lafOut[4])
    {
        std::memcpy(lafOut, lpVector, 16);
    }
}

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- the shared decl-only
// surface the committed immediate-mode TUs already use (CgsIm2dColTex.cpp:82, CgsIm2dUntex.cpp:72,
// CgsIm3dSkyDome.cpp). It opens one 16-byte shader-constant row and returns the write cursor.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace
{
    // One shader-constant write: open the row for lpHandle and copy the 16-byte value into it.
    // The X360 spells this inline per constant --
    //     renderengine::Device::BeginShaderStates(this + handleOffset, &cursor);
    //     stvx128 <value>, cursor;   cursor += 16;
    // -- with ONE cursor threaded across all the writes. Re-fetching per row (as the committed
    // ImRenderer<V>::SetTransform reconstruction does) keeps the observable stores identical.
    void PushShaderConstant(renderengine::ProgramVariableHandle* lpHandle, const void* lpValue16)
    {
        void* lpShaderState = nullptr;
        RenderEngineDeviceBeginShaderStates(lpHandle, &lpShaderState);
        if (lpShaderState != nullptr)
        {
            std::memcpy(lpShaderState, lpValue16, 16);
        }
    }

    // The renderengine sampler-state construction surface. Mirrors the BrnSunCorona.cpp
    // convention: the two committed renderengine state homes declare mutually exclusive models,
    // so the one call this TU needs is expressed locally against the opaque object pointer. The
    // X360 sequence is SamplerState::GetResourceDescriptor -> the allocator's vtable slot +0x10
    // -> SamplerState::Initialize, and the result is stored at this+0x100. The parameter block is
    // built store-for-store above and handed to this seam; the renderengine SamplerState resource
    // pipeline itself is out of scope (blocked class:renderengine::Device).
    // FLAG PC-platform leaf: renderengine SamplerState resource pipeline not reconstructed yet.
    const renderengine::SamplerState* BuildSamplerState(rw::IResourceAllocator* lpAllocator,
                                                        const SamplerStateParameters* lpParameters)
    {
        (void)lpAllocator;
        (void)lpParameters;
        return nullptr;
    }
}

namespace BrnGraphics
{

// -----------------------------------------------------------------------------------------
// Im3dSkyDome::Construct  X360 0x82406598
//
// Build the underlying ImRenderer with the single sky-dome program pair, resolve the
// sixteen named shader constants against it (the first nine on the VERTEX program at
// mapVertexProgramBuffer[0] == this+0x14, the last seven on the PIXEL program at
// mapPixelProgramBuffer[0] == this+0x34), then create the clouds sampler state.
//
// The X360 re-asserts "mapVertexProgramBuffer[ li8Program ] != NULL" (CgsImRenderer.h:570)
// before EVERY lookup; that assert is hoisted here into one check per program table.
// -----------------------------------------------------------------------------------------
void Im3dSkyDome::Construct(rw::IResourceAllocator* lpAllocator)
{
    // The one program pair (X360: the two guest .data microcode blobs; PC: the two
    // converted ShaderProgramBuffer images -- see the KU_SKYDOME_*_PROGRAM_SIZE note).
    const void* lapVertexProgramBinary[1] = { renderengine::gauSkyDomeVertexProgramPC };
    const void* lapPixelProgramBinary[1]  = { renderengine::gauSkyDomePixelProgramPC };
    const u32   lauVertexProgramSize[1]   = { KU_SKYDOME_VERTEX_PROGRAM_SIZE };
    const u32   lauPixelProgramSize[1]    = { KU_SKYDOME_PIXEL_PROGRAM_SIZE };

    CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::Construct(
        lpAllocator,
        lapVertexProgramBinary, lauVertexProgramSize,
        lapPixelProgramBinary,  lauPixelProgramSize,
        1);

    // ---- vertex-program constants (X360 lwz r3, 0x14(r31) before each lookup) ----------
    renderengine::ProgramBufferData* const lpVertexProgram =
        reinterpret_cast<renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]);
    if (lpVertexProgram != nullptr)
    {
        // "worldViewProj" lands in maShaderStateBlocks[0], NOT in maStateHandles. The X360 target
        // is `this + 88` (0x58), which is exactly `this + 4*(mi8CurrentProgram + 22)` for slot 0 --
        // the address ImRenderer<V>::SetTransform hands to BeginShaderStates. So the per-slot
        // "shader state block" word IS that slot's world-view-projection ProgramVariableHandle,
        // and SetTransform pushes the matrix through it.
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("worldViewProj"),
            reinterpret_cast<renderengine::ProgramVariableHandle*>(&maShaderStateBlocks[0]));
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("ViewPositionAndSkyScale"), &maStateHandles[0]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("KeyLightDirAndXZLength"), &maStateHandles[1]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("TopColourDrk"), &maStateHandles[2]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("HorColourPow"), &maStateHandles[3]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("SunColourPow"), &maStateHandles[4]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("HorBleedSclPow"), &maStateHandles[5]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("g_domeRanges"), &maStateHandles[6]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("g_textureScaleAndOffsets"), &maStateHandles[7]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram, reinterpret_cast<const u8*>("g_scatteringCoefficients"), &maStateHandles[8]);
    }
    else
    {
        CGS_ASSERT(false, "mapVertexProgramBuffer[ li8Program ] != NULL");
    }

    // ---- pixel-program constants (X360 lwz r3, 0x34(r31) before each lookup) -----------
    renderengine::ProgramBufferData* const lpPixelProgram =
        reinterpret_cast<renderengine::ProgramBufferData*>(mapPixelProgramBuffer[0]);
    if (lpPixelProgram != nullptr)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_densitySampler"), &maStateHandles[9]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_lightSampler"), &maStateHandles[10]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_darkColour"), &maStateHandles[11]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_liteColour"), &maStateHandles[12]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_layerCloudiness"), &maStateHandles[13]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_layerInvFeather"), &maStateHandles[14]);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram, reinterpret_cast<const u8*>("g_layerAlphas"), &maStateHandles[15]);
    }
    else
    {
        CGS_ASSERT(false, "mapPixelProgramBuffer[ li8Program ] != NULL");
    }

    // ---- the clouds sampler state (X360 Construct tail; every value is an immediate) ---
    SamplerStateParameters lSamplerParameters;
    std::memset(&lSamplerParameters, 0, sizeof(lSamplerParameters));
    lSamplerParameters.muAddressU      = 0u;
    lSamplerParameters.muAddressV      = 0u;
    lSamplerParameters.muAddressW      = 0u;
    lSamplerParameters.muMagFilter     = 1u;
    lSamplerParameters.muMinFilter     = 1u;
    lSamplerParameters.muMipFilter     = 1u;
    lSamplerParameters.muField6        = 0u;
    lSamplerParameters.muField7        = 0u;
    lSamplerParameters.muMaxAnisotropy = 13u;
    lSamplerParameters.muField9        = 0u;
    lSamplerParameters.muField10       = 1u;
    lSamplerParameters.mfMipLodBias    = 0.0f;
    lSamplerParameters.mfField12       = 0.0f;
    lSamplerParameters.muField13       = 0u;
    lSamplerParameters.muField14       = 0u;
    lSamplerParameters.muField15       = 0u;
    lSamplerParameters.mu8Field40      = 0u;
    lSamplerParameters.mu8Field41      = 0u;
    lSamplerParameters.mu8Field42      = 0u;
    lSamplerParameters.mu8Field43      = 1u;
    lSamplerParameters.mu8Field44      = 1u;

    mpCloudsSamplerState = BuildSamplerState(lpAllocator, &lSamplerParameters);
}

// -----------------------------------------------------------------------------------------
// Im3dSkyDome::SetConstants  X360 0x823FFEF0
//
// Push one frame's sky + cloud constants at the bound program and bind the two cloud
// textures through the clouds sampler state. The X360 order (which is the order the
// constants are written, NOT the parameter order) is:
//     [0] ViewPositionAndSkyScale      [7] g_textureScaleAndOffsets
//     [1] KeyLightDirAndXZLength       [8] g_scatteringCoefficients
//     [2] TopColourDrk                 [6] g_domeRanges
//     [3] HorColourPow                 -- then the two samplers --
//     [4] SunColourPow                 [11] g_darkColour   [12] g_liteColour
//     [5] HorBleedSclPow (w forced 0)  [13] g_layerCloudiness
//                                      [14] g_layerInvFeather  [15] g_layerAlphas
// -----------------------------------------------------------------------------------------
void Im3dSkyDome::SetConstants(Vector3Plus lViewPositionAndSkyScale,
                               Vector3     lKeyLightDirection,
                               Vector4     lSky_TopColourDrk,
                               Vector4     lSky_HorColourPow,
                               Vector4     lSky_SunColourPow,
                               Vector3     lSky_HorBleedSclPow,
                               const renderengine::Texture* lpLayer0Density,
                               const renderengine::Texture* lpLayer0Lighting,
                               Vector4     lFogScattering,
                               Vector4     lCloudDarkColour0,
                               Vector4     lCloudLiteColour0,
                               Vector4     lCloudDomeRanges,
                               Vector4     lCloudTextureScaleAndOffsets0,
                               Vector4     lCloudLayerDensity,
                               Vector4     lCloudLayerInvFeather,
                               Vector4     lCloudLayerOpacity)
{
    PushShaderConstant(&maStateHandles[0], &lViewPositionAndSkyScale);

    // "KeyLightDirAndXZLength": negate every lane of the key-light direction and replace the
    // w lane with the direction's XZ-plane length (see the file banner for how the two
    // gathered lanes are pinned by the constant's own name). Note sqrt(x^2 + z^2) is
    // sign-invariant, so it is the same before and after the negation -- which is why the
    // X360 can compute it from the un-negated vector.
    f32 lafKeyLight[4];
    Lanes(&lKeyLightDirection, lafKeyLight);
    f32 lafKeyLightDirAndXZLength[4];
    lafKeyLightDirAndXZLength[0] = -lafKeyLight[0];
    lafKeyLightDirAndXZLength[1] = -lafKeyLight[1];
    lafKeyLightDirAndXZLength[2] = -lafKeyLight[2];
    lafKeyLightDirAndXZLength[3] =
        sqrtf((lafKeyLight[0] * lafKeyLight[0]) + (lafKeyLight[2] * lafKeyLight[2]));
    PushShaderConstant(&maStateHandles[1], lafKeyLightDirAndXZLength);

    PushShaderConstant(&maStateHandles[2], &lSky_TopColourDrk);
    PushShaderConstant(&maStateHandles[3], &lSky_HorColourPow);
    PushShaderConstant(&maStateHandles[4], &lSky_SunColourPow);

    // HorBleedSclPow is a Vector3: the X360 spills it to a 12-byte slot and writes 0.0f into
    // the trailing float before reloading the 16 bytes, so the w lane is explicitly cleared.
    f32 lafHorBleedSclPow[4];
    Lanes(&lSky_HorBleedSclPow, lafHorBleedSclPow);
    lafHorBleedSclPow[3] = 0.0f;
    PushShaderConstant(&maStateHandles[5], lafHorBleedSclPow);

    PushShaderConstant(&maStateHandles[7], &lCloudTextureScaleAndOffsets0);
    PushShaderConstant(&maStateHandles[8], &lFogScattering);
    PushShaderConstant(&maStateHandles[6], &lCloudDomeRanges);

    // The two cloud textures, each bound on the sampler slot its own handle names, through the
    // shared clouds sampler state. The X360 reads the slot with a BYTE load off the handle:
    //     lbz r30, 0xE4(r31)   -> maStateHandles[ 9] byte 0   (g_densitySampler)
    //     lbz r30, 0xE8(r31)   -> maStateHandles[10] byte 0   (g_lightSampler)
    // then SetState(mpCloudsSamplerState, slot) / SetResource(texture, slot). Byte 0 of the
    // handle is mu8RegisterSet in the committed ProgramVariableHandle model; for a SAMPLER
    // handle it carries the sampler-unit index (shadow::Device::SetState asserts slot < 16, so
    // it cannot be the whole 4-byte word).
    {
        void* const lpSamplerState =
            const_cast<void*>(static_cast<const void*>(mpCloudsSamplerState));

        const u32 luDensitySampler = maStateHandles[9].mu8RegisterSet;
        shadow::Device::SetState(lpSamplerState, luDensitySampler);
        shadow::Device::SetResource(const_cast<void*>(static_cast<const void*>(lpLayer0Density)),
                                    luDensitySampler);

        const u32 luLightSampler = maStateHandles[10].mu8RegisterSet;
        shadow::Device::SetState(lpSamplerState, luLightSampler);
        shadow::Device::SetResource(const_cast<void*>(static_cast<const void*>(lpLayer0Lighting)),
                                    luLightSampler);
    }

    PushShaderConstant(&maStateHandles[11], &lCloudDarkColour0);
    PushShaderConstant(&maStateHandles[12], &lCloudLiteColour0);
    PushShaderConstant(&maStateHandles[13], &lCloudLayerDensity);
    PushShaderConstant(&maStateHandles[14], &lCloudLayerInvFeather);
    PushShaderConstant(&maStateHandles[15], &lCloudLayerOpacity);
}

}   // namespace BrnGraphics
