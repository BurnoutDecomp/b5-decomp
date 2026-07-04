#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"   // Tint
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"     // ProgramBuffer
#include "pc/gcm/renderengine/texture.h"                                        // Texture
#include "pc/gcm/renderengine/renderstates.h"                                   // TextureState
#include "rw/rwcore_structs.h"                                                  // rw::Resource / BaseResourceDescriptors<5>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::Tint::Initialize           @ 0x82403B48
//   rw::graphics::postfx::Tint::InitializePixelProgram @ 0x823FE7F0
//
// Initialize builds the effect's colour-lookup texture (a muSize^3 volume, format 4) and its sampler
// texture-state into the Tint's two rw resources via the resource allocator handed in Parameters,
// binds the shared pixel program's "lookupOffset" constant, and zeroes the colour-lookup offset.
// InitializePixelProgram compiles the shared tint pixel program once into the file-scope program slot
// the effects sample (the rw-resource create pattern: GetResourceDescriptor -> allocator->Allocate ->
// ProgramBuffer::Initialize; see the committed coronas/rwgcoronarenderer.cpp for the same idiom).
// Stores follow the X360 asm; the SIMD store of m_colourLookupOffset is the all-zero vector (the
// earlier "1" the decompiler showed is a reused stack slot, overwritten with 0.0 before the store).

namespace
{
    // The tint pixel-shader microcode (344 bytes) embedded in the X360 image at unk_82044D30.
    // HONEST PLACEHOLDER: compiled Xenos shader bytecode -- platform data with no recoverable bytes
    // from the function-only exports; declared so the parameter block can name it.
    extern const u8 gauTintPixelProgramMicrocode;   // X360 &unk_82044D30
    const u8 gauTintPixelProgramMicrocode = 0;

    // The packed texture format/usage word the X360 writes into the lookup-texture Parameters
    // (var_A8 == 0x18280186). Treated as an opaque platform format descriptor.
    const u32 KU_TINT_LOOKUP_TEXTURE_FORMAT = 0x18280186u;

    // The shared tint pixel program + its rw resource handle array (X360 dword_82FAEE8C /
    // dword_82FAFF48). InitializePixelProgram fills them; Initialize reads gpTintPixelProgram.
    rw::Resource                     gTintPixelProgramResource;   // X360 dword_82FAFF48
    renderengine::ProgramBufferData* gpTintPixelProgram = nullptr; // X360 dword_82FAEE8C
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 0x823FE7F0.
    renderengine::ProgramBufferData* Tint::InitializePixelProgram(rw::IResourceAllocator* lpAllocator)
    {
        renderengine::ProgramBufferParameters lParams = {};
        lParams.muFunction   = static_cast<u32>(reinterpret_cast<usize>(&gauTintPixelProgramMicrocode));
        lParams.muShaderType = 1;     // pixel program
        lParams.muReserved8  = 344;   // X360 var_68 == 0x158: the microcode blob size

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParams);

        // Allocate the program's rw resource (the X360 vtable+0x10 allocator call -> DoAllocate on PC).
        gTintPixelProgramResource =
            lpAllocator->DoAllocate(reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);

        gpTintPixelProgram = renderengine::ProgramBuffer::Initialize(
            reinterpret_cast<renderengine::ProgramResourceLayout*>(&gTintPixelProgramResource), &lParams);
        return gpTintPixelProgram;
    }

    // X360 0x82403B48.
    Tint* Tint::Initialize(const rw::Resource& lrResource, const Parameters& lrParameters)
    {
        // The constructed Tint lives in resource slot 0 (the rw-resource create convention).
        Tint* lpTint = static_cast<Tint*>(lrResource.m_baseResources[0]);

        lpTint->m_blendLock = false;                       // stb 0, +0x80
        lpTint->m_allocator = lrParameters.mpAllocator;    // +0xF0 = the rw allocator

        // --- the colour-lookup texture (a muSize x muSize x muSize volume) ---------------------------
        renderengine::Texture::Parameters lTextureParams;
        lTextureParams.miFormat    = 4;
        lTextureParams.muSysMem    = 0;
        lTextureParams.muWidth     = lrParameters.muSize;
        lTextureParams.muHeight    = lrParameters.muSize;
        lTextureParams.muDepth     = lrParameters.muSize;
        lTextureParams.muNumLevels = 1;
        lTextureParams.muReserved0 = KU_TINT_LOOKUP_TEXTURE_FORMAT;
        lTextureParams.muReserved1 = 0;

        rw::BaseResourceDescriptors<5> lTextureDescriptor;
        renderengine::Texture::GetResourceDescriptor(reinterpret_cast<u32*>(&lTextureDescriptor), &lTextureParams);
        lpTint->m_textureTintMapResource = lpTint->m_allocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lTextureDescriptor), nullptr);
        lpTint->m_textureTintMap =
            renderengine::Texture::Initialize(&lpTint->m_textureTintMapResource, &lTextureParams);

        // --- the sampler texture-state for that lookup texture ---------------------------------------
        renderengine::TextureState::Parameters lStateParams = {};
        lStateParams.muAddressU      = 2;
        lStateParams.muAddressV      = 2;
        lStateParams.muAddressW      = 2;
        lStateParams.muMagFilter     = 1;
        lStateParams.muMinFilter     = 1;
        lStateParams.muMipFilter     = 0;
        lStateParams.muField6        = 1;
        lStateParams.muField7        = 1;
        lStateParams.muMaxAnisotropy = 13;
        lStateParams.muField9        = 0;
        lStateParams.muField10       = 1;
        lStateParams.mfMipLodBias    = 0.0f;
        lStateParams.mfField12       = 0.0f;
        lStateParams.mu8Field43      = 1;
        lStateParams.mu8Field44      = 1;
        lStateParams.mpTexture       = lpTint->m_textureTintMap;

        rw::BaseResourceDescriptors<5> lStateDescriptor;
        renderengine::TextureState::GetResourceDescriptor(reinterpret_cast<u32*>(&lStateDescriptor));
        lpTint->m_textureStateTintMapResource = lpTint->m_allocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lStateDescriptor), nullptr);
        lpTint->m_textureStateTintMap =
            renderengine::TextureState::Initialize(&lpTint->m_textureStateTintMapResource, &lStateParams);

        // Bind the shared pixel program's lookup-offset constant, then zero the offset vector.
        renderengine::ProgramBuffer::GetVariableHandleByName(
            gpTintPixelProgram, reinterpret_cast<const u8*>("lookupOffset"), &lpTint->m_colourLookupOffsetHandle);
        lpTint->m_colourLookupOffset[0] = 0.0f;
        lpTint->m_colourLookupOffset[1] = 0.0f;
        lpTint->m_colourLookupOffset[2] = 0.0f;
        lpTint->m_colourLookupOffset[3] = 0.0f;

        return lpTint;
    }

    // X360 0x823F8310. Lock the colour-lookup (tint-map) texture's surface, then publish its geometry
    // + destination pixel pointer into the blend-job parameter block and hand a reference back to the
    // caller (BrnPostFx::BeginTintBlend) that schedules the EA::Jobs colour-cube blend into it.
    // numSources / src[] / factor[] are filled by the caller; this only seeds the destination surface.
    TintBlendParameters& Tint::BeginBlendJob()
    {
        renderengine::Texture::Lock(m_textureTintMap, 2, 0, 0, &m_textureLock);

        m_blendParameters.size           = renderengine::Texture::GetWidth(m_textureTintMap);
        m_blendParameters.dstStride      = m_textureLock.muStride;
        m_blendParameters.dstSliceStride = m_textureLock.muSliceStride;
        m_blendParameters.dst            = static_cast<u8*>(m_textureLock.mpPixelData);
        return m_blendParameters;
    }
}
}
}
