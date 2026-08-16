#ifndef CGS_CUSTOM_RENDERER_H
#define CGS_CUSTOM_RENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                          // typedef u64 CgsID (GetID return type)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"  // CgsGui::GuiEventQueueSmall (the manager's mEventQueue)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::CustomRenderComponentInterface::Construct        @ 0x828476B0
//   CgsGui::CustomRenderComponentInterface::GetRenderOutput   @ 0x828476C0
//   CgsGui::CustomRenderComponentInterface::Render            @ 0x82857748
//   BrnGui::MainMapRenderer::SetRenderEnabled                 @ 0x82C290D8
//
// CgsCustomRenderer.h homes the GUI custom-renderer component base interface: the
// polymorphic contract BrnGui::CustomRendererManager drives all ten of its render
// components through (NetworkPlayerImage, SatNav, MainMap, CrashNavIcon, BoostBar,
// AboveCar, ProgressBar, BlackBar, InGameMessage, CreditsText).
//
// ⚠️ 2026-08-16 -- THE METHOD LIST BELOW IS THE **DWARF** LIST, NOT A GUESS.
// The previous shape of this class was an "additive grow" invented from the manager's
// call sites, and it named four slots WRONG:
//     invented                             DWARF (references/DecFIGS/dwarfdump/
//                                            GameShared/.../CgsCustomRenderer.h)
//     GetComponentTexture(...)          -> GetRenderOutput(int32_t, int32_t*, ImRendererSet*)
//     GetComponentID()                  -> GetID() const               [returns CgsID, not u32]
//     GetNumTexturesForComponent()      -> GetNumTextures() const
//     Prepare(void*, void*, void*)      -> Prepare(GuiEventQueueSmall*,
//                                                  rw::IResourceAllocator*,
//                                                  rw::IResourceAllocator*)
// Those four names are the MANAGER's (CgsGui::CustomRendererManager) names, not the
// component's. Every concrete renderer in the tree was reconstructed against the DWARF
// names, so under the invented base NONE of their overrides bound: they were pure
// hollow shells -- `GetID()` and `GetRenderOutput()` on a live component both fell
// through to the do-nothing base and returned 0. That -- not "ten missing vtables" --
// was the real reason the custom-renderer layer could not be mounted.
//
// VTABLE ORDER is the DWARF declaration order, cross-checked against the byte offsets
// the manager asm dispatches at (BrnGui::CustomRendererManager, BrnCustomRenderer.cpp):
//   +0x00 Construct        +0x04 Prepare        +0x08 Release      +0x0C Destruct
//   +0x10 GetRenderOutput  [GetComponentTexture @0x824452B0: (*(**v11+16))(comp,a3,a4,a5)]
//   +0x14 RecvEvent        +0x18 Update         +0x1C SetRenderEnabled
//   +0x20 GetRenderLayer   [Render @0x82450848: (*(**v6+32))(*v6) == layer]
//   +0x24 GetID            [GetComponentTexture: (*(**v11+36))(*v11) != id]
//   +0x28 GetNumTextures   +0x2C StartFade      +0x30 ClearFadeState
//   +0x34 RenderComponent  [base Render @0x82857748 tail: (*(*a1+52))(a1,a2)]
// Render() and GetRenderEnabled() are NON-virtual (DWARF), and mbRenderEnabled sits at
// +0x04 (h:182) -- the flag GetComponentRenderable @0x82445468 reads with `lbz r3,4(r11)`.
// The host gate is 64-bit so the byte offsets are not load-bearing; the ORDER and the
// SIGNATURES are, because that is what makes the concrete overrides bind.
//
// FLAG (minimal slice, unchanged): the DWARF class carries only _vptr + mbRenderEnabled
// as data, so the layout here is complete. The out-of-scope dependency is the base
// Render() body's Im2d state setup (it drives ImRendererSet::mpIm2dRenderBuffer through
// four uncommitted state setters at 0x824587B0/0x82458EC0/0x82458CD0/0x82458DC8/
// 0x82458898); that is documented in the body rather than invented.

namespace rw           { struct IResourceAllocator; }   // struct: matches rwcore_structs.h's class-key
namespace renderengine { class  Texture; }
namespace CgsModule    { struct Event; }
namespace CgsGraphics  { struct TextRenderer; }         // struct: matches CgsAptRenderHandler.h
namespace CgsLanguage  { class  LanguageManager; }

namespace CgsGui
{
    // DWARF CgsCustomRenderer.h:55 -- the active renderer set passed to the render/
    // texture-fetch slots. Declared `struct` to match the DWARF class-key (a `class`
    // spelling mangles to a DIFFERENT MSVC symbol, so the key must be consistent
    // tree-wide or the override silently fails to bind).
    struct ImRendererSet;

    // DWARF CgsCustomRenderer.h:95
    enum eCustomRenderLayer
    {
        E_CUSTOMRENDERLAYER_1 = 1,
        E_CUSTOMRENDERLAYER_2 = 2,

        E_CUSTOMRENDERLAYER_COUNT = 3
    };

    // DWARF CgsCustomRenderer.h:105.
    //   _vptr          [+0x00]
    //   mbRenderEnabled[+0x04]  (h:182)
    class CustomRenderComponentInterface
    {
    public:
        virtual ~CustomRenderComponentInterface() {}

        // cpp:73 -- @0x828476B0: `*(result + 4) = 0;` i.e. mbRenderEnabled = false.
        virtual void Construct() { mbRenderEnabled = false; }

        // h:116 -- the staged bring-up every component implements. The manager passes
        // its own output event queue plus the two resource allocators
        // (BrnGui::CustomRendererManager::Prepare @0x82444140).
        virtual bool Prepare(GuiEventQueueSmall* lpEventQueue,
                             rw::IResourceAllocator* lpHeapAllocator,
                             rw::IResourceAllocator* lpTextureAllocator)
        {
            (void)lpEventQueue; (void)lpHeapAllocator; (void)lpTextureAllocator;
            return true;
        }

        // h:119
        virtual bool Release() { return true; }

        // cpp:91
        virtual void Destruct() {}

        // cpp:112 -- @0x828476C0. The base implementation asserts the out-pointer,
        // zeroes it, then asserts outright: a component that does not render to a
        // texture must never be asked for one. Reproduced faithfully (both asserts),
        // returning null.
        virtual renderengine::Texture* GetRenderOutput(s32 liTextureIndex,
                                                       s32* lpiShaderProgram,
                                                       ImRendererSet* lpRendererSet);

        // h:135
        virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
        {
            (void)lpEvent; (void)liEventType;
        }

        // h:139
        virtual void Update() {}

        // cpp:135 -- @0x82857748. NON-virtual (DWARF). Installs the shared Im2d render
        // state on the set's 2D buffer, then tail-calls the virtual RenderComponent.
        void Render(ImRendererSet* lpRendererSet);

        // h:307
        virtual void SetRenderEnabled(bool lbRenderEnabled) { mbRenderEnabled = lbRenderEnabled; }

        // h:324 -- NON-virtual (DWARF).
        bool GetRenderEnabled() const { return mbRenderEnabled; }

        // h:341 -- the DWARF returns the first layer unconditionally.
        virtual eCustomRenderLayer GetRenderLayer() const { return E_CUSTOMRENDERLAYER_1; }

        // h:159 -- the component's CgsID. Concrete renderers return a CgsIDCompress()
        // constant (e.g. NetworkPlayerImageRenderer: CgsIDCompress("PlayerImage")).
        virtual CgsID GetID() const { return 0; }

        // cpp:176
        virtual s32 GetNumTextures() const { return 0; }

        // cpp:195 / cpp:211 -- the fade pair the manager's event-213 map toggle drives
        // (BrnGui::CustomRendererManager::RecvEvent case 213 calls component vtable
        // +0x2C then +0x30 on the MainMap slot).
        virtual void StartFade(bool lbFadeIn, f32 lfDuration) { (void)lbFadeIn; (void)lfDuration; }
        virtual void ClearFadeState() {}

    protected:
        // h:180 -- the per-component draw the non-virtual Render() dispatches to.
        virtual void RenderComponent(ImRendererSet* lpRendererSet) { (void)lpRendererSet; }

        bool mbRenderEnabled;   // [+0x04] h:182
    };

    // ---- DWARF CgsCustomRenderer.h:195 -- the shared custom-renderer MANAGER base ------
    // Bodies are the X360's, and they pin the layout exactly:
    //   Construct @0x828577C0 : *(a1+4)=0; *(a1+8)=0; VariableEventQueue<4096,16>::Construct(a1+12)
    //   Prepare   @0x82847748 : *(a1+4)=a2; *(a1+8)=a3; return 1
    //   Destruct  @0x828577D8 : VariableEventQueue<4096,16>::Destruct(a1+12)
    // -> vptr[+0], mpHeapAllocator[+4], mpTextureAllocator[+8], mEventQueue[+12].
    // GuiEventQueueSmall IS VariableEventQueue<4096,16>, so DWARF and asm agree.
    //
    // ⚠️ THIS CLASS LIVED IN GameSource/Gui/BrnCustomRendererManager.h, FLAG'd "no
    // committed home exists (grep GameShared finds none)". It has a home -- this file, per
    // the DWARF -- and putting it here is what lets GameShared's CgsGuiViewModule reach the
    // real type instead of reinterpret_cast-ing the manager to a locally-invented
    // "CustomRendererManagerWiring" interface with three guessed signatures.
    //
    // VTABLE ORDER (DWARF declaration order), cross-checked against the two call sites
    // that dispatch it blind:
    //   +0x00 Construct  +0x04 Prepare  +0x08 Release  +0x0C Destruct  +0x10 RecvEvent
    //   +0x14 Update     +0x18 Render
    //   +0x1C GetComponentTexture   [AptCallbackCustom::ControlRender @0x8285BFA0: +28]
    //   +0x20 SetComponentRenderable[BrnGui::...::Prepare @0x82444140: (*(*a1+32))(a1,0,1)]
    //   +0x24 GetComponentRenderable  +0x28 GetComponentID  +0x2C GetNumComponents
    //   +0x30 GetNumTexturesForComponent
    //   +0x34 SetAllRenderingState  [Prepare: (*(*a1+52))(a1,0)]
    //   +0x38 SetTextRenderer       [ViewModule::SetCustomRendererManager: (*(*a2+56))]
    //   +0x3C SetLanguageManager    [                    ditto:            (*(**v7+60))]
    //   +0x40 SetReplaySerialiser   [                    ditto:            (*(**v7+64))]
    class CustomRendererManager
    {
    public:
        virtual ~CustomRendererManager() {}

        // cpp:228
        virtual void Construct()
        {
            mpHeapAllocator    = 0;
            mpTextureAllocator = 0;
            mEventQueue.Construct();
        }

        // cpp:250
        virtual bool Prepare(rw::IResourceAllocator* lpHeapAllocator,
                             rw::IResourceAllocator* lpTextureAllocator)
        {
            mpHeapAllocator    = lpHeapAllocator;
            mpTextureAllocator = lpTextureAllocator;
            return true;
        }

        // cpp:269 / cpp:289
        virtual bool Release()  { return true; }
        virtual void Destruct() { mEventQueue.Destruct(); }

        // h:218 / h:222 / h:227
        virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
        {
            (void)lpEvent; (void)liEventType;
        }
        virtual void Update() {}
        virtual void Render(ImRendererSet* lpRendererSet, eCustomRenderLayer leLayer)
        {
            (void)lpRendererSet; (void)leLayer;
        }

        // h:356 -- NON-virtual (DWARF). The queue the components publish GUI events into,
        // and the queue the manager hands each component as Prepare's first argument.
        GuiEventQueueSmall* GetOutputEventQueue() { return &mEventQueue; }

        // h:244 -- ⭐ the Apt custom-control entry point.
        virtual renderengine::Texture* GetComponentTexture(CgsID lComponentID,
                                                           s32 liTextureIndex,
                                                           s32* lpiShaderProgram,
                                                           ImRendererSet* lpRendererSet)
        {
            (void)lComponentID; (void)liTextureIndex; (void)lpRendererSet;
            if (lpiShaderProgram) *lpiShaderProgram = 0;
            return 0;
        }

        // h:250 / h:255 / h:259 / h:262 / h:266 / h:272
        virtual void  SetComponentRenderable(s32 liComponent, bool lbRenderable)
        {
            (void)liComponent; (void)lbRenderable;
        }
        virtual bool  GetComponentRenderable(s32 liComponent)  { (void)liComponent; return false; }
        virtual CgsID GetComponentID(s32 liComponent) const    { (void)liComponent; return 0; }
        virtual s32   GetNumComponents() const                 { return 0; }
        virtual s32   GetNumTexturesForComponent(s32 liComponent) const
        {
            (void)liComponent; return 0;
        }
        virtual void  SetAllRenderingState(bool lbRenderable)  { (void)lbRenderable; }

        // cpp:306 / cpp:322
        virtual void SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer)
        {
            (void)lpTextRenderer;
        }
        virtual void SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager)
        {
            (void)lpLanguageManager;
        }

        // FLAG (asm-attested, not in this DWARF dump): the +0x40 slot.
        // CgsGui::ViewModule::SetCustomRendererManager @0x824EBBF8 dispatches
        // `(*(**v7 + 64))(*v7, a4)` immediately after SetLanguageManager, through a BASE
        // pointer -- so a base-visible slot exists there even though the DecFIGS method
        // list for this class stops at SetLanguageManager. The concrete implementation is
        // BrnGui::CustomRendererManager::SetReplaySerialiser @0x82445648, and the argument
        // GuiModule::Prepare @0x82518D68 passes is its replay-serialiser object
        // (guiModule+1629284, the same object it registers as GuiReplayRegisterSerialiser).
        virtual CustomRendererManager* SetReplaySerialiser(void* lpReplaySerialiser)
        {
            (void)lpReplaySerialiser; return this;
        }

    protected:
        rw::IResourceAllocator* mpHeapAllocator;      // h:285  [+0x04]
        rw::IResourceAllocator* mpTextureAllocator;   // h:286  [+0x08]
        GuiEventQueueSmall      mEventQueue;          // h:288  [+0x0C]
    };
}

namespace BrnGui
{
    // ⛔ BrnGui::MainMapRenderer is NOT declared here any more.
    //
    // It used to be declared TWICE with incompatible layouts -- as a
    // CustomRenderComponentInterface subclass in this header, and as a standalone
    // { void* mpVtable; u32 maZeroGroups[6][5]; ParticleSystem2d maParticleSystems[4]; }
    // in GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.h. That is a
    // textbook ODR fork: it links silently, and whichever definition the linker keeps
    // decides what every call site actually touches. The ledger function
    // (MainMapRenderer::SetRenderEnabled @0x82C290D8) keeps its home in the sibling
    // CgsCustomRenderer.cpp, which owns the one local declaration it needs.
    // Its real home header is BrnMainMapRenderer.h.
}

#endif // CGS_CUSTOM_RENDERER_H
