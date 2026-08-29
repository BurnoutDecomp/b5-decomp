#ifndef BRN_CUSTOM_RENDERER_MANAGER_H
#define BRN_CUSTOM_RENDERER_MANAGER_H

#include "types.hpp"

#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h" // CgsGui::CustomRenderComponentInterface, eCustomRenderLayer, ImRendererSet
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                           // CgsGui::GuiEventQueueSmall (the base's mEventQueue)
#include "GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h" // the live slot-0 component
#include "GameSource/Gui/CustomRenderer/Renderers/BrnInGameMessageRenderer.h"      // [tut-ticker] the live slot-8 component
#include "GameSource/Gui/CustomRenderer/Renderers/BrnBoostBarRenderer.h"           // [boost-bar] the live slot-4 component
#include "GameSource/Gui/CustomRenderer/Renderers/BrnSatNavRenderer.h"             // [H3b] the live slot-1 component (the minimap)
#include "GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.h"            // [map-world] the live slot-2 component (THE MAP WORLD)
#include "GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.h"       // [map-world] the live slot-3 component (the icon layer on top of it)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (SetMaskRect)

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BrnGui::CustomRendererManager  @ 0x82444040 .. 0x82450908  (16 functions)
//   ctor                           @ 0x827E20A8
//
// The manager owns the GUI custom-renderer set: ten polymorphic
// CgsGui::CustomRenderComponentInterface components whose lifecycle it drives
// (Construct / Prepare / Release / Destruct / Update), whose events it routes
// (RecvEvent), and through which the Apt player fetches a substituted texture for a
// custom-render movieclip (GetComponentTexture -> the component's GetRenderOutput).
//
// ---- WHY THIS IS ON THE CRITICAL PATH -------------------------------------------
// Two separately-reported defects both dead-end here:
//   * the intro licence card's PLAYER PHOTO. B5LicenseRank0 char[23] places a custom
//     control typed `_type='PlayerImage', _index=1`. The Apt engine hands that to
//     gAptFuncs.pfnCustomControlRender == CgsGui::AptCallbackCustom::ControlRender
//     @0x8285BFA0, which does CgsIDCompress("PlayerImage") and calls THIS class's
//     GetComponentTexture. With no manager the assert "Rendering a custom component
//     when no custom render manager set up" is the console's own name for the bug, and
//     the quad falls back to the movie's authored salmon fill -- the "red icon".
//   * the in-game TUTORIAL TICKER: GUI event 537 routes through RecvEvent to the
//     E_INGAME_MESSAGE component.
//
// ---- HOST OBJECT MODEL -----------------------------------------------------------
// The X360 object is ~128 KB of BY-VALUE renderer subobjects with a parallel array of
// interface pointers at +0x101C that every lifecycle loop dispatches through (the ctor
// @0x827E20A8 constructs SatNav @+0x1050, MainMap @+0x28D0, CrashNavIcon @+0xB060,
// BoostBar @+0xE520 in place and installs vtables at +0x1B660 NetworkPlayerImage,
// +0x1B970 AboveCar, +0x1C0F0 ProgressBar, +0x1C108 BlackBar, +0x1C120 CreditsText,
// +0x1E130 InGameMessage). The same shape is reproduced here -- by-value subobjects
// plus the pointer array -- except that only the components whose reconstruction is
// COMPLETE ENOUGH TO RUN are embedded. See maCustomRenderComponents below.

namespace rw          { struct IResourceAllocator; }
namespace CgsGraphics { struct TextRenderer; }
namespace CgsLanguage { class  LanguageManager; }
namespace BrnFlapt    { struct FlaptRenderer; }
// SetMaskRect (below) touches its collaborators pointer/reference-only.
namespace rw { namespace math { namespace vpu { struct Vector4; } } }
namespace renderengine { class TextureState; }
namespace CgsGraphics
{
    struct Basic2dColouredTexturedVertex;
    template <typename V> struct ImRenderBuffer;
}

// ⭐ The shared base CgsGui::CustomRendererManager USED TO BE DECLARED HERE, FLAG'd
// "no committed home exists". It does have one -- GameShared/GameClasses/Gui/View/
// CustomRenderer/CgsCustomRenderer.h, per the DecFIGS DWARF -- and it now lives there,
// included above. Keeping a GameShared base class in a GameSource header also kept
// CgsGuiViewModule (GameShared) from ever seeing the real type, which is why that TU
// reinterpret_cast the manager to a locally-invented three-method interface.

namespace BrnGui
{

// BrnCustomRenderer.h:88 (DWARF) -- index of the manager's renderer-component array. The
// order matches the construction order in Construct() and the component-pointer slots
// guest +0x101C..+0x1040.
enum ECustomRenderTypes
{
    E_NETWORK_PLAYER_IMAGE = 0,
    E_SATNAV               = 1,
    E_MAINMAP              = 2,
    E_CRASHNAVICONS        = 3,
    E_BOOSTBAR             = 4,
    E_ABOVECAR             = 5,
    E_PROGRESSBAR          = 6,
    E_BLACKBAR             = 7,
    E_INGAME_MESSAGE       = 8,
    E_CREDITS_TEXT         = 9,

    E_CUSTOM_RENDER_TYPES_COUNT = 10
};

class CustomRendererManager : public CgsGui::CustomRendererManager
{
public:
    // BrnCustomRenderer.h:69 / :75 -- prepare/release state machine markers.
    enum EPrepareStage { E_PREPARESTAGE_START = 0, E_PREPARESTAGE_DONE = 1 };
    enum EReleaseStage { E_RELEASESTAGE_START = 0, E_RELEASESTAGE_DONE = 1 };

    // ctor @0x827E20A8 -- installs the component vtables / constructs the by-value
    // subobjects and seeds the array. Modelled as the C++ ctor it was.
    CustomRendererManager();

    // ---- the reconstructed overrides (BrnCustomRenderer.cpp) ----
    virtual void Construct();                                                   // 0x82444040
    virtual bool Prepare(rw::IResourceAllocator* lpHeapAllocator,               // 0x82444140
                         rw::IResourceAllocator* lpTextureAllocator);
    virtual bool Release();                                                     // 0x824442B0
    virtual void Destruct();                                                    // 0x82444378
    virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType);   // 0x824443D0
    virtual void Update();                                                      // 0x82450908
    virtual void Render(CgsGui::ImRendererSet* lpRendererSet,                   // 0x82450848
                        CgsGui::eCustomRenderLayer leLayer);

    virtual renderengine::Texture* GetComponentTexture(CgsID lComponentID,      // 0x824452B0
                                                       s32 liTextureIndex,
                                                       s32* lpiShaderProgram,
                                                       CgsGui::ImRendererSet* lpRendererSet);
    virtual CgsID GetComponentID(s32 liComponent) const;                        // 0x82445378
    virtual void  SetComponentRenderable(s32 liComponent, bool lbRenderable);   // 0x824453F8
    virtual bool  GetComponentRenderable(s32 liComponent);                      // 0x82445468
    virtual s32   GetNumComponents() const { return E_CUSTOM_RENDER_TYPES_COUNT; }
    virtual s32   GetNumTexturesForComponent(s32 liComponent) const;            // 0x82445658
    virtual void  SetAllRenderingState(bool lbRenderable);                      // 0x824454E0
    virtual void  SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer);   // 0x82445538
    virtual void  SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager); // 0x824455A8

    void EndOfFrame();                                                          // 0x82449930

    // Setters that wire shared sub-systems into the renderers. SetFlaptRenderer is a
    // plain member; SetReplaySerialiser is the DERIVED class's extra vtable slot (+0x40)
    // ViewModule::SetCustomRendererManager dispatches after SetLanguageManager.
    CustomRendererManager* SetFlaptRenderer(BrnFlapt::FlaptRenderer* lpFlaptRenderer); // 0x82449920
    virtual CustomRendererManager* SetReplaySerialiser(void* lpReplaySerialiser);      // 0x82445648

private:
    // RecvEvent's case-213 sub-routine (the SatNav/MainMap/CrashNav map-toggle path).
    void RecvEvent_Event213(const CgsModule::Event* lpEvent);

    // ---- the ten render components ---------------------------------------------------
    // ⭐ THE ARRAY IS NOW POPULATED. It previously held ten UNINITIALISED pointers that
    // Construct() never wrote and that RecvEvent / GetComponentID / SetComponentRenderable
    // / Update dereferenced unguarded -- the [[valid-pointer-invalid-object]] shape, except
    // worse: the slots were never even zeroed, so the first GUI event would have jumped
    // through a stack-garbage vptr. Mounting the TU in that state would have been an
    // instant AV.
    //
    // Slot 0 is a REAL, by-value component, exactly as the console embeds it: the
    // NetworkPlayerImageRenderer is fully reconstructed (Construct/Prepare/Release/
    // Destruct/RecvEvent/Update/GetID/GetRenderOutput/GetNumTextures/RenderComponent all
    // present and DWARF-named) and it is the component the licence-card photo needs.
    //
    // ⛔ Slots 1..9 are deliberately NULL, not stubs. Their renderers are in every state
    // from "48 KB, derives from the interface, overrides mis-named" (SatNav, AboveCar,
    // ProgressBar) down to "minimal slice with one ledger function and no base class at
    // all" (MainMap, InGameMessage, BlackBar, CrashNavIcon, BoostBar). Embedding those
    // would produce components that construct, prepare, report success and DRAW NOTHING
    // -- a hollow shell whose zeros look plausible. A null slot is honestly absent: every
    // loop and every dispatch in BrnCustomRenderer.cpp skips it, and the manager reports
    // "no such component" rather than "component says 0".
    CgsGui::CustomRenderComponentInterface* mapCustomRenderComponents[E_CUSTOM_RENDER_TYPES_COUNT];

    // The by-value subobject for slot 0 (guest this+0x1B660).
    NetworkPlayerImageRenderer mNetworkPlayerImageRenderer;

    // ⭐ [H3b] the by-value subobject for slot 1 (guest this+0x1050) -- the sat-nav
    // minimap renderer, reconstructed whole 2026-08-25 (RenderComponent + the zoomed
    // view chain landed; the hollow-shell caveat in the array banner no longer applies).
    SatNavRenderer mSatNavRenderer;

    // ⭐ [map-world 2026-08-29] the by-value subobject for slot 2 (guest this+0x28D0) --
    // BrnGui::MainMapRenderer, the full-screen map WORLD. It draws the published
    // active-texture set (the low-res Paradise City backdrop) inside the map's clip mask,
    // over its own fade-to-edges background.
    MainMapRenderer mMainMapRenderer;

    // ⭐ [map-world 2026-08-29] the by-value subobject for slot 3 (guest this+0xB060) --
    // BrnGui::CrashNavIconRenderer, the icon/road-sign/cursor layer that draws INTO the map
    // world above. Its class had been reconstructed and compiled since 2026-08-29 (FIX2)
    // but deliberately left unmounted: its RenderComponent opens with the map background
    // mask over the published view rect, so mounting it while slot 2 was still null put the
    // icons over the driving scene. The two go live together, in this order.
    CrashNavIconRenderer mCrashNavIconRenderer;

    // ⭐ [tut-ticker] the by-value subobject for slot 8 (guest this+0x1E0F0) -- the
    // bottom-of-screen ticker, reconstructed whole 2026-08-24. The hollow-shell caveat in
    // the array banner above no longer applies to it.
    InGameMessageRenderer mInGameMessageRenderer;

    // ⭐ [boost-bar] the by-value subobject for slot 4 (guest this+0xE520) -- the in-game
    // boost gauge, reconstructed whole 2026-08-24/25 (lifecycle + state machine + the full
    // render family). The hollow-shell caveat above no longer applies to it either.
    BoostBarRenderer mBoostBarRenderer;

    // Guest +0x1F498: the master rendering-enable flag SetAllRenderingState() stores.
    bool mbRenderingEnable;

    // Per-component renderable-state cache the Update() pass reads, plus TWO separate
    // gates. ⚠️ The previous model conflated them into one: Update @0x82450908 branches on
    // byte +0x1F4B1 and then ANDs each component's flag with byte +0x1F4AF -- two distinct
    // bytes, `if (*(a1+128177)) { ... v2 = a1+128175; ... *v2 && *(a1+128173) ... }`.
    bool mbSatNavRenderable;          // guest +0x1F4AD (SatNav slot, RecvEvent 213 sub-mode 1)
    bool mbMainMapRenderable;         // guest +0x1F4AE (MainMap slot, RecvEvent 213 sub-mode 0)
    bool mbComponentRenderableGate;   // guest +0x1F4AF (ANDed into every Update() enable)
    bool mbThirdSlotRenderable;       // guest +0x1F4B0 (the +0x1030 / AboveCar slot in Update)
    bool mbHaveValidMapPosition;      // guest +0x1F4B1 (the outer Update() branch gate)

    // Prepare / Release state-machine markers (guest +0x1F490 / +0x1F494).
    EPrepareStage mePrepareStage;
    EReleaseStage meReleaseStage;

    // Guest +0x1F4CC: per-frame "rendered without an update" HACK counter, zeroed by
    // Construct() and Update(); Render() increments it and clears the event queue past 10.
    s32 miHACK_NumberOfRendersWithoutUpdate;

    // Shared sub-system back-pointers the setters store (guest +0x1B91C / +0x1BEC4 /
    // +0x1BEC8 / +0x1BED0). The replay serialiser's concrete type is out of scope.
    BrnFlapt::FlaptRenderer*      mpFlaptRenderer;     // guest +0x1B91C
    CgsGraphics::TextRenderer*    mpTextRenderer;      // guest +0x1BEC4
    CgsLanguage::LanguageManager* mpLanguageManager;   // guest +0x1BEC8
    void*                         mpReplaySerialiser;  // guest +0x1BED0
};

// ⭐ [H3b x boost-bar reconcile 2026-08-25] BrnGui::SetMaskRect is an X360 OVERLOAD PAIR:
// @0x82450BE0 (pointer buffer; the boost-bar callers) and @0x82450D28 (reference buffer;
// the sat-nav caller -- a distinct function in the image, IDA-named BrnGui::SetMaskRect).
// Both are homed in BrnCustomRenderer.cpp; neither shadows the other.
// ---------------------------------------------------------------------------------------------
// BrnGui::SetMaskRect @0x82450BE0 (home: BrnCustomRenderer.cpp, per its own baked assert path,
// :889) -- the shared GUI clip-mask helper the boost-bar render paths use: transform the
// 0..1-proportion rect {x0,y0,x1,y1} into the mask's screen space, order the two corners
// min/max, and push an Im2dRenderBuffer clip mask (opcode 17) bound to lpTextureState with the
// corner UVs taken from lv4MaskUVs {u0,v0, u1,v1}. Body + PC-fold notes in BrnCustomRenderer.cpp.
// (The PS3 export also names the paired pop as BrnGui::UnsetMaskRect -- a plain PopMask wrapper
// the X360 build ICF-folds onto Im2dRenderBuffer::PopMask itself, so no separate symbol here.)
// ---------------------------------------------------------------------------------------------
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
                 const renderengine::TextureState* lpTextureState,
                 const rw::math::vpu::Vector4& lrv4Rect,
                 const rw::math::vpu::Vector4& lrv4MaskUVs);

// ---------------------------------------------------------------------------------
// Free mask helpers homed in BrnCustomRenderer.cpp (their X360 assert strings name
// that file).
//   SetMaskRect @0x82450D28 -- push a stencil/clip mask over a normalised screen rect
//     with the given mask texture state + mask-UV range (two corner vertices ->
//     Im2dRenderBuffer::PushMask). [H3b PC fold: corners are built in the engine's
//     1280x720 logical pixels; the console's normalised->NDC mask matrix hop
//     (SetMaskAspectCorrectionMatrix's product) collapses into that scale.]
//   SetMaskAspectCorrectionMatrix @0x82450A70 -- build the console's mask NDC/aspect
//     matrices (@0x82FB3220/@0x82FB3010). On the PC fold the pixel-space corners above
//     need no matrix; the call remains for the GuiModule::Construct call-order parity.
// ---------------------------------------------------------------------------------
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrCmd,
                 const renderengine::TextureState* lpMaskTextureState,
                 const Vector4& lv4Rect, const Vector4& lv4MaskUv);
void SetMaskAspectCorrectionMatrix(class GuiCache* lpGuiCache);

} // namespace BrnGui

#endif // BRN_CUSTOM_RENDERER_MANAGER_H
