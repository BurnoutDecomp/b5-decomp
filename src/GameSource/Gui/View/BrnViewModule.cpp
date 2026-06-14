#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::ViewModule::ViewModule  @ 0x827E3D18
//
// Constructor for the Burnout view module. It chains to the shared
// CgsGui::ViewModule base, then initialises its own state: an empty intrusive
// view list (the head's next/prev/tail all point back at the head) and two
// "uninitialised" sentinel indices set to INT_MAX. The base class and the members
// between the list and the sentinels are large and not yet individually recovered,
// so they are held as explicit padding (their real fields are separate TUs); every
// field this constructor touches is accessed by name.

namespace CgsGui
{
    // Shared view-module base (own TU). Polymorphic: its vtable pointer sits at
    // offset 0. Body is a trap stub until the base TU lands.
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

namespace BrnGui
{
    // Empty intrusive list head: the three link pointers all reference the head
    // itself, the canonical "empty circular list" state.
    struct ViewListHead
    {
        s32   maiHeader[3];
        void* mpNext;
        void* mpPrev;
        void* mpTail;
        s32   miCount;
    };

    class ViewModule : public CgsGui::ViewModule
    {
    public:
        ViewModule();
        ~ViewModule() override {}
    private:
        ViewListHead mViewList;            // @ object +167116
        u8           mPad1[2184];          // intervening members (separate TUs)
        s32          miFocusIndex;         // @ +169328, INT_MAX sentinel
        u8           mPad2[16];
        s32          miSelectionIndex;     // @ +169348, INT_MAX sentinel
    };

    ViewModule::ViewModule()
    {
        mViewList.maiHeader[0] = 0;
        mViewList.maiHeader[1] = 0;
        mViewList.maiHeader[2] = 0;
        mViewList.mpNext = &mViewList;
        mViewList.mpPrev = &mViewList;
        mViewList.mpTail = &mViewList;
        mViewList.miCount = 0;

        miFocusIndex = 0x7FFFFFFF;
        miSelectionIndex = 0x7FFFFFFF;
    }
}
