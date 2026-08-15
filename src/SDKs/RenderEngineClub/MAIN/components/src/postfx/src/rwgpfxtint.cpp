#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"   // Tint
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"  // PfxHelper::CreateProgram
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
    //
    // ⚠ THE PREVIOUS PLACEHOLDER WAS DANGEROUS AND IS GONE. It was a ONE-BYTE object whose address
    // was handed to CreateProgram alongside the real 344-byte SIZE; had the console route ever run
    // it would have read 343 bytes past the end of a one-byte global. The pointer is null now: the
    // bytes are absent, CreateProgram refuses a null image, and the size stays as documentation.
    //
    // [FLAG BLOCKED: the Xenos package at X360 0x82044D30, 344 bytes (pixel), exposing the named
    //  constant "lookupOffset". DATA_DUMP.md:1722 carries only its first 32 bytes. Extractable from
    //  the ARTIST .i64 the same way rwgpfxrendertargetdebugger.cpp already extracted its own pair.]
    const void* const KP_TINT_PIXEL_PROGRAM_MICROCODE = nullptr;   // X360 &unk_82044D30
    const u32         KU_TINT_PIXEL_PROGRAM_SIZE      = 344u;      // X360 var_68 == 0x158

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
    //
    // ⚠ REWRITTEN ONTO PfxHelper::CreateProgram. The committed body open-coded the console's
    // GetResourceDescriptor -> DoAllocate -> ProgramBuffer::Initialize route, which on the PC backend
    // is a CRASH, not a gap: both of those vendor bodies call XGGetMicrocodeShaderParts, whose PC
    // stub returns 0 without writing *lpParts (ImmediateModePCLeaf.cpp:626-646). Routing through
    // CreateProgram puts this site behind the same single gate as every other post-fx program (which
    // also tries a PC image first via ProgramBufferPC_Adopt), and the console's own parameter shape
    // -- shader type 1, the blob, its size, the caller's allocator -- is exactly CreateProgram's
    // argument list, so nothing about the call is invented.
    //
    // gTintPixelProgramResource is no longer written here: on the gated path there is no resource to
    // record, and CreateProgram owns the carve on the console path. It is kept (and kept null) so the
    // eventual ReleasePixelProgram (DWARF rwgpfxtint.h:43) has its member.
    renderengine::ProgramBufferData* Tint::InitializePixelProgram(rw::IResourceAllocator* lpAllocator)
    {
        gpTintPixelProgram = PfxHelper::CreateProgram(1, KP_TINT_PIXEL_PROGRAM_MICROCODE,
                                                      KU_TINT_PIXEL_PROGRAM_SIZE, lpAllocator);
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
        // The null test is a PC bring-up guard only: the committed lookup dereferences its first
        // argument unconditionally (programbuffer.cpp:263), and on this build the program is null
        // because its microcode is absent. A zero register count is what the lookup itself writes
        // when a name misses, so the "not bound" state is spelled the same way either way.
        // [FLAG PC bring-up: the tint pixel program's microcode is absent -- see the note above]
        if (gpTintPixelProgram != nullptr)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                gpTintPixelProgram, reinterpret_cast<const u8*>("lookupOffset"),
                &lpTint->m_colourLookupOffsetHandle);
        }
        else
        {
            lpTint->m_colourLookupOffsetHandle.mu8RegisterSet   = 0u;
            lpTint->m_colourLookupOffsetHandle.mu8RegisterIndex = 0u;
            lpTint->m_colourLookupOffsetHandle.mu8ShaderType    = 0u;
            lpTint->m_colourLookupOffsetHandle.mu8RegisterCount = 0u;
        }
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

        // asm 0x823F835C `stw r9, 0(r31)`   -- size           <- Texture::GetWidth
        //     0x823F8358 `stw r11, 8(r31)`  -- dstStride      <- lock +0x08 (muStride)
        //     0x823F8360 `stw r10, 0xC(r31)`-- dstSliceStride <- lock +0x14 (muSliceStride)
        //     0x823F8364 `stw r8, 0x10(r31)`-- dst            <- lock +0x04 (mpPixelData)
        // numSources / src[] / factor[] are the caller's (BrnPostFx::BeginTintBlend); this seeds
        // only the destination surface, and it deliberately does NOT touch +0x04.
        m_blendParameters.size           = renderengine::Texture::GetWidth(m_textureTintMap);
        m_blendParameters.dstStride      = m_textureLock.muStride;
        m_blendParameters.dstSliceStride = m_textureLock.muSliceStride;
        m_blendParameters.dst            = static_cast<u8*>(m_textureLock.mpPixelData);
        return m_blendParameters;
    }

    // ============================================================================================
    // Tint::EndBlendJob -- INLINED on the X360 at BrnPostFx::Render 0x8240A4DC-0x8240A4F4.
    //
    // The flattened form is `lwz r3, 0xC0(r11)` / `addi r4, r11, 0xA4` / `bl Texture__Unlock`, i.e.
    // Unlock(m_textureTintMap, &m_textureLock) -- the exact counterpart of BeginBlendJob's Lock, on
    // the two members this class already names. Nothing else: m_blendLock is NOT cleared here (the
    // caller clears its own m_processTint flag), and that omission is faithful.
    // ============================================================================================
    void Tint::EndBlendJob()
    {
        renderengine::Texture::Unlock(m_textureTintMap, &m_textureLock);
    }

    // ============================================================================================
    // Tint::Release -- INLINED on the X360 at BrnPostFx::Destruct 0x82408150-0x82408188.
    //
    //   renderengine::Texture::Release(m_textureTintMap);        (`lwz r3, 0xC0(r31)`)
    //   m_allocator->DoFree(m_textureTintMapResource);           (`lwz r3, 0xF0(r31)`, `addi r4, r31, 0xC8`)
    //   m_allocator->DoFree(m_textureStateTintMapResource);      (`addi r4, r31, 0xDC`)
    //
    // THREE THINGS THE CONSOLE DOES NOT DO, all reproduced by omission:
    //   * no null test on any of the three (the asm has none -- both `bctrl`s are unconditional);
    //   * no renderengine::TextureState::Release on m_textureStateTintMap -- only its RESOURCE is
    //     handed back, which is why the sampler object leaks its own D3D reference on the console
    //     too;
    //   * no member is nulled afterwards, so a double Release would double-free. BrnPostFx::Destruct
    //     is the only caller and calls it once per tint buffer.
    // The rw::Resource members are passed BY REFERENCE in place (`addi r4, r31, 0xC8` is the address
    // of the member, not a stack copy), matching rwcore_structs.h's DoFree(const rw::Resource&) --
    // the vtbl+0x14 release slot, the same convention PfxHelper::SafeRelease uses.
    // ============================================================================================
    void Tint::Release()
    {
        renderengine::Texture::Release(m_textureTintMap);
        m_allocator->DoFree(m_textureTintMapResource);
        m_allocator->DoFree(m_textureStateTintMapResource);
    }
}
}
}
