#ifndef BRN_GUI_FLAPT_COMPONENT_H
#define BRN_GUI_FLAPT_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the Construct tripwire)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef (embedded by value)
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"        // BrnFlapt::FileRef::FindComponent (Prepare)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"  // CgsCore::SnPrintf (Prepare)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h
//
// BrnGui::BrnFlaptComponent -- the shared base of every apt-driven GUI flow
// component (FlaptButtonIconComponent, FlaptIconComponent, FlaptAnimatorComponent,
// FlaptHelpItem, FlaptTimerFieldComponent, ...). It owns the StateInterface the
// component talks to plus a single MovieClipRef handle (mAptRef) onto the live apt
// movie clip the component drives. Member names/types/shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../BrnGuiFlaptComponent.h), gated on the X360 ledger.
//
// SOURCE LINES: the declarations sit on the DWARF lines (class h:53, Construct h:64,
// Prepare h:71/h:75, GetMovieClipRef h:84, SetInvalid h:87, members h:91/h:92) and the
// header-inline bodies follow the class with their asserts on the original lines ARTIST
// bakes into every copy: Construct h:113 (FriendsListEntry::Construct @0x82414B28),
// Prepare h:133 (RoadSignIcon::Prepare @0x8241CC6C) and h:164. No .cpp in DWARF.
//
// LAYOUT (X360 asm of Prepare @ 0x8240E670 and the subclass Construct bodies):
//   +0x00  mpStateInterface  -- the GUI state channel. The base is NOT polymorphic:
//                               polymorphic subclasses put their vptr at +0x00 and
//                               this subobject at +0x04; the others place it at +0x00.
//   +0x04  mAptRef           -- BrnFlapt::MovieClipRef (mpMovieClipInst @ +0x04,
//                               mpTransform @ +0x08); Prepare fills it from the file
// sizeof(BrnFlaptComponent) == 0x0C.
//
// Not declared (no X360 ledger row): SetPosition (h:80). All member access is BY
// NAME; there are no raw-offset hacks.
// ============================================================================

namespace CgsGui
{
    // Stored by-pointer only; full type lives in CgsGuiStateInterface.h.
    struct StateInterface;
}


namespace BrnGui
{
    // DWARF: BrnGuiFlaptComponent.h:53. The X360 lays it out as a plain (vtable-less)
    // base; see LAYOUT above.
    //
    //
    //
    //
    class BrnFlaptComponent
    {
    public:
        // Longest component key Prepare builds ("<parent>_<name>"), DWARF h:57.
        static const u8 KU_MAX_COMPONENT_NAME_LEN = 128;
        // Construct -- adopt the state channel and invalidate the clip. X360
        // header-inline (never emitted out of line): the h:113 "lpStateInterface"
        // tripwire, the interface store, the ref clear. DecFIGS declares two further
        // const void* parameters no X360 inlined copy reads; the one-argument form is
        // what every Gui caller passes.
        //
        void Construct(CgsGui::StateInterface* lpStateInterface);

        // Prepare -- resolve the named component within lFile (the composite key is
        // "lacParentName_lacName" when a parent prefix is given, else just lacName),
        // bind its MovieClipRef into mAptRef, and reset the bound clip's timeline.
        // Header-inline (asserts name BrnGuiFlaptComponent.h); 0x8240E670 is the copy
        // ARTIST kept for the out-of-line callers (FlaptIconComponent::Prepare, ...).
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile, const char* lacParentName);

        // Prepare(const MovieClipRef*) -- adopt an already-resolved clip (assert
        // h:164); header-inline, out-of-line copy @ 0x8240E740.
        void Prepare(const BrnFlapt::MovieClipRef* lpMovieClipRef);

        // (h:80 SetPosition: PS3-only, see the header comment.)
        //
        // GetMovieClipRef -- the bound apt clip handle (DistrictMarkerComponent::Prepare
        // @0x82420FB8 reaches each embedded container movie's clip ref through it). Non-
        // virtual, like every member here: the base has no vtable (see LAYOUT).
        //
        const BrnFlapt::MovieClipRef& GetMovieClipRef() const { return mAptRef; }
        BrnFlapt::MovieClipRef&       GetMovieClipRef()       { return mAptRef; }
        //
        // SetInvalid -- drop the bound clip (X360 always inlines it: the mAptRef zero
        void SetInvalid() { mAptRef.SetInvalid(); }   // pairs @0x824B1F0C / @0x824B2020)

    protected:
        // +0x00 the GUI state channel; +0x04 the live apt movie clip this component drives.
        CgsGui::StateInterface* mpStateInterface;
        BrnFlapt::MovieClipRef  mAptRef;
    };

    // ------------------------------------------------------------------------
    // Header-inline definitions (the original header defines them after the
    // class; the assert below must stay on line 113, see SOURCE LINES above).
    // ------------------------------------------------------------------------
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    inline void BrnFlaptComponent::Construct(CgsGui::StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");
        mpStateInterface = lpStateInterface;
        mAptRef.SetInvalid();
    }

    // Prepare: DWARF locals lpFullName (h:135) and lacTempString (h:140, in the
    // parent-prefix block). The buffer is kept at function scope: lpFullName still
    // points into it at the FindComponent call.
    //
    //
    //
    //
    //
    //
    //
    //
    inline void BrnFlaptComponent::Prepare(const char* lacName,
                                           const BrnFlapt::FileRef& lFile,
                                           const char* lacParentName)
    {
        CGS_ASSERT_AT(lacName != NULL, "lacName != NULL", "..\\..\\..\\GameSource\\Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h", 133);

        const char* lpFullName = lacName;

        char lacTempString[KU_MAX_COMPONENT_NAME_LEN];
        if (lacParentName != NULL)
        {
            CgsCore::SnPrintf(lacTempString, KU_MAX_COMPONENT_NAME_LEN - 1,
                              "%s_%s", lacParentName, lacName);
            lacTempString[KU_MAX_COMPONENT_NAME_LEN - 1] = '\0';
            lpFullName = lacTempString;
        }

        mAptRef = lFile.FindComponent(lpFullName);

        mAptRef.ResetTimeline();
    }

    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    inline void BrnFlaptComponent::Prepare(const BrnFlapt::MovieClipRef* lpMovieClipRef)
    {
        CGS_ASSERT_AT(lpMovieClipRef != NULL, "NULL != lpMovieClipRef", "..\\..\\..\\GameSource\\Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h", 164);

        mAptRef = *lpMovieClipRef;

        mAptRef.ResetTimeline();
    }
}

#endif // BRN_GUI_FLAPT_COMPONENT_H
