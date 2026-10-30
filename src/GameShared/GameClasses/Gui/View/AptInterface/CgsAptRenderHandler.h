#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"      // CgsGraphics::Im2dTransform (mVertexTransform)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // CgsGraphics::ImRenderBuffer<V> (the +4 command buffer)
#include "GameShared/GameClasses/Containers/CgsArray.h"
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
    enum PrimitiveType ;
}

namespace CgsGraphics
{
    template <typename V> struct ImRenderBuffer;
    struct Basic2dColouredTexturedVertex;
    struct TextRenderer;   // DrawString hands the glyph batcher a TextObject (CgsFontRenderer.h)
}

namespace CgsLanguage    { class LanguageManager; }       // held by Construct (mpLanguageManager)
namespace CgsGui         { struct FontCollection; struct ImRendererSet; class CustomRendererManager; }   // text-layout fonts + the custom-renderer wiring mirror


namespace CgsGui
{
    // APT and FLAPT share the actual polymorphic Im2dRenderBuffer. The console
    // +4 adjustment is its base-class adjustment, not a separate head-word wrapper.
    typedef CgsGraphics::Im2dRenderBuffer AptIm2dRenderBuffer;

    class AptRenderHandler
    {
        // The original AptAux debug operations update the existing text-layout
        // inputs consumed by AllocateString; no separate debug state is stored.
        friend class AptAux;
    public:
        // X360 0x827DFBD8. Brings the two per-shape texture-state caches up empty, clears the
        // batch transform, marks every AptString slot free, and seeds the renderer/white-texture
        // slots. (The X360 ctor's 0x7FFFFFFF seeding of the 12-byte cache bins is the
        // BaseLinkedList "uninitialised" sentinel each HashTable bin starts with; here the
        // HashTable default ctor + Init() produce that same start-empty state.)
        AptRenderHandler();

        // PS3 0x5C472C. The parametrised bring-up AptAux::Construct delegates the render-state
        // seeding to (it is called on the AptAux's embedded handler at a1+0x420). This is where
        // the wave-3 slice's deferred field seeds land:
        //   * mpImRenderers   = lpImRenderers   (guest _R27[27147], +108588) -- the renderer set
        //   * mpTextRenderer  = lpTextRenderer  (guest _R27[27148], +0x1A830) -- the glyph batcher
        //   * mpLanguageManager = lpLanguageManager (guest _R27[24944], +99776)
        //   * mpFontCollection = lpFonts        (guest *_R27, +0x00) -- the text-layout fonts
        //   * mAptResolution  : the stage HEIGHT/WIDTH lanes derived from the 1280x720 base and the
        //                       display aspect ratio (the guest's SIMD aspect-fold of the constant
        //                       0x44A0000044340000 == {1280.0, 720.0}); the two read lanes (.x/.y)
        //                       are seeded so GetStageHeight/GetStageWidth return the right values.
        //   * mfFontSizeScale = 1.0  / miTextEffect = 0     (guest stores 1.0 @ +0x1A828, 0 @ +0x1A824)
        //   * the 256-slot AptString pool is constructed against the alt-colour table
        //     (lpAlternateTextColours / liNumAlternateColours); every slot starts FREE.
        //   * the two per-shape texture-state caches start empty.
        //   * mpWhiteTexture is seeded from the Im-renderer state library (guest _R27[32]); that
        //     state-library global is not modelled in this slice, so the white texture is supplied
        //     separately by SetWhiteTexture during boot (FLAG'd below).
        void Construct(ImRendererSet* lpImRenderers,
                       CgsGraphics::TextRenderer* lpTextRenderer,
                       CgsLanguage::LanguageManager* lpLanguageManager,
                       const FontCollection* lpFonts,
                       f32 lfAspectRatio,
                       const ::RGBA* lpAlternateTextColours,
                       s32 liNumAlternateColours);

        // X360 0x828466E0. Cache the white fallback texture pointer (guest +0x80 == +128).
        // Asserts the incoming pointer is non-null ("Invalid texture pointer sent to
        // AptRenderHandler::SetWhiteTexture").
        void SetWhiteTexture(renderengine::Texture* lpWhiteTexture);

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

        // ---- ZID <-> pool-slot bridge (x64 render-data handle scheme) -----------------------
        // On the console the Apt dynamic-text render-data handle (AptRenderItemDynamicText::mZID)
        // is the 32-bit CgsAptString* the string-allocate callback returns. On x64 the pool slot
        // is a 64-bit pointer that does NOT fit the engine's int32 mZID, so the string-allocate
        // callback hands back a SLOT INDEX + 1 (so 0 stays the "unresolved" value the engine
        // tests) and the text draw/release hooks map that id back to the pool slot. These two
        // helpers are the round-trip; the pool array is contiguous so the id is just the index.
        int           AptStringToZID(const CgsAptString* lpAptString) const;   // slot -> index+1 (0 on miss)
        CgsAptString* ZIDToAptString(int liZID);                              // index+1 -> slot (null on OOB)

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
        renderengine::Texture* GetWhiteTexture() const { return mpWhiteTexture; }

        // The custom-renderer manager mirror slot (guest RenderHandler+0xB4 ==
        // ViewModule+58596): CgsGui::ViewModule::Construct @0x828605A0 zeroes it and
        // ViewModule::SetCustomRendererManager @0x824EBBF8 mirrors the module's manager
        // pointer into it (the render callbacks' side of the custom-renderer wiring).
        // The guest stores the field directly from the ViewModule bodies; the inline
        // setter is the access path (the layout stays private).
        void SetCustomRendererManager(CustomRendererManager* lpManager)
        { mpCustomRendererManager = lpManager; }

        // ⭐ THE READER. Until 2026-08-16 this member had a setter and ZERO readers -- the
        // exact dead shape of the mpImpulsePasser bug, inverted. Its one console reader is
        // CgsGui::AptCallbackCustom::ControlRender @0x8285BFA0, the host body behind
        // gAptFuncs.pfnCustomControlRender: it asserts the slot ("Rendering a custom
        // component when no custom render manager set up", CgsAptAux.cpp:1002) and then
        // dispatches the manager's GetComponentTexture at vtable +0x1C:
        //   `(*(**(v12 + 45) + 28))(*(v12 + 45), CgsIDCompress(szType), index, &shader,
        //                           *(v12 + 27147))`
        // -- dword index 45 == byte +0xB4 == this member, dword 27147 == +108588 ==
        // mpImRenderers. Exposed by name so that callback can reach the same state.
        CustomRendererManager* GetCustomRendererManager() const
        { return mpCustomRendererManager; }

        // The renderer-set pointer ControlRender forwards to GetComponentTexture as its
        // last argument (the guest passes the raw word at +108588 straight through).
        ImRendererSet* GetImRendererSet() const
        { return mpImRenderers; }

        // The active 2D command buffer (the ImRenderBuffer<V> base subobject). The
        // mask-push/pop callbacks (DrawRenderingUnit) append raw mask commands through it.
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* GetCommandBuffer()
        {
            return GetIm2dRendererType();
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

        // The language manager Construct held (guest RenderHandler +99776). CgsAptString::Prepare
        // resolves a $LANGID / ~ key through it (guest reads *(v14+99776) off &mRenderHandler and
        // calls LanguageManager::FindString). Returned as the typed pointer the call site needs;
        // the member stays an opaque word (only the text path dereferences it).
        CgsLanguage::LanguageManager* GetLanguageManager() const
        {
            return mpLanguageManager;
        }

    public:
        typedef CgsContainers::HashTable<u32, renderengine::TextureState*, 25> TextureStateTable;
        typedef CgsContainers::HashTableElement<u32, renderengine::TextureState*> TextureStateElement;

    private:
        // CgsAptRender.h:240..298 (DecFIGS), attested by ARTIST accesses
        // and the constructor at 827DFBD8. Native PPC pointers are four bytes.
        const FontCollection* mpFontCollection;
        rw::RGBA mBackgroundColour;
        CgsGraphics::Im2dTransform mVertexTransform;
        rw::math::vpu::Vector2 mAptResolution;
        rw::math::vpu::Vector4 mPlatformTransform;
        rw::math::vpu::Vector4 mOriginTransform;
        renderengine::Texture* mpWhiteTexture;
        renderengine::TextureState* mpWrapTextureState;
        rw::Resource mWrapTextureStateResource;
        renderengine::TextureState* mpClampTextureState;
        rw::Resource mClampTextureStateResource;
        CustomRendererManager* mpCustomRendererManager;
        static const u32 KU_NUM_APT_STRINGS = 256;
        static const u32 KU_APT_STRING_CHARS = 256;
        CgsAptString maAptStrings[KU_NUM_APT_STRINGS];
        u8 maacAptStringChars[KU_NUM_APT_STRINGS][KU_APT_STRING_CHARS];
        u8 mabUnusedAptStrings[KU_NUM_APT_STRINGS];
        CgsLanguage::LanguageManager* mpLanguageManager;
        TextureStateTable mWrapTextureStateTable;
        Array<TextureStateElement, 256> maWrapTextureStateTableElements;
        TextureStateTable mClampTextureStateTable;
        Array<TextureStateElement, 256> maClampTextureStateTableElements;
        s32 miTextEffect;
        f32 mfFontSizeScale;
        ImRendererSet* mpImRenderers;
        CgsGraphics::TextRenderer* mpTextRenderer;
    };
}
