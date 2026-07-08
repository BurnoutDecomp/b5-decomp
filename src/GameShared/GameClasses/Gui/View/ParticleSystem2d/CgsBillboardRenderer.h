#ifndef CGS_BILLBOARD_RENDERER_H
#define CGS_BILLBOARD_RENDERER_H

#include "types.hpp"
#include "rw/rwcore_structs.h"  // rw::Resource / rw::IResourceAllocator / rw::ResourceDescriptor
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // CgsGraphics::Basic2dColouredTexturedVertex

// Reconstructed from BURNOUT_X360_ARTIST.XEX (decfigs-attributed TU
// GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.cpp):
//   CgsGui::BillboardRenderer::Construct  @ 0x82852C38
//   CgsGui::BillboardRenderer::Render     @ 0x828577E0  (const)
//   CgsGui::BillboardRenderer::SetUVTable @ 0x8284F478  (private)
//
// A small screen-space billboard batcher: a GUI custom renderer (the X360
// BrnGui::BoostBarRenderer holds an array of six of these) hands it an array of BillboardInfo
// descriptors; Render expands each into a rotated/scaled textured quad, stitches the quads into
// one triangle strip in its owned vertex buffer, and submits the strip through an immediate-mode
// render buffer. SetUVTable precomputes a per-animation-frame UV atlas table (a framesX x framesY
// grid) the draw indexes by BillboardInfo::miTextureFrame.
//
// LAYOUT POLICY (matches the sibling GUI renderers / CgsGui::ParticleSystem2d): the compile gate
// is a per-TU `cl /c` on a 64-bit host, so guest byte offsets are NOT load-bearing (pointers widen
// 4->8). Members are declared BY NAME in the DWARF/asm-attested order (offsets noted for the
// record). The X360 guest object is ~0x204C bytes (mafUVTab dominates); the x64 sizeof differs and
// is deliberately not pinned.

namespace renderengine { class TextureState; class BlendState; }

// The immediate-mode render buffer the draw submits through is used pointer-only in this header
// (the full template lives in ImRenderBuffer/CgsImRenderBufferTemplate.h, pulled in by the .cpp).
namespace CgsGraphics { template <typename V> struct ImRenderBuffer; }

namespace CgsGui
{
    struct BillboardInfo;  // real home: CustomRenderer/CgsGuiBillboardInfo.h (pointer-only here)

    class BillboardRenderer
    {
    public:
        // The animation playback mode SetUVTable/Render honour when a billboard's requested frame
        // index is at or beyond miNumFrames (DWARF CgsBillboardRenderer.h:111 eAnimMode).
        enum eAnimMode
        {
            E_BLANK   = 0,   // frame >= count -> skip (draw nothing)
            E_REPEAT  = 1,   // wrap: frame % count
            E_CLAMP   = 2,   // hold the last frame (count - 1)
            E_SHUTTLE = 3,   // ping-pong across the frame range
        };

        // DWARF CgsBillboardRenderer.h:69 -- the UV table caps at 256 animation frames.
        static const s32 KI_MAX_ANIMFRAMES = 256;

        // 0x82852C38. Prime the renderer: record the max-billboard capacity + the bound texture/blend
        // states + the resource allocator, default the animation mode to E_REPEAT, allocate the
        // owned vertex buffer (120 bytes == six vertices per billboard) through the allocator, then
        // build the UV table for a framesX x framesY atlas of liNumFrames frames.
        void Construct(rw::IResourceAllocator* lpAllocator, s32 liMaxBillboards,
                       const renderengine::TextureState* lpTextureState,
                       const renderengine::BlendState* lpBlendState,
                       s32 liFramesX, s32 liFramesY, s32 liNumFrames);

        // 0x828577E0. Expand laBillboards[0..liNumBillboards) into rotated/scaled textured quads,
        // stitch them into one triangle strip in maVertexList, and submit the strip through the
        // immediate-mode render buffer. See the .cpp for the DWARF-vs-reduced-parameter note on the
        // render-buffer argument.
        void Render(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
                    const BillboardInfo* laBillboards, s32 liNumBillboards) const;

    private:
        // 0x8284F478. Precompute the per-frame UV atlas: for a framesX x framesY grid, frame i lands
        // at grid cell (i%framesX, i/framesX) and stores its four corner UVs (8 floats) into mafUVTab.
        // A one-frame (or blank) table points mpUVTable at the shared full-texture default instead.
        void SetUVTable(s32 liFramesX, s32 liFramesY, s32 liNumFrames);

        // The shared full-texture UV set (guest &unk_820E08B8) the one-frame/blank table uses: the
        // four corners of the whole texture {0,0, 0,1, 1,0, 1,1} -- exactly what SetUVTable's grid
        // generator emits for a single 1x1 frame (see the .cpp; it is derived from that generator,
        // not a guessed rodata blob).
        static const f32 maUVDefault[8];

        // ---- members (X360 guest word offsets in comments; not load-bearing on the x64 gate) ----
        s32       miNumFrames;                 // +0     valid animation-frame count
        eAnimMode meAnimMode;                  // +4     playback mode (Construct defaults E_REPEAT)
        const f32* mpUVTable;                  // +8     -> mafUVTab, or maUVDefault for the 1-frame case
        f32       mafUVTab[2048];              // +12    KI_MAX_ANIMFRAMES * 8 floats (four corner UVs/frame)
        s32       miMaxBillboards;             // +0x200C
        CgsGraphics::Basic2dColouredTexturedVertex* maVertexList; // +0x2010 owned strip vertex buffer
        s32       miVertexCount;               // +0x2014 (Construct zeroes it; unused by these bodies)
        rw::IResourceAllocator* mpAllocator;   // +0x2018
        rw::Resource mTextureStateResource;    // +0x201C (X360 serialised BaseResources<5>; x64 <4>)
        const renderengine::TextureState* mpTextureState; // +0x2030
        rw::Resource mBlendStateResource;      // +0x2034
        const renderengine::BlendState* mpBlendState;     // +0x2048
    };
}

#endif
