#ifndef BRN_MAIN_MAP_RENDERER_H
#define BRN_MAIN_MAP_RENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                    // Vector2 / Vector4 / Matrix33 / CgsID
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"  // CustomRenderComponentInterface base + ImRendererSet
#include "GameShared/GameClasses/Gui/View/CgsParticleSystem2d.h"               // CgsGui::ParticleSystem2d (the four pulse banks)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"   // CgsGraphics::Im2dRenderBuffer (== Im2d)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> -- the actual draw target
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"     // CgsGraphics::Vector2 / RGBA8
#include "GameSource/Gui/SatNav/BrnMainMap.h"                                  // BrnGui::GuiEventRenderMainMap (mRenderMainMapEvent)
#include "rw/rwcore_structs.h"                                                 // rw::Resource / rw::IResourceAllocator

// BrnGui::MainMapRenderer -- the GUI custom-render component that draws THE MAP WORLD:
// the Paradise City map surface itself, inside the CrashNav / pre-race map chrome.
// Manager slot 2 (E_MAINMAP).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008) against
// references/DecFIGS/dwarfdump/GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.h
// (full DWARF class layout + the nested enums). The functions bodied in
// BrnMainMapRenderer.cpp:
//   MainMapRenderer (ctor)        @0x827DF3E8      Construct                 @0x82446238
//   Prepare                       @0x824462D0      Release                   @0x82446380
//   Destruct                      (vtable +0x0C -> 0x822A9750 -> a bare blr)
//   RecvEvent                     @0x82449E98      Update  (vtable +0x18 -> 0x8284CB38, blr)
//   SetRenderEnabled              @0x82C290D8      GetRenderLayer (vtable +0x20 -> 0x82C296C8)
//   GetID                         @0x824464F8      StartFade                 @0x82446510
//   ClearFadeState                @0x824465D8      UpdateAlphaForFadeState   @0x824465E8
//   ClearBackgroundFadeToMapEdges @0x8245A5F0      RenderComponent           @0x82460130
//
// ⭐ 2026-08-29 -- THE CLASS IS NOW A REAL RENDERER. It used to be a declared
// "minimal-slice" standalone struct with an opaque vptr word, six anonymous `maZeroGroups`
// and four particle systems, because only the compiler-generated constructor had been
// reconstructed. The DWARF names every one of those slots: the six 5-dword zero runs the
// ctor clears at guest +0x48/+0x60/+0x78/+0x90/+0xA8/+0xC0 are the SIX renderengine
// `Resource` descriptors below, and the "gap" dword after each (+0x5C/+0x74/+0x8C/+0xA4/
// +0xBC/+0xD4) is the TextureState*/BlendState* that follows it -- which is exactly the
// set Construct() zeroes and Release() frees. Nothing about the old shape survives.
//
// LAYOUT POLICY (matches BrnSatNavRenderer.h / BrnCrashNavIconRenderer.h): the compile gate
// is a per-TU `cl /c` on a 64-bit host, so guest byte offsets are NOT load-bearing (pointers
// widen 4->8). Members are declared BY NAME from the DWARF; the .cpp applies no raw offset
// cast anywhere. The console's five-dword `Resource` descriptor slots are kept as the
// documented X360 shape (`u32[5]`) and the runtime-created texture states get real
// `rw::Resource` backings alongside, the same PC fold BrnSatNavRenderer.cpp and
// BrnCrashNavIconRenderer.cpp use.
//
// OUT OF SCOPE (declared by the DWARF, NOT reconstructed here -- and unreachable in this
// build): the ROUTE and PULSE layer -- RenderRoute / ClipRoute / IsWorldPosInViewport /
// RenderPulses / RenderPulse / GetRouteLength / PreparePulseParticleSystem /
// ClearBackgroundFlatBlack. RenderComponent @0x82460130 never calls any of them (its whole
// body is the mask, the fade-to-edges background and the active-texture quad loop), and the
// three resources they would draw with (mRouteSegment*/mPulse*) have NO writer anywhere in
// the ARTIST export set, so their pointers are permanently null. They are declared as
// members because Release() tests them; they are not stubbed with plausible bodies.

namespace renderengine
{
    // Uncommitted renderengine state types reached only as opaque handles by this slice.
    class TextureState;
    class BlendState;
}

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;

    class MainMapRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // ---- DWARF nested enums (BrnMainMapRenderer.h:58 / :64 / :114) ----
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 1,
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 1,
        };

        // The fade the manager's event-213 map toggle drives through StartFade /
        // ClearFadeState, and UpdateAlphaForFadeState turns into mu8Alpha.
        enum EFadeState
        {
            E_FADESTATE_NONE = 0,
            E_FADESTATE_IN   = 1,
            E_FADESTATE_OUT  = 2,
            E_FADESTATE_NUM  = 3,
        };

        // ---- class-scope constants (DWARF h:55/56/125) ----
        // KU8_MAX_ALPHA / KU8_INACTIVE_ALPHA are the two literals UpdateAlphaForFadeState
        // and RenderComponent use (`li r11,0xFF` / `li r28,0x80`).
        static const u8  KU8_MAX_ALPHA      = 255;
        static const u8  KU8_INACTIVE_ALPHA = 128;
        // DWARF h:125. The pulse bank count; it is why there are four ParticleSystem2d
        // members and a four-entry mfLastProp. The pulse LAYER itself is out of scope.
        static const s32 KI_MAX_PULSES      = 4;

    public:
        MainMapRenderer();                                      // @0x827DF3E8

        // ---- virtual overrides the GUI CustomRendererManager drives ----
        virtual void   Construct();                             // @0x82446238
        virtual bool   Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                               rw::IResourceAllocator* lpHeapAllocator,
                               rw::IResourceAllocator* lpTextureAllocator);   // @0x824462D0
        virtual bool   Release();                               // @0x82446380
        virtual void   Destruct();                              // vtable +0x0C
        virtual void   RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType); // @0x82449E98
        virtual void   Update();                                // vtable +0x18
        virtual void   SetRenderEnabled(bool lbRenderEnabled);  // @0x82C290D8
        virtual CgsGui::eCustomRenderLayer GetRenderLayer() const;   // vtable +0x20
        virtual CgsID  GetID() const;                           // @0x824464F8
        virtual void   StartFade(bool lbFadeIn, f32 lfDuration);// @0x82446510
        virtual void   ClearFadeState();                        // @0x824465D8

    protected:
        // The per-frame draw the non-virtual base Render() dispatches to. @0x82460130.
        virtual void   RenderComponent(CgsGui::ImRendererSet* lpRendererSet);

    private:
        // @0x824465E8 -- roll mu8Alpha from meFadeState + the cache's clock.
        void UpdateAlphaForFadeState();

        // @0x8245A5F0 -- the map's background: a solid band behind the drawn map columns
        // and two horizontal gradient bars that fade out to the view rect's edges.
        // The console signature (DWARF cpp:626) is
        //   (Im2dRenderBuffer*, Vector4, uint32_t, Matrix33, Matrix44, Matrix44)
        // and the two trailing Matrix44s are DEAD in this build -- @0x8245A5F0 never reads
        // r7/r8, and the caller passes the SAME stack slot for both (the slot the
        // now-dead GetNormalisedToRendererTransform / GetGuiCamera results were written
        // into before MakeCoordSpaceFromRect overwrote them). They are dropped here rather
        // than carried as two ignored arguments; see the note on the body.
        void ClearBackgroundFadeToMapEdges(
                 CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
                 const Vector4& lrv4ViewRect,
                 u32 luTextureCount,
                 const Matrix33& lrm33MapToView);

        // ---- member state (DWARF h:133-213, declared in DWARF order) --------------------
        // NOTE: mbRenderEnabled lives in the base (CgsGui::CustomRenderComponentInterface),
        // which is the `stb r4, 4(r3)` SetRenderEnabled writes.
        EPrepareStage           mePrepareStage;         // h:133  guest +0x08
        EReleaseStage           meReleaseStage;         // h:134  guest +0x0C
        // h:136 -- the latched id-223 payload. The console memcpy's a flat 48 bytes; on this
        // host the record carries a native-width pointer (mpActiveTextures), so a fixed
        // 48-byte copy would TRUNCATE it -- typed assignment instead (the SatNav-212 /
        // PlayAptMovie width precedent). guest +0x10.
        GuiEventRenderMainMap   mRenderMainMapEvent;
        GuiCache*               mpGuiCache;             // h:137  guest +0x40
        rw::IResourceAllocator* mpHeapAllocator;        // h:139  guest +0x44

        // h:142-158 -- the six console `Resource` descriptors and the state pointer that
        // follows each. The ctor zeroes the six descriptors; Construct() zeroes five of the
        // six pointers (mpPreRaceMaskTextureState is the ONE the guest leaves alone -- the
        // ctor's descriptor clear already covered its slot and Construct never re-stores it).
        u32                         mBackgroundMaskTextureStateResource[5];  // h:142 guest +0x48
        renderengine::TextureState* mpBackgroundMaskTextureState;            // h:143 guest +0x5C
        u32                         mPreRaceMaskTextureStateResource[5];     // h:145 guest +0x60
        renderengine::TextureState* mpPreRaceMaskTextureState;               // h:146 guest +0x74
        u32                         mMapTileBlendStateResource[5];           // h:148 guest +0x78
        renderengine::BlendState*   mpMapTileBlendState;                     // h:149 guest +0x8C
        u32                         mRouteSegmentTextureStateResource[5];    // h:151 guest +0x90
        renderengine::TextureState* mpRouteSegmentTextureState;              // h:152 guest +0xA4
        u32                         mRouteSegmentBlendStateResource[5];      // h:154 guest +0xA8
        renderengine::BlendState*   mpRouteSegmentBlendState;                // h:155 guest +0xBC
        u32                         mPulseTextureStateResource[5];           // h:157 guest +0xC0
        renderengine::TextureState* mpPulseTextureState;                     // h:158 guest +0xD4

        EFadeState              meFadeState;            // h:188  guest +0xD8
        f32                     mfFadeStartTime;        // h:189  guest +0xDC
        f32                     mfFadeDuration;         // h:190  guest +0xE0
        u8                      mu8Alpha;               // h:191  guest +0xE4

        CgsGui::ParticleSystem2d mParticleSystem[KI_MAX_PULSES];  // h:210 guest +0xF0 stride 0x21A0
        f32                     mfLastProp[KI_MAX_PULSES];        // h:211 guest +0x8770
        bool                    mbDrawRoute;                      // h:213 guest +0x8780

        // PC fold: backing storage for the two texture states RecvEvent creates at runtime
        // (the font-path / SatNav / CrashNav precedent -- renderengine::TextureState::
        // Initialize(rw::Resource*, Parameters*)). The console's u32[5] descriptor slots
        // above stay as the documented X360 shape. The other four descriptors get no
        // backing: nothing in this build ever creates those states.
        rw::Resource            mBackgroundMaskTextureStateBacking;
        rw::Resource            mPreRaceMaskTextureStateBacking;
    };
}

#endif // BRN_MAIN_MAP_RENDERER_H
