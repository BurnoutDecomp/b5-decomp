#include "GameSource/Gui/CustomRenderer/BrnGuiCustRendererDebugComponent.h"

#include "GameSource/Gui/BrnCustomRendererManager.h"                              // BrnGui::CustomRendererManager (owning manager)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // CgsDev::Debug2DImmediateRender (DrawValue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT

// BrnGui::GuiCustRendererDebugComponent -- the "Gui Custom Renderer" in-game debug-menu component.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This TU owns the 10 X360-attested bodies; the live
// boost-bar editing reaches a renderer deep inside the custom-renderer manager, which the X360 does
// through external (outlined) helpers -- those are declared file-locally below (their bodies live in
// their own TUs), exactly the way the sibling debug components declare MaybeDrawText.

namespace BrnGui
{
// ------------------------------------------------------------------------------------------------
// External / outlined helpers this TU calls (declared for the compile gate; bodied in their own TUs).
// ------------------------------------------------------------------------------------------------

// MaybeDrawText (X360 0x82824048) -- the shared "draw debug text iff on-screen" helper the HUD draw
// loop uses; wraps CgsDev::Debug2DImmediateRender::DrawText. Signature recovered from the call sites:
// (display, text, x, y, scale, colour, centred).
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

// SetBoostBarColoursForBoostType (X360 0x8244B450) -- apply the predefined inner/outer colour pair
// for the given boost type to the manager's live boost-bar renderer. The X360 reaches the embedded
// BoostBarRenderer at a fixed offset inside the manager (mpCustRenderManager + the subobject offset)
// and loads the colour pair from a per-type .rdata table; that resolution lives inside this helper
// (its own TU), so this TU passes the manager by name and never touches a raw offset.
void SetBoostBarColoursForBoostType(CustomRendererManager* lpManager, BrnWorld::EBoostType leBoostType);

// ToggleBoostBarRendererDebugScreen -- flip the boost-bar renderer's own debug-screen flag. The X360
// reaches it as a byte deep inside the manager (the embedded BoostBarRenderer's flag); the subobject
// resolution lives in this helper's TU, so this TU passes the manager by name (no raw offset here).
void ToggleBoostBarRendererDebugScreen(CustomRendererManager* lpManager);

// HUD layout constants (read off the RenderHUD asm: flt_82004000 == 16.0 is the text scale; the
// X/Y coordinates are the .rdata floats the pseudocode already resolves to these literals).
namespace
{
    const f32 KF_TEXT_SCALE = 16.0f;          // flt_82004000

    const CgsDev::RGBA K_WHITE = 0xFFFFFFFFu;  // li r30, -1  (the colour every label/value uses)

    // Boost-colour debug-screen column/row anchors.
    const f32 KF_LABEL_X_RED      =  50.0f;   // "Red Channel"
    const f32 KF_LABEL_X_GREEN    = 250.0f;   // "Green Channel"
    const f32 KF_LABEL_X_INNER    = 450.0f;   // "Inner Flame" column
    const f32 KF_LABEL_X_OUTER    = 660.0f;   // "Outer Flame" column
    const f32 KF_LABEL_X_COMBINED = 850.0f;   // "Outer Flame + Inner Flame"
    const f32 KF_ROW_Y_TOP        =  40.0f;
    const f32 KF_ROW_Y_SECOND     =  60.0f;

    // Active-renderer list layout (the name column at x=50, the texture-count value at x=200).
    const f32 KF_LIST_X        =  50.0f;       // flt_820138DC -- renderer-name column
    const f32 KF_LIST_VALUE_X  = 200.0f;       // flt_8201A1F0 -- texture-count value column
    const f32 KF_LIST_HEADER_Y =  20.0f;       // flt_820054CC -- header row
    const f32 KF_LIST_ROW_STEP =  40.0f;       // flt_82004D0C -- y == (visible row index) * 40

    // The active-renderer slots, in ECustomRenderTypes order (RenderHUD's switch over 0..9 + default).
    const s32 KI_CUSTOM_RENDER_TYPE_COUNT = 10;
}

// ================================================================================================
// Construct  @0x824EC850
// ------------------------------------------------------------------------------------------------
// Store the owning custom-renderer manager, then seed the menu state: every layer toggle off, the
// master "Enable Custom Renderer" flag on, and the six boost-colour sliders zeroed.
// ================================================================================================
void GuiCustRendererDebugComponent::Construct(CustomRendererManager* lpCustRenderManager)
{
    CGS_ASSERT(lpCustRenderManager != nullptr,
               " invalid custom render manager passed to custom render debug component");

    // The X360 chains the (empty) base DebugComponent::Construct here (ICF-folded to a bare blr).
    DebugComponent::Construct();

    mpCustRenderManager = lpCustRenderManager;

    mbShowCustomRenderStatus    = false;
    mbSatNavRenderEnabled       = false;
    mbMainMapRenderEnabled      = false;
    mbAllRenderEnabled          = true;   // stb r11(==1), 0x13 -- custom rendering on by default
    mbAboveCarRenderEnabled     = false;
    mbOverride                  = false;
    mbBoostBarDebugScreenEnabled = false;

    mfBoostOuterRed   = 0.0f;
    mfBoostOuterGreen = 0.0f;
    mfBoostOuterBlue  = 0.0f;
    mfBoostInnerRed   = 0.0f;
    mfBoostInnerGreen = 0.0f;
    mfBoostInnerBlue  = 0.0f;
}

// ================================================================================================
// Destruct  @0x824EC938
// ------------------------------------------------------------------------------------------------
// Assert the component was constructed, drop the manager pointer, then chain the base teardown.
// ================================================================================================
void GuiCustRendererDebugComponent::Destruct()
{
    CGS_ASSERT(mpCustRenderManager != nullptr, " destroying a non constructed object");

    mpCustRenderManager = nullptr;

    // The X360 chains the (empty) base DebugComponent::Destruct here (ICF-folded to a bare blr).
    DebugComponent::Destruct();
}

// ================================================================================================
// GetName  @0x824EC9E0
// ================================================================================================
const char* GuiCustRendererDebugComponent::GetName() const
{
    return "Gui Custom Renderer";
}

// ================================================================================================
// OnActivate  @0x824FCBC8
// ------------------------------------------------------------------------------------------------
// Register every debug-menu control: the per-layer bool toggles, the five boost-colour action
// buttons, and the six boost-colour sliders (each clamped to [0,2] with a 0.025 step). Finishes by
// priming the slider cache from the live boost-bar renderer.
// ================================================================================================
void GuiCustRendererDebugComponent::OnActivate()
{
    RegisterVariable(&mbShowCustomRenderStatus, "Show Custom Renderer Status");
    RegisterVariable(&mbOverride,               "Override what game is telling to render");
    RegisterVariable(&mbSatNavRenderEnabled,    "Sat Nav Render");
    RegisterVariable(&mbMainMapRenderEnabled,   "Main Map Render");
    RegisterVariable(&mbAllRenderEnabled,       "Enable Custom Renderer");
    RegisterVariable(&mbAboveCarRenderEnabled,  "Above Car Renderer");

    RegisterFunction(&SetBoostColourDangerCallback,        this, "Set boost colour to Danger");
    RegisterFunction(&SetBoostColourAggressionCallback,    this, "Set boost colour to Aggression");
    RegisterFunction(&SetBoostColourStuntsCallback,        this, "Set boost colour to Stunts");
    RegisterFunction(&SetBoostColourToCustomCallback,      this, "Set boost bar colours to custom");
    RegisterFunction(&ToggleBoostBarDebugScreenCallback,   this, "Toggle Boost Bar Debug screen");

    RegisterVariable(&mfBoostOuterRed, "Outer boost Red");
    SetRange(&mfBoostOuterRed, 0.0f, 2.0f);
    SetStep(&mfBoostOuterRed, 0.025f);

    RegisterVariable(&mfBoostOuterGreen, "Outer boost Green");
    SetRange(&mfBoostOuterGreen, 0.0f, 2.0f);
    SetStep(&mfBoostOuterGreen, 0.025f);

    RegisterVariable(&mfBoostOuterBlue, "Outer boost Blue");
    SetRange(&mfBoostOuterBlue, 0.0f, 2.0f);
    SetStep(&mfBoostOuterBlue, 0.025f);

    RegisterVariable(&mfBoostInnerRed, "Inner boost Red");
    SetRange(&mfBoostInnerRed, 0.0f, 2.0f);
    SetStep(&mfBoostInnerRed, 0.025f);

    RegisterVariable(&mfBoostInnerGreen, "Inner boost Green");
    SetRange(&mfBoostInnerGreen, 0.0f, 2.0f);
    SetStep(&mfBoostInnerGreen, 0.025f);

    RegisterVariable(&mfBoostInnerBlue, "Inner boost Blue");
    SetRange(&mfBoostInnerBlue, 0.0f, 2.0f);
    SetStep(&mfBoostInnerBlue, 0.025f);

    UpdateBoostColours();
}

// ================================================================================================
// RenderHUD  @0x82505108
// ------------------------------------------------------------------------------------------------
// Two independent gated read-outs:
//   1. mbBoostBarDebugScreenEnabled -> the boost-colour channel/flame labels (after re-applying the
//      current slider values to the live renderer via SetBoostColourToCustom).
//   2. mbShowCustomRenderStatus     -> the list of currently-active custom renderers, each shown
//      with its name and texture count (only renderers the manager reports as active are listed,
//      drawn top-down by visible-row index).
// ================================================================================================
void GuiCustRendererDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    // The asm calls SetBoostColourToCustom unconditionally up front (push the live sliders to the
    // renderer) before either gated block.
    SetBoostColourToCustom();

    if (mbBoostBarDebugScreenEnabled)
    {
        MaybeDrawText(lpDisplay, "Red Channel",               KF_LABEL_X_RED,      KF_ROW_Y_TOP,    KF_TEXT_SCALE, K_WHITE, false);
        MaybeDrawText(lpDisplay, "Green Channel",             KF_LABEL_X_GREEN,    KF_ROW_Y_TOP,    KF_TEXT_SCALE, K_WHITE, false);
        MaybeDrawText(lpDisplay, "Inner Flame",               KF_LABEL_X_INNER,    KF_ROW_Y_TOP,    KF_TEXT_SCALE, K_WHITE, false);
        MaybeDrawText(lpDisplay, "(Red * Inner Colour)",      KF_LABEL_X_INNER,    KF_ROW_Y_SECOND, KF_TEXT_SCALE, K_WHITE, false);
        MaybeDrawText(lpDisplay, "Outer Flame",               KF_LABEL_X_OUTER,    KF_ROW_Y_TOP,    KF_TEXT_SCALE, K_WHITE, false);
        MaybeDrawText(lpDisplay, "(Green * Outer Colour)",    KF_LABEL_X_OUTER,    KF_ROW_Y_SECOND, KF_TEXT_SCALE, K_WHITE, false);
        MaybeDrawText(lpDisplay, "Outer Flame + Inner Flame", KF_LABEL_X_COMBINED, KF_ROW_Y_TOP,    KF_TEXT_SCALE, K_WHITE, false);
    }

    if (mbShowCustomRenderStatus)
    {
        MaybeDrawText(lpDisplay, "Currently active renderers (number of textures):",
                      KF_LIST_X, KF_LIST_HEADER_Y, KF_TEXT_SCALE, K_WHITE, false);

        s32 liVisibleRow = 0;
        for (s32 liComponent = 0; liComponent < KI_CUSTOM_RENDER_TYPE_COUNT; ++liComponent)
        {
            if (!mpCustRenderManager->GetComponentRenderable(liComponent))
            {
                continue;
            }

            const char* lpcName = nullptr;
            switch (liComponent)
            {
                case 0:  lpcName = "Player Image";    break;
                case 1:  lpcName = "Sat Nav";         break;
                case 2:  lpcName = "Main Map";        break;
                case 3:  lpcName = "Crash Nav Icons"; break;
                case 4:  lpcName = "Boost Bar";       break;
                case 5:  lpcName = "Above Car";       break;
                case 6:  lpcName = "Progress Bar";    break;
                case 7:  lpcName = "Black Bar";       break;
                case 8:  lpcName = "Ticker";          break;
                default: lpcName = "Unknown custom render type"; break;
            }

            const f32 lfRowY = static_cast<f32>(liVisibleRow) * KF_LIST_ROW_STEP;
            MaybeDrawText(lpDisplay, lpcName, KF_LIST_X, lfRowY, KF_TEXT_SCALE, K_WHITE, false);

            const s32 liTextureCount = mpCustRenderManager->GetNumTexturesForComponent(liComponent);
            lpDisplay->DrawValue(static_cast<u32>(liTextureCount),
                                 KF_LIST_VALUE_X, lfRowY, KF_TEXT_SCALE, K_WHITE);

            ++liVisibleRow;
        }
    }
}

// ================================================================================================
// SetBoostColourDangerCallback  @0x824FC900
// SetBoostColourAggressionCallback  @0x824FC9B8
// SetBoostColourStuntsCallback  @0x824FCA70
// ------------------------------------------------------------------------------------------------
// Debug-menu action buttons: apply a predefined boost-type colour pair to the live boost-bar
// renderer, then refresh the slider cache. The registered user-data is the component.
// ================================================================================================
void GuiCustRendererDebugComponent::SetBoostColourDangerCallback(void* lpUserData)
{
    CGS_ASSERT(lpUserData != nullptr, "Invalid data in the callback");

    GuiCustRendererDebugComponent* lpComponent = static_cast<GuiCustRendererDebugComponent*>(lpUserData);
    SetBoostBarColoursForBoostType(lpComponent->mpCustRenderManager, BrnWorld::E_BOOST_TYPE_DANGER);
    lpComponent->UpdateBoostColours();
}

void GuiCustRendererDebugComponent::SetBoostColourAggressionCallback(void* lpUserData)
{
    CGS_ASSERT(lpUserData != nullptr, "Invalid data in the callback");

    GuiCustRendererDebugComponent* lpComponent = static_cast<GuiCustRendererDebugComponent*>(lpUserData);
    SetBoostBarColoursForBoostType(lpComponent->mpCustRenderManager, BrnWorld::E_BOOST_TYPE_AGGRESSION);
    lpComponent->UpdateBoostColours();
}

void GuiCustRendererDebugComponent::SetBoostColourStuntsCallback(void* lpUserData)
{
    CGS_ASSERT(lpUserData != nullptr, "Invalid data in the callback");

    GuiCustRendererDebugComponent* lpComponent = static_cast<GuiCustRendererDebugComponent*>(lpUserData);
    SetBoostBarColoursForBoostType(lpComponent->mpCustRenderManager, BrnWorld::E_BOOST_TYPE_STUNT);
    lpComponent->UpdateBoostColours();
}

// ================================================================================================
// SetBoostColourToCustomCallback  @0x824FCB28
// ------------------------------------------------------------------------------------------------
// Debug-menu action button: push the live slider values into the renderer as a custom colour pair.
// ================================================================================================
void GuiCustRendererDebugComponent::SetBoostColourToCustomCallback(void* lpUserData)
{
    CGS_ASSERT(lpUserData != nullptr, "Invalid data in the callback");

    static_cast<GuiCustRendererDebugComponent*>(lpUserData)->SetBoostColourToCustom();
}

// ================================================================================================
// ToggleBoostBarDebugScreenCallback  @0x824EC9F0
// ------------------------------------------------------------------------------------------------
// Debug-menu action button: flip BOTH the boost-bar renderer's own debug-screen flag (reached deep
// inside the manager) AND this component's mirror flag. The X360 toggles each with the
// cntlzw/extrwi "== 0" idiom (logical NOT of a bool).
// ================================================================================================
void GuiCustRendererDebugComponent::ToggleBoostBarDebugScreenCallback(void* lpUserData)
{
    CGS_ASSERT(lpUserData != nullptr, "Invalid data in the callback");

    GuiCustRendererDebugComponent* lpComponent = static_cast<GuiCustRendererDebugComponent*>(lpUserData);

    // Flip the renderer-side debug flag (the X360 toggles a byte inside the boost-bar renderer reached
    // through the manager), then this component's own mirror flag.
    ToggleBoostBarRendererDebugScreen(lpComponent->mpCustRenderManager);

    lpComponent->mbBoostBarDebugScreenEnabled = !lpComponent->mbBoostBarDebugScreenEnabled;
}

}
