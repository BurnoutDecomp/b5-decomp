#include "GameSource/Gui/BrnGuiSatNavDebugComponent.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // DrawFrame
#include "GameSource/Gui/CustomRenderer/Renderers/BrnSatNavRenderer.h"                       // SatNavRenderer::UpdateRendererTransform
#include "GameShared/GameClasses/Core/CgsAssert.h"                                           // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                                       // CgsGuiModuleIO::InputBuffer (Update)
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"                                    // Lock/UnlockBuffersForIO (Update)
#include "GameSource/Gui/BrnGuiModule.h"                                                     // GuiModule::mGuiCache (Update)
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"                                        // SatNavComponent::GetViewDistance (Update)
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                                            // MapTransform (the live sat-nav viewport rect)

// BrnGui::SatNavDebugComponent -- the sat-nav (mini-map) debug component bodies. OnActivate wires
// every editable to the debug menu; the change callbacks re-publish the state / rectangle to the
// renderer; RenderHUD draws the rect outline when its toggle is on. See the header for the layout.

namespace BrnGui
{
    // Original C++ initialization writes no module/toggle/status/base data.
    SatNavDebugComponent::SatNavDebugComponent()
        : CgsDev::DebugComponent(CgsDev::DebugComponent::GuiConstructorTag{})
    {
        mInputQueue.MarkUnconstructed();
    }

    // Actual enclosing module Destruct82507720..A4 inlines this derived body.
    void SatNavDebugComponent::Destruct()
    {
        if (!mpGuiModule)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpGuiModule != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiSatNavDebugComponent.cpp", 84);
            CgsDev::Assert::EndAssert();
        }
        mpGuiModule = 0;
        mInputQueue.Destruct();
        // The original following base call is folded to empty8284CB38.
    }

    // BrnSatNav.cpp:36..40 (DecFIGS); these are the actual shared tuning
    // words and the separately initialized/runtime-published lerp band.
    extern f32 KF_CAR_MAX_MPH;
    extern f32 KF_MIN_VIEW_DIST;
    extern f32 KF_MAX_VIEW_DIST;
    extern f32 KF_MIN_VIEW_LERP;
    extern f32 KF_MAX_VIEW_LERP;

    namespace
    {
        // Rect-corner slider step + normalised range, and the alpha range (asm: step flt_82013F90,
        // min flt_82001CC0=0, max flt_82001C98=1, alpha SetRange 0..100).
        const f32 KF_SATNAV_POS_STEP   = 0.001f;
        const f32 KF_SATNAV_POS_MIN    = 0.0f;
        const f32 KF_SATNAV_POS_MAX    = 1.0f;
        const u32 KU_SATNAV_ALPHA_MAX  = 100u;

        // RenderHUD: the normalised rect scaled into the 1280x720 virtual HUD space, 2px frame border.
        const f32 KF_SATNAV_HUD_WIDTH      = 1280.0f;   // flt_8201A7A8
        const f32 KF_SATNAV_HUD_HEIGHT     = 720.0f;    // flt_8201A7A4
        const f32 KF_SATNAV_OUTLINE_BORDER = 2.0f;      // flt_82001D9C
    }

    // @ 0x824ED338 -- debug-menu label.
    const char* SatNavDebugComponent::GetName() const
    {
        return "Sat Nav";
    }

    // ARTIST vtable 0x820CE830 slot 4 points to the ICF-shared 0x824ED348 body.
    const char* SatNavDebugComponent::GetPath() const
    {
        return "Gui";
    }

    // The base Construct, the module binding, the input queue, every toggle off, alpha 90, and the
    // four rect corners copied out of the live sat-nav viewport rect.
    void SatNavDebugComponent::Construct(GuiModule* lpGuiModule)
    {
        CGS_ASSERT(lpGuiModule != 0, "Invalid Gui module passed to SatNavDebugComponent::Construct");

        // Original base Construct resolves to empty8284CB38; no base data reset.
        mpGuiModule = lpGuiModule;
        mInputQueue.Construct();

        mbRivalFovFreeBurn          = false;
        mbRivalFovRace              = false;
        mbViewTrajectory            = false;
        mbRotateSatNav              = false;
        mbShowOffLineRivalsOnSatNav = false;
        mbDrawSatNavOutline         = false;

        miSatNavAlpha = 90;

        const Vector4& lrv4Rect = MapTransform::GetSatNavViewRect();
        mfSatNavTopLeftX     = lrv4Rect.x;
        mfSatNavTopLeftY     = lrv4Rect.y;
        mfSatNavBottomRightX = lrv4Rect.z;
        mfSatNavBottomRightY = lrv4Rect.w;
    }

    // Republish the (debug-tunable) view band as the lerp band, read the current view distance
    // for the player's speed and zoom level into the read-only readout, then append the queued
    // toggle events to the GUI module's input events and clear the local queue.
    void SatNavDebugComponent::Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer)
    {
        KF_MIN_VIEW_LERP = KF_MIN_VIEW_DIST;
        KF_MAX_VIEW_LERP = KF_MAX_VIEW_DIST;

        const GuiCache& lrGuiCache = mpGuiModule->mGuiCache;
        mfCurrentZoomValue = SatNavComponent::GetViewDistance(
            static_cast<f32>(lrGuiCache.miPlayerSpeedMph), lrGuiCache.GetSatNavZoomLevel());

        CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer");
        CgsModule::LockBuffersForIO(lpInputBuffer);
        lpInputBuffer->GetGuiEvents()->Append(mInputQueue);
        CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer");
        CgsModule::UnlockBuffersForIO(lpInputBuffer);

        mInputQueue.Clear();
    }

    // =============================================================================================
    // OnActivate  @ 0x82513280
    //
    // Register every editable of the SatNav debug component with the debug menu, in the asm's order:
    //   * six state toggles (rotate / rival-FOV free-burn / rival-FOV race / trajectory / off-line
    //     rivals / rect outline);
    //   * a SatNavStateCallback on five of them (all but the rect-outline toggle);
    //   * the four normalised rect corners + alpha, each wired to SatNavPositionCallback;
    //   * step 0.001 and range [0,1] on the four corners, range [0,100] on alpha;
    //   * the three module-scope view-distance / max-speed tuning globals;
    //   * the read-only "current zoom" display.
    // =============================================================================================
    void SatNavDebugComponent::OnActivate()
    {
        RegisterVariable( &mbRotateSatNav,               "SatNav: Rotate SatNav as player car turns" );
        RegisterVariable( &mbRivalFovFreeBurn,           "SatNav: FOV for rivals in Free Burn" );
        RegisterVariable( &mbRivalFovRace,               "SatNav: FOV for rivals in races" );
        RegisterVariable( &mbViewTrajectory,             "SatNav: ToggleTrajectoryView" );
        RegisterVariable( &mbShowOffLineRivalsOnSatNav,  "SatNav: Show off-line Rivals" );
        RegisterVariable( &mbDrawSatNavOutline,          "SatNav: Show Sat Nav Rect outline" );

        SetChangeCallback( &mbRivalFovFreeBurn,          &SatNavStateCallback, this );
        SetChangeCallback( &mbRivalFovRace,              &SatNavStateCallback, this );
        SetChangeCallback( &mbViewTrajectory,            &SatNavStateCallback, this );
        SetChangeCallback( &mbRotateSatNav,              &SatNavStateCallback, this );
        SetChangeCallback( &mbShowOffLineRivalsOnSatNav, &SatNavStateCallback, this );

        RegisterVariable( &mfSatNavTopLeftX,     "SatNav: Top Left X" );
        RegisterVariable( &mfSatNavTopLeftY,     "SatNav: Top Left Y" );
        RegisterVariable( &mfSatNavBottomRightX, "SatNav: Bottom Right X" );
        RegisterVariable( &mfSatNavBottomRightY, "SatNav: Bottom Right Y" );
        RegisterVariable( &miSatNavAlpha,        "SatNav: Alpha" );

        SetChangeCallback( &mfSatNavTopLeftX,     &SatNavPositionCallback, this );
        SetChangeCallback( &mfSatNavTopLeftY,     &SatNavPositionCallback, this );
        SetChangeCallback( &mfSatNavBottomRightX, &SatNavPositionCallback, this );
        SetChangeCallback( &mfSatNavBottomRightY, &SatNavPositionCallback, this );
        SetChangeCallback( &miSatNavAlpha,        &SatNavPositionCallback, this );

        SetStep( &mfSatNavTopLeftX,     KF_SATNAV_POS_STEP );
        SetStep( &mfSatNavTopLeftY,     KF_SATNAV_POS_STEP );
        SetStep( &mfSatNavBottomRightX, KF_SATNAV_POS_STEP );
        SetStep( &mfSatNavBottomRightY, KF_SATNAV_POS_STEP );

        SetRange( &mfSatNavTopLeftX,     KF_SATNAV_POS_MIN, KF_SATNAV_POS_MAX );
        SetRange( &mfSatNavTopLeftY,     KF_SATNAV_POS_MIN, KF_SATNAV_POS_MAX );
        SetRange( &mfSatNavBottomRightX, KF_SATNAV_POS_MIN, KF_SATNAV_POS_MAX );
        SetRange( &mfSatNavBottomRightY, KF_SATNAV_POS_MIN, KF_SATNAV_POS_MAX );
        SetRange( &miSatNavAlpha,        0u, KU_SATNAV_ALPHA_MAX );

        RegisterVariable( &KF_MAX_VIEW_DIST, "SatNav: Max View Distance" );
        RegisterVariable( &KF_MIN_VIEW_DIST, "SatNav: Min View Distance" );
        RegisterVariable( &KF_CAR_MAX_MPH,     "SatNav: Max Speed MPH" );

        RegisterVariable( &mfCurrentZoomValue, "SatNav: Current Zoom variable (Read Only)" );
        SetReadOnly( &mfCurrentZoomValue, true );
    }

    // =============================================================================================
    // RenderHUD  @ 0x824F7F28
    //
    // When the "Show Sat Nav Rect outline" toggle is on, draw the SatNav rect as a 2-pixel frame. The
    // live normalised sat-nav viewport rect (written by the position path) is read, the X lanes
    // scaled by the virtual-screen width (1280) and the Y lanes by the height (720),
    // and tail-calls DrawFrame(x0,y0,x1,y1,colour,border). (The X360 does the four scalings as a VMX
    // splat/multiply cascade; reproduced here as the equivalent per-corner scalar arithmetic.)
    //
    // ARTIST824F7FD8 loads r8 with 0xFFFF0000; the scalar DrawFrame entry
    // at8281C960 saves that colour at8281C990 and forwards it to each DrawBox.
    // The five float arguments occupy f1..f5; r8 is the packed-colour argument.
    // =============================================================================================
    void SatNavDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if ( !mbDrawSatNavOutline )
        {
            return;
        }

        const Vector4& lrv4Rect = MapTransform::GetSatNavViewRect();
        const f32 lfX0 = lrv4Rect.x * KF_SATNAV_HUD_WIDTH;   // TopLeftX     * 1280
        const f32 lfY0 = lrv4Rect.y * KF_SATNAV_HUD_HEIGHT;  // TopLeftY     *  720
        const f32 lfX1 = lrv4Rect.z * KF_SATNAV_HUD_WIDTH;   // BottomRightX * 1280
        const f32 lfY1 = lrv4Rect.w * KF_SATNAV_HUD_HEIGHT;  // BottomRightY *  720

        lpRender->DrawFrame( lfX0, lfY0, lfX1, lfY1, 0xFFFF0000u, KF_SATNAV_OUTLINE_BORDER );
    }

    // =============================================================================================
    // SatNavStateCallback  @ 0x8250D9C8  (registered VariableCallbackFunction; static)
    //
    // A sat-nav debug TOGGLE changed (Rotate / FOV-in-FreeBurn / FOV-in-races / ToggleTrajectoryView /
    // Show-off-line-rivals -- the five bools registered with a change callback in OnActivate). The X360
    // passes the component as the user-data (2nd arg), ignoring the unused value ptr (1st). It packs
    // the five toggle bytes into a 5-byte event and posts it (event type 200) into the component's
    // input queue for the GUI model to consume.
    // =============================================================================================
    void SatNavDebugComponent::SatNavStateCallback(void* /*lpValue*/, void* lpUserData)
    {
        CGS_ASSERT(lpUserData != nullptr, "lpUserData");   // BrnGuiSatNavDebugComponent.cpp:247

        SatNavDebugComponent* lpThis = static_cast<SatNavDebugComponent*>(lpUserData);

        // Byte image of the five sat-nav debug toggles (mbRivalFovFreeBurn@0x10 .. mbShowOffLineRivalsOnSatNav@0x14).
        SatNavStateChangeEvent lEvent;
        lEvent.macToggles[0] = static_cast<u8>(lpThis->mbRivalFovFreeBurn);
        lEvent.macToggles[1] = static_cast<u8>(lpThis->mbRivalFovRace);
        lEvent.macToggles[2] = static_cast<u8>(lpThis->mbViewTrajectory);
        lEvent.macToggles[3] = static_cast<u8>(lpThis->mbRotateSatNav);
        lEvent.macToggles[4] = static_cast<u8>(lpThis->mbShowOffLineRivalsOnSatNav);

        // Event type 200 (X360 0xC8), size 5 -- posted to the GUI model input queue (queue @+0x30).
        lpThis->mInputQueue.AddEvent(&lEvent, 200, 5);
    }

    // =============================================================================================
    // SatNavPositionCallback  @ 0x8250DB40  (registered VariableCallbackFunction; static)
    //
    // Any of the four rect corners / alpha changed: re-push the SatNav rect + alpha to the renderer.
    // The X360 passes the component as lpUserData and tail-calls the instance update.
    // =============================================================================================
    void SatNavDebugComponent::SatNavPositionCallback(void* /*lpValue*/, void* lpUserData)
    {
        CGS_ASSERT( lpUserData != nullptr, "lpUserData" );   // BrnGuiSatNavDebugComponent.cpp:281

        static_cast<SatNavDebugComponent*>(lpUserData)->TriggerSatNavPositionUpdate();
    }

    // =============================================================================================
    // TriggerSatNavPositionUpdate  @ 0x8250DA58
    //
    // A sat-nav rectangle/alpha slider changed (top-left/bottom-right X/Y, or Alpha). Republishes the
    // on-screen sat-nav viewport rectangle to the shared descriptor, rebuilds the renderer's screen-
    // space transform from it, then repacks the rectangle's tint colour (RGB white, A = alpha*2.55).
    //
    // FLAGS:
    //  - The rect goes to the live sat-nav viewport rect (MapTransform::SetSatNavRect, one aligned
    //    Vector4 store); BrnGui::SatNavRenderer::UpdateRendererTransform reads it.
    //  - The sat-nav renderer (X360 mpGuiModule+0x4D2E0) and its map-quad tint colour (mpGuiModule+
    //    0x4D2E8 == renderer+8) live in a part of the GuiModule layout this slice does not model by
    //    name; they are reached through the X360-attested byte offsets below.
    // =============================================================================================
    void SatNavDebugComponent::TriggerSatNavPositionUpdate()
    {
        // Republish the sat-nav viewport rectangle (a single aligned Vector4 store on the X360).
        Vector4 lv4Rect;
        lv4Rect.x = mfSatNavTopLeftX;
        lv4Rect.y = mfSatNavTopLeftY;
        lv4Rect.z = mfSatNavBottomRightX;
        lv4Rect.w = mfSatNavBottomRightY;
        MapTransform::SetSatNavRect(lv4Rect);

        CGS_ASSERT(miSatNavAlpha <= 100, "miSatNavAlpha <= 100");   // BrnGuiSatNavDebugComponent.cpp:264

        // Reach the sat-nav renderer + its tint colour through the X360-attested GuiModule byte offsets.
        u8* lpGuiModuleBytes = reinterpret_cast<u8*>(mpGuiModule);
        SatNavRenderer* lpRenderer =
            reinterpret_cast<SatNavRenderer*>(lpGuiModuleBytes + KU_OFF_SATNAV_RENDERER);
        lpRenderer->UpdateRendererTransform();

        // Repack the map-quad tint: RGB white, alpha = round-toward-zero(miSatNavAlpha * 2.55).
        const u8 luAlpha = static_cast<u8>(static_cast<s64>((miSatNavAlpha & 0xFF) * 2.55));
        const u32 luColour = (static_cast<u32>(luAlpha) << 24) | 0x00FFFFFFu;
        *reinterpret_cast<u32*>(lpGuiModuleBytes + KU_OFF_SATNAV_ICON_COLOUR) = luColour;
    }
}
