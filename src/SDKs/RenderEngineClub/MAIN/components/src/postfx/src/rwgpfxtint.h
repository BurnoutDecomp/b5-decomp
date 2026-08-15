#ifndef RW_GPFX_TINT_H
#define RW_GPFX_TINT_H

#include "types.hpp"
#include "rw/rwcore_structs.h"                                                  // rw::Resource
#include "pc/gcm/renderengine/texture.h"                                        // renderengine::Texture
#include "pc/gcm/renderengine/renderstates.h"                                   // renderengine::TextureState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"     // renderengine::ProgramVariableHandle

// rw::graphics::postfx::Tint -- the post-fx colour-tint effect. It owns a 3D colour-lookup texture
// (the tint map) + its sampler state and a shared pixel program; Initialize builds the lookup
// texture + texture-state into the effect's rw resources and binds the program's "lookupOffset"
// constant, while the static InitializePixelProgram compiles the shared tint pixel program once.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../postfx/include/rwgpfxtint.h), member offsets confirmed against
// the X360 binary (Tint::Initialize @0x82403B48):
//   m_blendParameters             +0x00  (TintBlendParameters -- not touched by this TU; opaque)
//   m_blendLock                   +0x80  (cleared to 0)
//   m_colourLookupOffset          +0x90  (Vector4, zeroed)
//   m_colourLookupOffsetHandle    +0xA0
//   m_textureLock                 +0xA4  (Texture::Locked -- not touched; opaque)
//   m_textureTintMap              +0xC0
//   m_textureStateTintMap         +0xC4
//   m_textureTintMapResource      +0xC8  (rw::Resource handle array)
//   m_textureStateTintMapResource +0xDC
//   m_allocator                   +0xF0
// (Offsets are the X360 4-byte-pointer image; the PC build lays the named members out for its own
// pointer width -- semantic parity, not byte-matching, so the layout is intentionally not pinned.)

// The rw resource allocator the post-fx effects are handed is rw::IResourceAllocator (rwcore_structs.h,
// from rwcore.pdb): its DoAllocate(descriptor, name) -> rw::Resource is the PC-semantic equivalent of
// the X360 create path's allocator virtual (the X360 vtable+0x10 call that fills a handle array).

namespace rw
{
namespace graphics
{
namespace postfx
{
    // TintBlendParameters -- the parameter block the EA::Jobs colour-cube blend runs against
    // (DWARF rwgpfxtintblender.h). Tint::BeginBlendJob seeds only the destination-surface
    // geometry (size / dst strides / dst pointer) from the locked tint-map texture; the caller
    // (BrnPostFx::BeginTintBlend) fills numSources / src[] / factor[] before scheduling the job.
    // The X360 block is 0x80 bytes (m_blendLock follows at +0x80); the named destination fields
    // are laid out first and the caller-filled remainder is carried as opaque source storage
    // (PC-semantic parity, not byte-matched).
    // ⚠ LAYOUT CORRECTION -- THE COMMITTED VERSION WAS MISSING `numSources` AND SHIFTED EVERY
    // FIELD AFTER IT BY ONE WORD. The DWARF names all eight members
    // (references/DecFIGS/dwarfdump/.../include/postfx/rwgpfxtintblender.h:14-22) and BOTH X360
    // writers agree with it word for word:
    //   Tint::BeginBlendJob @0x823F8310:  `stw r9, 0(r31)` size, `stw r11, 8(r31)` dstStride,
    //                                     `stw r10, 0xC(r31)` dstSliceStride, `stw r8, 0x10(r31)` dst
    //   BrnPostFx::BeginTintBlend @0x823F83D8: `stw r11, 4(r29)` numSources, then src[] from +0x14
    //                                     and factor[] from +0x2C
    // and the consumer proves it too: rw::graphics::postfx::TintBlend @0x82AD4860 dispatches on
    // `lwz r10, 4(r3)` -- the source count -- as its jump-table index. With the committed layout
    // BeginBlendJob wrote the row pitch into the slot the job reads as its VARIANT INDEX, i.e. an
    // out-of-range indirect jump the moment the tint blend ever ran.
    struct TintBlendParameters
    {
        u32   size;            // +0x00  colour-lookup edge (== texture width)
        u32   numSources;      // +0x04  cube count the blend job mixes (the TintBlend jump index)
        u32   dstStride;       // +0x08  destination row pitch (bytes)
        u32   dstSliceStride;  // +0x0C  destination slice pitch (bytes)
        u8*   dst;             // +0x10  destination pixel data
        u8*   src[6];          // +0x14  source cube pixel data (filled by BrnPostFx::BeginTintBlend)
        f32   factor[6];       // +0x2C  per-source blend weight
        u8    pad[60];         // +0x44  the block is 0x80 bytes on the guest (m_blendLock follows)
    };

    class Tint
    {
    public:
        // Tint construction parameters (DWARF Tint::Parameters). Only the leading edge-length word
        // and the allocator are read by Initialize.
        struct Parameters
        {
            u32                    muSize;       // +0x00 the lookup-texture edge (width=height=depth)
            rw::IResourceAllocator* mpAllocator; // +0x04 the rw resource allocator
        };

        // X360 0x82403B48 -- build the colour-lookup texture + its texture-state into the effect's rw
        // resources from `lpParameters`, then bind the shared pixel program's "lookupOffset" constant
        // and zero the colour-lookup offset. The constructed Tint lives in the resource's slot 0.
        static Tint* Initialize(const rw::Resource& lrResource, const Parameters& lrParameters);

        // X360 0x823FE7F0 -- compile the shared tint pixel program once (into the file-scope program
        // slot the effects sample) and return it.
        static renderengine::ProgramBufferData* InitializePixelProgram(rw::IResourceAllocator* lpAllocator);

        // X360 0x823F8310 -- lock the colour-lookup (tint-map) texture surface, seed the blend-job
        // parameter block's destination geometry + pixel pointer, and return it by reference for the
        // caller (BrnPostFx::BeginTintBlend) to fill the source list and schedule the blend.
        TintBlendParameters& BeginBlendJob();

        // DWARF rwgpfxtint.h:39 -- close the blend: unlock the colour-lookup surface the job wrote.
        // INLINED on the X360 (BrnPostFx::Render 0x8240A4DC-0x8240A4F4); body belongs to this TU.
        void EndBlendJob();

        // DWARF rwgpfxtint.h:27 -- release the colour-lookup texture and free the two rw resources
        // it and its texture-state were carved into. INLINED on the X360 (BrnPostFx::Destruct
        // 0x82408150-0x82408188); body belongs to this TU.
        void Release();

        // rwgpfxtint.h:110 (DWARF) -- the colour-lookup sampler state (X360 +0xC4), which the post-fx
        // composite binds as its 3D-tint sampler. Defined inline: the X360 read is a single `lwz` and
        // an out-of-line body would add a link dependency for a member read.
        const renderengine::TextureState* GetTextureState() const { return m_textureStateTintMap; }

    private:
        rw::graphics::postfx::TintBlendParameters m_blendParameters;        // +0x00
        bool                                m_blendLock;                    // +0x80
        f32                                 m_colourLookupOffset[4];        // +0x90 Vector4
        renderengine::ProgramVariableHandle m_colourLookupOffsetHandle;     // +0xA0
        renderengine::Texture::Locked       m_textureLock;                  // +0xA4
        renderengine::Texture*              m_textureTintMap;               // +0xC0
        renderengine::TextureState*         m_textureStateTintMap;          // +0xC4
        rw::Resource                        m_textureTintMapResource;       // +0xC8
        rw::Resource                        m_textureStateTintMapResource;  // +0xDC
        rw::IResourceAllocator*             m_allocator;                    // +0xF0
    };
}
}
}

#endif // RW_GPFX_TINT_H
