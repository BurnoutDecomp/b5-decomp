#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::ViewModule::ViewModule  @ 0x827E3D18
//
// Default constructor for the Burnout view module. It chains to the shared
// CgsGui::ViewModule base, zeroes its prepare/release stage enums, and brings up
// its FlaptManager — whose empty view list links back to its own head and whose
// focus/selection indices start "unset" (INT_MAX). Member names recovered from the
// DecFIGS DWARF (BrnGuiViewModule.h): meBrnPrepareStage, meBrnReleaseStage,
// mFlaptManager, mpGuiModule. The CgsGui::ViewModule base and the bulk of
// FlaptManager are large and recovered by their own TUs, so they are held as
// explicit padding; every field this constructor writes is named.

namespace BrnGui { class GuiModule; }

namespace CgsGui
{
    // Shared view-module base (own TU). Polymorphic: vtable pointer at offset 0.
    class ViewModule
    {
    public:
        ViewModule();
        virtual ~ViewModule() {}
    protected:
        u8 mPad0[167112]; // base members (recovered by their own TUs)
    };
    ViewModule::ViewModule() { __debugbreak(); }
}

namespace BrnFlapt
{
    // The Flash/APT manager embedded in the view module. Only the pieces the view
    // constructor initialises are modelled: the head of an intrusive, initially
    // empty circular view list, and the focus/selection index sentinels deeper in.
    class FlaptManager
    {
    public:
        FlaptManager();
    private:
        s32   miListHead;        // list-head sentinel payload
        void* mpNext;
        void* mpPrev;
        void* mpTail;
        s32   miCount;
        u8    mPad0[2184];
        s32   miFocusIndex;      // "no focus" == INT_MAX
        u8    mPad1[16];
        s32   miSelectionIndex;  // "no selection" == INT_MAX
    };

    FlaptManager::FlaptManager()
    {
        miListHead = 0;
        mpNext = this;   // empty circular list: links point back at the head
        mpPrev = this;
        mpTail = this;
        miCount = 0;
        miFocusIndex = 0x7FFFFFFF;
        miSelectionIndex = 0x7FFFFFFF;
    }
}

namespace BrnGui
{
    class ViewModule : public CgsGui::ViewModule
    {
    public:
        ViewModule();
        ~ViewModule() override {}
    private:
        enum BrnPrepareStage : s32 { E_BRN_PREPARESTAGE_START = 0 };
        enum BrnReleaseStage : s32 { E_BRN_RELEASESTAGE_START = 0 };

        BrnPrepareStage      meBrnPrepareStage;
        BrnReleaseStage      meBrnReleaseStage;
        BrnFlapt::FlaptManager mFlaptManager;
        BrnGui::GuiModule*   mpGuiModule;
    };

    ViewModule::ViewModule()
        : meBrnPrepareStage(E_BRN_PREPARESTAGE_START),
          meBrnReleaseStage(E_BRN_RELEASESTAGE_START),
          mpGuiModule(nullptr)
    {
        // mFlaptManager brings up the empty view list and index sentinels in its
        // own constructor (above).
    }
}
