#ifndef BRN_CUSTOM_RENDERER_MANAGER_H
#define BRN_CUSTOM_RENDERER_MANAGER_H

#include "types.hpp"

#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h" // CgsGui::CustomRenderComponentInterface, eCustomRenderLayer, ImRendererSet
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                           // CgsGui::GuiEventQueueSmall (the base's mEventQueue)
#include "GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h" // the live slot-0 component
#include "GameSource/Gui/CustomRenderer/Renderers/BrnInGameMessageRenderer.h"      // [tut-ticker] the live slot-8 component

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

    // ⭐ [tut-ticker] the by-value subobject for slot 8 (guest this+0x1E0F0) -- the
    // bottom-of-screen ticker, reconstructed whole 2026-08-24. The hollow-shell caveat in
    // the array banner above no longer applies to it.
    InGameMessageRenderer mInGameMessageRenderer;

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

} // namespace BrnGui

#endif // BRN_CUSTOM_RENDERER_MANAGER_H
