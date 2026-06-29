#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"      // CgsGraphics::Im2dTransform (mVertexTransform)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // CgsGraphics::ImRenderBuffer<V> (the +4 command buffer)
#include "GameShared/GameClasses/Containers/CgsHashTable.h"                      // CgsContainers::HashTable<u32,TextureState*,25>
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"    // CgsResource::GuiGeometryFile (Render arg)
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"                             // CgsUnicode::CgsUtf8 (GetUnusedAptString out-param)
#include "rw/rwcore_structs.h"                                                   // rw::RGBA (mBackgroundColour)

// CgsGui::AptRenderHandler - the GUI's Apt (Adobe-Flash-player) rendering bridge: it owns the
// Im2d renderer set, a white fallback texture, the per-batch vertex/colour transform, two
// per-shape texture-state caches (one CLAMP-addressed, one WRAP-addressed), and a fixed pool of
// reusable AptString text units the Apt callback renderer hands out while drawing a movie.
//
// Recovered from the PS3 External ELF (primary) cross-checked vs BURNOUT_X360_ARTIST.XEX:
//   AptRenderHandler()      X360 0x827DFBD8  (constructor; EXECUTED in the boot trace)
//   SetWhiteTexture(tex)    X360 0x828466E0  (EXECUTED in the boot trace)
//   GetIm2dRendererType()   X360 0x82846780  (debug-asserted Im2d renderer-type accessor)
//   Render(geom, shader)    PS3  0x5CB230    (the shape-geometry -> 2D command/vertex submit)
//   GetUnusedAptString()    PS3  0x5BAC6C    (hand out a free AptString from the pool)
//   DestroyAptString(str)   PS3  0x5BACF8    (return an AptString to the pool)
//
// LAYOUT NOTE: the real guest object is very large (~108 KB). The fields modelled here are the
// ones the in-scope methods touch, declared BY NAME in the order the guest stores them; the
// console byte-offsets the PS3/X360 code addresses (this+99780 cache A, this+104180 cache B,
// this+128 white texture, this+108588 renderer set, the +99520 AptString pool base) are recorded
// in [c:0xNN]/[guest +N] comments. The gate compiles for a 64-bit host, so pointers widen
// 4->8 bytes and those exact offsets are NOT load-bearing; the members reproduce the same
// observable state. FLAG: partial slice -- every DWARF member the in-scope methods do not touch
// is intentionally omitted.

namespace renderengine
{
    class Texture;        // the bound raster (white fallback + the per-shape sampled pages)
    class TextureState;   // the resolved sampler+raster state the caches memoise
    enum PrimitiveType : s32;
}

namespace CgsGraphics
{
    template <typename V> struct ImRenderBuffer;
    struct Basic2dColouredTexturedVertex;
    struct TextRenderer;   // DrawString hands the glyph batcher a TextObject (CgsFontRenderer.h)
}


namespace CgsGui
{
    // One pooled text unit. The guest array strides 0x80 (128 bytes) per element (the
    // DestroyAptString slot search uses `i << 7`); only the pool bookkeeping is in scope, so the
    // unit body is modelled as opaque storage of that stride. FLAG: CgsAptString interior is
    // out-of-scope opaque state (homed when the text-draw path is recovered).
    struct CgsAptString
    {
        u8 mau8Opaque[128];   // 0x80-byte stride (DestroyAptString `i << 7`)
    };

    // The active 2D renderer the Apt rasteriser drives. On the X360 this is an Im2dRenderBuffer:
    // its leading word is a head slot (the value GetIm2dRendererType returns the ADDRESS of), and
    // the actual command-buffer state (CgsGraphics::ImRenderBuffer<V>) lives at +4. Render reads
    // GetIm2dRendererType() (the base pointer) and then `base + 4` (this mCommandBuffer) and issues
    // every command -- SetTransform, SetProgram, SetState, SetTexture, RenderFromStaticVertexBuffer
    // -- through it. (X360 Im2dRenderBuffer::SetTransform @0x8244FF30 takes the base and itself does
    // `+4`; the per-mesh ops in Render take `base + 4` directly. The committed ImRenderBuffer<V>
    // template IS that +4 command-buffer object -- its members line up with 0x20/0x30/0x34/0x41
    // relative to base+4 -- so SetTransform here is called on mCommandBuffer too.)
    struct AptIm2dRenderBuffer
    {
        u32 mu32Head;   // [c:0x00] the renderer base head word (GetIm2dRendererType returns &this)
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex> mCommandBuffer; // [c:0x04]
    };

    class AptRenderHandler
    {
    public:
        // X360 0x827DFBD8. Brings the two per-shape texture-state caches up empty, clears the
        // batch transform, marks every AptString slot free, and seeds the renderer/white-texture
        // slots. (The X360 ctor's 0x7FFFFFFF seeding of the 12-byte cache bins is the
        // BaseLinkedList "uninitialised" sentinel each HashTable bin starts with; here the
        // HashTable default ctor + Init() produce that same start-empty state.)
        AptRenderHandler();

        // X360 0x828466E0. Cache the white fallback texture pointer (guest +0x80 == +128).
        // Asserts the incoming pointer is non-null ("Invalid texture pointer sent to
        // AptRenderHandler::SetWhiteTexture").
        void SetWhiteTexture(void* lpWhiteTexture);

        // X360 0x82846780. Despite the name (the verbatim EA symbol), this does NOT return a type
        // word: it asserts the renderer-set pointer (mpImRenderers, guest +108588) is non-null
        // ("mpImRenderers"), then returns **mpImRenderers -- i.e. the SET's leading slot, which is
        // the active 2D renderer (Im2dRenderBuffer) BASE POINTER. The asm is two loads:
        // `lwz r11, 0(this+108588)` (the set ptr) then `lwz r3, 0(r11)` (the renderer base ptr).
        // Render then uses (base + 4) as the command-buffer sub-object (see AptIm2dRenderBuffer).
        AptIm2dRenderBuffer* GetIm2dRendererType() const;

        // PS3 0x5CB230. Walk a GUI shape geometry's meshes and submit each as a textured 2D
        // primitive batch through the Im2d render buffer: choose the primitive topology, resolve
        // (and memoise) the per-mesh texture state, and draw the mesh's static vertex run. Bounded
        // by the batch transform's colour-scale alpha (a fully-transparent batch is skipped).
        void Render(CgsResource::GuiGeometryFile* lpGeometry, int liShaderIndex);

        // PS3 0x5BAC6C. Hand out the first free AptString from the pool: stamp its preallocated
        // char-buffer pointer through lpcStringPointer, mark the slot in-use, and return it (or
        // null when the pool is exhausted). lpcStringToAllocate is the requested text (carried for
        // signature parity; the pool slot already owns its char storage).
        CgsAptString* GetUnusedAptString(CgsUnicode::CgsUtf8** lpcStringPointer,
                                         const char* lpcStringToAllocate);

        // PS3 0x5BACF8. Return an AptString to the pool: find its slot and mark it free.
        void DestroyAptString(CgsAptString* lpAptString);

        // ---- accessors the AptCallbackRender host callbacks reach state through -----------
        // The Apt player drives the engine -> render-handler bridge (CgsGui::AptCallbackRender,
        // CgsAptCallbackRender.cpp) entirely through CgsGui::AptAuxPointer::mpAptAuxInst->
        // mRenderHandler. SetVertexMatrix / SetColourTransform write the per-batch transform
        // (guest RenderHandler+0x10..+0x4F); SetBackgroundColour writes the clear colour
        // (guest RenderHandler+0x04); GetStageWidth / GetStageHeight read the stage resolution
        // (guest RenderHandler+0x50 == mAptResolution.x height / +0x54 == .y width). Exposed by
        // name so the callbacks touch the same observable state the console code addresses.
        CgsGraphics::Im2dTransform& GetVertexTransform() { return mVertexTransform; }
        rw::RGBA&                   GetBackgroundColour() { return mBackgroundColour; }
        // mAptResolution.mV.body[0] is the stage HEIGHT (lfs 0(+0x470)); [1] is the WIDTH
        // (lfs 4(+0x470)). Named getters mirror GetStageHeight @0x5BE010 / GetStageWidth @0x5BE028.
        f32 GetStageHeight() const { return mAptResolution.x; }
        f32 GetStageWidth()  const { return mAptResolution.y; }

        // The white fallback texture the mask/draw path binds (guest +128). Read-only accessor
        // for the callbacks (the cache + transform members stay private).
        void* GetWhiteTexture() const { return mpWhiteTexture; }

        // The active 2D command buffer (GetIm2dRendererType()->mCommandBuffer == base+4). The
        // mask-push/pop callbacks (DrawRenderingUnit) append raw mask commands through it.
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* GetCommandBuffer()
        {
            return &GetIm2dRendererType()->mCommandBuffer;
        }

        // The 2D renderer BASE pointer the DrawString callback hands to TextRenderer::RenderString
        // (the guest passes the renderer base, *(mpImRenderers+0), as the Im2dRenderBuffer* arg).
        AptIm2dRenderBuffer* GetIm2dRendererBase() { return GetIm2dRendererType(); }

        // The glyph batcher the DrawString callback drives (guest RenderHandler +0x1A830, i.e. the
        // word right after mpImRenderers). AptAux::Construct seeds it from the TextRenderer passed
        // to AptAux::Construct @0x5C4B6C.
        CgsGraphics::TextRenderer* GetTextRenderer() const { return mpTextRenderer; }

        // The text-layout inputs AllocateString @0x5C7260 hands to CgsAptString::Prepare: the font
        // collection (guest RenderHandler +0x00), the text effect (guest +0x1A824), and the size
        // scale (guest +0x1A828). Exposed by name so the AllocateString TU -- which includes the
        // REAL CgsGui::CgsAptString (text object) and so CANNOT include this header's opaque pool
        // CgsAptString -- can read them without pulling this layout in. Returned as raw words.
        const void* GetFontCollection() const { return mpFontCollection; }
        s32         GetTextEffect()     const { return miTextEffect; }
        f32         GetFontSizeScale()  const { return mfFontSizeScale; }

    public:
        // One per-shape texture-state cache: a chained HashTable<textureId, TextureState*, 25>
        // (the 25 bins) backed by an EXTERNAL fixed node pool. The guest lays the 25-bin table
        // (300 bytes) immediately ahead of the node pool, with the live node count 0x1000 bytes
        // past the pool base (== 256 nodes * 16 bytes) -- so the pool holds 256 entries. Render's
        // miss path bump-allocates the next free node, stamps {key,value}, and sorted-inserts it
        // into its bin (key % 25). Sources: AptRenderHandler::Render 0x5CB230 (the inlined
        // GetInternal + ordered insert), HashTable<...,25>::GetInternal 0x5CEF84.
        struct TextureStateCache
        {
            typedef CgsContainers::HashTable<u32, renderengine::TextureState*, 25> Table;
            typedef Table::Node                                                    Node;

            static const u32 KU_POOL_SIZE = 256;   // 0x1000 / sizeof(Node) (16-byte nodes)

            Table mBins;                  // [guest +0] the 25 chained bins (300 bytes)
            Node  maNodePool[KU_POOL_SIZE];// [guest +300] the external node pool (256 * 16 bytes)
            u32   muNodeCount;            // [guest +300 + 0x1000] live node count (next-free index)

            void Init() { mBins.Init(); muNodeCount = 0; }

            // The ordered insert Render inlines: take the next pool node, stamp the pair, and
            // splice it into its bin keeping the bin sorted ascending by key. Mirrors the guest's
            // InternalGetHead/InternalGetTail compare-and-(AddHead/AddTail/AddBefore) chain.
            renderengine::TextureState* InsertSorted(u32 luKey, renderengine::TextureState* lpState);

            // Look the key up; returns the memoised state or null on a miss (HashTable::Get).
            renderengine::TextureState* Find(u32 luKey)
            {
                renderengine::TextureState** lppState = mBins.Get(luKey);
                return lppState ? *lppState : nullptr;
            }
        };

    private:
        // ---- white fallback texture (guest +0x80 == +128; SetWhiteTexture / mesh mode 0) ----
        void* mpWhiteTexture;

        // ---- the per-batch screen-space + colour transform (mColourScale.w gates the batch) --
        CgsGraphics::Im2dTransform mVertexTransform;

        // ---- the Apt stage clear colour (guest RenderHandler+0x04 == AptAux+0x424) -----------
        // SetBackgroundColour @0x5C0E2C stores the rotated (ARGB->RGBA) clear colour here. Modelled
        // as rw::RGBA (the guest stores a single 32-bit colour word). [guest +0x04]
        rw::RGBA mBackgroundColour;

        // ---- the Apt stage resolution (guest RenderHandler+0x50 == AptAux+0x470) --------------
        // GetStageHeight @0x5BE010 reads mV.body[0] (lfs 0); GetStageWidth @0x5BE028 reads
        // mV.body[1] (lfs 4). Modelled as a Vector4 (the guest reads two of its lanes). The set-up
        // of these lanes is owned by AptAux::Construct (the resolution comes from the display
        // mode); only the two read lanes are in scope. [guest +0x50]
        rw::math::vpu::Vector4 mAptResolution;

        // ---- the two per-shape texture-state caches ------------------------------------------
        TextureStateCache mWrapTextureCache;   // [guest +99780]  mesh texture-mode 2 (WRAP address)
        TextureStateCache mClampTextureCache;  // [guest +104180] mesh texture-mode 1 (CLAMP address)

        // ---- the AptString pool (guest base +99520) ------------------------------------------
        static const u32 KU_NUM_APT_STRINGS = 256;
        CgsAptString    maAptStrings[KU_NUM_APT_STRINGS];       // [guest +99520+192] 0x80-byte units
        u32             maacAptStringChars[KU_NUM_APT_STRINGS]; // [guest +99520+...] preallocated char ptrs (32-bit)
        u8              mabUnusedAptStrings[KU_NUM_APT_STRINGS];// [guest +99520] free flags (1 == free)

        // ---- Im2d renderer set (guest +108588; read by GetIm2dRendererType / Render) ----------
        // The guest holds a pointer to the active render set. Its leading slot (offset 0) is the 2D
        // renderer (AptIm2dRenderBuffer) BASE POINTER -- GetIm2dRendererType returns it (**this+108588)
        // and Render drives `base + 4` (the embedded command buffer). Offset +16 is the 3D renderer
        // (Render asserts it non-null but this path does not use it). Modelled as the set struct below.
    public:
        struct AptImRendererSet
        {
            AptIm2dRenderBuffer* mpIm2dRenderer;  // [c:0x00] *mpImRenderers == GetIm2dRendererType() (2D renderer base)
            void*                mpReserved04;    // [c:0x04] (set slot; unused by this path)
            void*                mpReserved08;    // [c:0x08]
            void*                mpReserved0C;    // [c:0x0C]
            void*                mp3dRenderer;    // [c:0x10] 3D renderer (Render asserts *(set+16) != 0)
        };
    private:
        AptImRendererSet* mpImRenderers;   // guest +108588 ("mpImRenderers")

        // The glyph batcher (guest +0x1A830, the word right after mpImRenderers). DrawString
        // @0x5C9364 reaches it as `-0x57D0(addis this,2)` == this+0x1A830 and calls
        // TextRenderer::RenderString through it. Seeded by AptAux::Construct @0x5C4B6C.
        CgsGraphics::TextRenderer* mpTextRenderer;

        // The text-layout inputs AllocateString reads (font collection @ guest +0, text effect
        // @ guest +0x1A824, size scale @ guest +0x1A828). Held as opaque/raw words: AllocateString
        // forwards them to CgsAptString::Prepare. Seeded by AptAux::Construct @0x5C4B6C.
        const void* mpFontCollection;   // guest +0x00 (a CgsGui::FontCollection*)
        s32         miTextEffect;       // guest +0x1A824 (CgsAptString::ETextEffects)
        f32         mfFontSizeScale;    // guest +0x1A828
    };
}
