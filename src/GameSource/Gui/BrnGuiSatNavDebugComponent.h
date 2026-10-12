#pragma once

// BrnGuiSatNavDebugComponent.h
// Home of BrnGui::SatNavDebugComponent -- the in-game debug component that tunes the sat-nav /
// mini-map from the debug menu: the display toggles (rotate-with-car, rival-FOV, trajectory view,
// off-line rivals, outline), the viewport rectangle (top-left / bottom-right X/Y), the alpha, and
// the read-only current-zoom readout. Derives from CgsDev::DebugComponent (the debug-menu
// plumbing). Class shape + member names/offsets from the DecFIGS DWARF
// (GameSource/Gui/BrnGuiSatNavDebugComponent.h:45/:94..:112); offsets confirmed against the X360
// OnActivate/Trigger* pseudocode/asm. The base CgsDev::DebugComponent occupies this+0..0x0B, so
// the first member sits at +0x0C:
//   mpGuiModule +0x0C  mbRivalFovFreeBurn +0x10  mbRivalFovRace +0x11  mbViewTrajectory +0x12
//   mbRotateSatNav +0x13  mbShowOffLineRivalsOnSatNav +0x14  mbDrawSatNavOutline +0x15
//   mfSatNavTopLeftX +0x18  mfSatNavTopLeftY +0x1C  mfSatNavBottomRightX +0x20
//   mfSatNavBottomRightY +0x24  miSatNavAlpha +0x28  mfCurrentZoomValue +0x2C  mInputQueue +0x30
//
// LAYOUT POLICY: the position-update path uses the typed owner chain
// GuiModule::mCustomRendererManager::mSatNavRenderer. Its X360 displacements are evidence for the
// two named member accesses (renderer at GuiModule+0x4D2E0, colour at +0x4D2E8); the PC build uses
// its native pointer widths. The live sat-nav viewport rect is BrnGui::MapTransform's
// smv4SatNavViewRect.

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector4
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue / Event

namespace CgsDev { struct Debug2DImmediateRender; }
namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer; } }

namespace BrnGui
{
    class GuiModule;
    class SatNavRenderer;

    // Byte image of the five sat-nav debug toggles posted by SatNavStateCallback (event type 200,
    // 5 bytes). CgsModule::Event is an empty base (no vptr) so macToggles sits at offset 0. Field
    // layout is a plain byte image (not individually attested); modelled opaque.
    struct SatNavStateChangeEvent : public CgsModule::Event
    {
        u8 macToggles[5];   // mbRivalFovFreeBurn / mbRivalFovRace / mbViewTrajectory /
                            // mbRotateSatNav / mbShowOffLineRivalsOnSatNav (component[0x10..0x14])
    };

    // BrnGuiSatNavDebugComponent.h:45
    struct SatNavDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Bind the GUI module, clear the toggles, seed alpha 90 and the rect from the live
        // sat-nav viewport rect.
        void Construct(GuiModule* lpGuiModule);
        void Destruct();

        // Per frame: republish the tuned view band, refresh the zoom readout, then hand the
        // queued toggle events to the GUI module's input buffer.
        void Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer);

        // @ 0x824F7F28 -- draw the SatNav rect outline when mbDrawSatNavOutline is set.
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

    protected:
        const char* GetName() const override;   // @0x824ED338 -> "Sat Nav"
        const char* GetPath() const override;   // debug-menu group path (out of batch)
        void OnActivate() override;             // @0x82513280 -- register every editable

        void TriggerSatNavParamUpdate();     // BrnGuiSatNavDebugComponent.cpp:223 (out of batch)
        void TriggerSatNavPositionUpdate();  // @0x8250DA58 -- republish rect + tint, rebuild transform

        // ---- debug-menu change callbacks (registered as VariableCallbackFunction = void(*)(void*,void*);
        //      static so they can be taken as plain C function pointers; the X360 passes the component
        //      as the user-data (2nd arg), ignoring the unused value ptr (1st)). ----
        static void SatNavStateCallback(void* lpValue, void* lpUserData);     // @0x8250D9C8
        static void SatNavPositionCallback(void* lpValue, void* lpUserData);  // @0x8250DB40

    private:
        // ---- member layout (DWARF h:94..:112; console byte offsets confirmed vs OnActivate asm) ----
        GuiModule* mpGuiModule;                 // +0x0C  owning GUI module (holds the sat-nav renderer)

        bool mbRivalFovFreeBurn;                // +0x10
        bool mbRivalFovRace;                    // +0x11
        bool mbViewTrajectory;                  // +0x12
        bool mbRotateSatNav;                    // +0x13
        bool mbShowOffLineRivalsOnSatNav;       // +0x14
        bool mbDrawSatNavOutline;               // +0x15  (drives RenderHUD's outline draw)

        f32  mfSatNavTopLeftX;                  // +0x18
        f32  mfSatNavTopLeftY;                  // +0x1C
        f32  mfSatNavBottomRightX;              // +0x20
        f32  mfSatNavBottomRightY;              // +0x24

        u32  miSatNavAlpha;                     // +0x28  0..100 (asserted <= 100)
        f32  mfCurrentZoomValue;                // +0x2C  read-only zoom readout

        CgsModule::VariableEventQueue<18432, 16> mInputQueue;  // +0x30  (DWARF: InputBuffer::GuiEventQueue)
    };

}
