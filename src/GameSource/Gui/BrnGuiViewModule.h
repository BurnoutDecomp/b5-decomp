#ifndef BRN_GUI_VIEW_MODULE_H
#define BRN_GUI_VIEW_MODULE_H

#include "types.hpp"

#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"  // CgsGui::ViewModule (base) + IO/colour fwd-decls
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"              // BrnFlapt::FlaptManager (embedded by value)

// ============================================================================
// GameSource/Gui/BrnGuiViewModule.h
//
// BrnGui::ViewModule -- the Burnout GUI view module. It extends the shared
// CgsGui::ViewModule (the engine-side GUI view subsystem module) by embedding a
// BrnFlapt::FlaptManager and driving it in lock-step with the base module's
// lifecycle (Construct / Prepare / Release / Destruct / Update / Render) and by
// turning a "FLAPT load" resource-load notification into a FlaptManager file
// registration.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). Member NAMES /
// TYPES / order and the two staging enums are from the DecFIGS DWARF
// (BrnGuiViewModule.h); member PLACEMENT and behaviour are confirmed against the
// X360 pseudocode + asm:
//   Construct                       @ 0x824F13B8   (virtual; EXECUTED in boot trace)
//   Prepare                         @ 0x824F1468   (virtual)
//   Release                         @ 0x824F15B0   (virtual)
//   Destruct                        @ 0x824F16F0   (virtual)
//   Update                          @ 0x824F1730   (virtual)
//   RenderInternal                  @ 0x824F1770   (virtual, protected)
//   ProcessIncomingLoadNotification @ 0x824F9468   (virtual, protected)
//
// LAYOUT NOTE. The DWARF member set for BrnGui::ViewModule, in declaration order, is
// the four fields below; they are modelled as REAL NAMED C++ members and accessed BY
// NAME (no offset arithmetic). On the X360 build they sit PAST the large base at
//   +0x28CB0 meBrnPrepareStage / +0x28CB4 meBrnReleaseStage /
//   +0x28CC0 mFlaptManager (by value) / +0x29590 mpGuiModule
// (provenance only). Exact X360 byte offsets are NOT reproduced here: the base
// CgsGui::ViewModule is a declaration-only skeleton whose interior is dominated by
// uncommitted pointer-bearing sub-objects, and the x64 PC ABI widens those pointers, so
// byte-for-byte placement is neither achievable nor required (the project gate is
// SEMANTIC parity by named members, not byte-matching). The base sub-objects this module
// hands to its FlaptManager (and the per-frame time-step it reads) are reached through
// the base's own named accessors (GetImRendererSet() / GetTextRenderer() /
// GetLanguageManager() / GetFontCollection() / GetUpdateTimeStep()).
// ============================================================================

namespace BrnGui
{
    class GuiModule;   // owning GUI module (stored by pointer; AAC sub-object via named accessor)

    class ViewModule : public CgsGui::ViewModule
    {
    public:
        // Two-phase Prepare state machine (DWARF BrnGuiViewModule.h:110). Prepare()
        // advances one stage per call: base-class Prepare, then the FlaptManager, then
        // DONE. (The X360 enum is shared with the asm switch: 0->1 on entry, 2 after the
        // base prepares, 3 when the flapt manager is ready.)
        enum BrnPrepareStage
        {
            E_BRNPREPARESTAGE_START      = 0,
            E_BRNPREPARESTAGE_BASE_CLASS = 1,
            E_BRNPREPARESTAGE_FLAPT      = 2,
            E_BRNPREPARESTAGE_DONE       = 3,
        };

        // Two-phase Release state machine (DWARF BrnGuiViewModule.h:118). Mirror of
        // Prepare in reverse: release the FlaptManager first, then the base class.
        enum BrnReleaseStage
        {
            E_BRNRELEASESTAGE_START      = 0,
            E_BRNRELEASESTAGE_FLAPT      = 1,
            E_BRNRELEASESTAGE_BASE_CLASS = 2,
            E_BRNRELEASESTAGE_DONE       = 3,
        };

        // @0x824F13B8 -- construct the base view module, store the owning GuiModule, build
        // the embedded FlaptManager and seed the two staging enums. NOTE: this is a NEW
        // virtual (it prepends a GuiModule* before the base Construct's parameter list), not
        // an override of CgsGui::ViewModule::Construct.
        virtual void Construct(BrnGui::GuiModule* lpGuiModule, const char* lpcName, int liArg2,
                               f32 lfArg3, const RGBA* lpColour, int liArg5);

        // @0x824F1468 -- advance the prepare state machine one step; true once DONE.
        virtual bool Prepare(CgsMemory::HeapMalloc* lpHeap, rw::IResourceAllocator* lpResAlloc,
                             CgsMemory::HeapMalloc* lpHeap2, CgsMemory::LinearMalloc* lpLinear) /*override*/;

        // @0x824F15B0 -- advance the release state machine one step; true once DONE.
        virtual bool Release() /*override*/;

        // @0x824F16F0 -- destruct the embedded FlaptManager, then the base view module.
        virtual void Destruct() /*override*/;

        // @0x824F1730 -- tick the base view module then the FlaptManager (fed the base
        // module's current time-step).
        virtual void Update(CgsGui::ViewIO::IOBufferStack* lpInStack,
                            CgsGui::ViewIO::IOBufferStack* lpOutStack,
                            const CgsGui::ViewIO::InputBuffer* lpInput,
                            CgsGui::ViewIO::OutputBuffer* lpOutput) /*override*/;

        // @0x824F1130 (DWARF BrnGuiViewModule.h:93) -- accessor for the embedded manager.
        BrnFlapt::FlaptManager* GetFlaptManager() { return &mFlaptManager; }

    protected:
        // @0x824F1770 -- clear the screen, render the base view content, then render the
        // FlaptManager (the in-game flash overlay) on top.
        virtual void RenderInternal(const CgsGui::ViewIO::InputBuffer* lpInput) /*override*/;

        // @0x824F9468 -- if the notification is a FLAPT load (request type 10 on X360),
        // register the loaded file with the FlaptManager and (re)prepare the GuiModule's
        // always-available components against it; otherwise defer to the base handler.
        virtual void ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent) /*override*/;

    private:
        // DWARF BrnGuiViewModule.h:127/128/130/132 (X360 order). Real named members,
        // accessed by name throughout the bodies.
        BrnPrepareStage        meBrnPrepareStage;   // X360 +0x28CB0 (a1[41772])
        BrnReleaseStage        meBrnReleaseStage;   // X360 +0x28CB4 (a1[41773])
        BrnFlapt::FlaptManager mFlaptManager;       // X360 +0x28CC0 (embedded by value)
        BrnGui::GuiModule*     mpGuiModule;         // X360 +0x29590 (a1[42340])
    };

    // ---- the 2D pixel-order drain hook (homed in BrnGuiModule.cpp) --------------------
    // FLAG PC-platform leaf. Flushes the Apt/GUI command buffer to D3D9 from inside
    // ViewModule::RenderInternal, BEFORE mFlaptManager.Render() submits the HUD through the
    // IMMEDIATE Im2d backend -- so the HUD lands on top of the sat-nav map, as the console's
    // single recorded buffer replays it. ON by default; BRN_FLAPT_AFTER_DISPATCH=0 restores
    // the old order on the same binary (the fix's falsification control). Declared here (not
    // via BrnGuiModule.h) so this TU keeps avoiding GuiModule.h's heavy transitive includes,
    // exactly as GetAlwaysAvailableComponentsManager does. Full mechanism note at the
    // definition in BrnGuiModule.cpp.
    void DrainAptRenderResidueBeforeFlapt();
}

#endif // BRN_GUI_VIEW_MODULE_H
