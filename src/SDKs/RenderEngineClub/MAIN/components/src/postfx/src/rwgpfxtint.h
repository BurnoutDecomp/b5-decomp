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

    private:
        // Opaque, not touched by this TU (recovered only as much as the layout needs).
        struct TintBlendParameters { u8 mauOpaque[0x80]; };   // X360 +0x00 (size inferred: m_blendLock @ +0x80)
        struct TextureLocked       { u8 mauOpaque[0x1C]; };   // X360 +0xA4 (renderengine::Texture::Locked)

        TintBlendParameters                 m_blendParameters;              // +0x00
        bool                                m_blendLock;                    // +0x80
        f32                                 m_colourLookupOffset[4];        // +0x90 Vector4
        renderengine::ProgramVariableHandle m_colourLookupOffsetHandle;     // +0xA0
        TextureLocked                       m_textureLock;                  // +0xA4
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
