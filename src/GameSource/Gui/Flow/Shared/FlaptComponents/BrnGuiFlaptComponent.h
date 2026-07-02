#ifndef BRN_GUI_FLAPT_COMPONENT_H
#define BRN_GUI_FLAPT_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef (embedded by value)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h
//
// BrnGui::BrnFlaptComponent -- the shared base of every apt-driven GUI flow
// component (FlaptButtonIconComponent, FlaptIconComponent, FlaptAnimatorComponent,
// FlaptHelpItem, FlaptTimerFieldComponent, ...). It owns the StateInterface the
// component talks to plus a single MovieClipRef handle (mAptRef) onto the live apt
// movie clip the component drives, and exposes Prepare() which resolves+binds that
// clip out of a loaded Flapt file. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// member names/types/shape taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../BrnGuiFlaptComponent.h), gated on the X360 ledger.
//
// This is the single declaration home for the base so the FlaptComponents subclass
// TUs share one definition (no ODR fork). It is the hard base/contained-type
// dependency the subclasses #include and derive from.
//
// LAYOUT (DWARF + the X360 asm of Prepare @ 0x8240E670 and the subclass Construct
// bodies, e.g. FlaptIconComponent::Construct @ 0x8241C1F0 / FlaptHelpItem::Construct
// @ 0x8241D198):
//   +0x00  mpStateInterface  -- the GUI state channel (the subclass Construct stores
//                               lpStateInterface into the FIRST word of the base
//                               subobject; the base is NOT polymorphic -- the vptr,
//                               where present, belongs to the *derived* class)
//   +0x04  mAptRef           -- BrnFlapt::MovieClipRef (mpMovieClipInst @ +0x04,
//                               mpTransform @ +0x08); Prepare fills it from the file
// sizeof(BrnFlaptComponent) == 0x0C.
//
// IMPORTANT (X360 layout vs. PS3 DWARF): the base carries NO own vtable. Polymorphic
// subclasses (FlaptIconComponent, FlaptTimerFieldComponent) introduce their vptr at
// the component's +0x00 and place this base subobject AFTER it (so the base lands at
// derived+0x04); non-polymorphic subclasses (FlaptButtonIconComponent, FlaptHelpItem)
// place the base at +0x00 directly. Both are proven by the subclass Construct asm.
//
// Only Prepare is X360-attested for the base itself (the DWARF's Construct / the
// MovieClipRef Prepare / SetPosition / GetMovieClipRef / SetInvalid are PS3-only or
// inlined into the subclass bodies -- not in the X360 func ledger -- so they are not
// declared here). Grow this header additively as those member TUs land. All member
// access is BY NAME; there are no raw-offset hacks.
// ============================================================================

namespace CgsGui
{
    // Stored by-pointer only; full type lives in CgsGuiStateInterface.h. Forward-
    // declared here to keep that heavy GUI header out of this widely-included base.
    struct StateInterface;
}

namespace BrnFlapt
{
    struct FileRef;   // used by const-reference in Prepare; full type pulled in by the .cpp
}

namespace BrnGui
{
    class BrnFlaptComponent
    {
    public:
        // Prepare @ 0x8240E670 -- resolve the named component within lFile (the
        // composite key is "lacParentName_lacName" when a parent prefix is given,
        // else just lacName), bind its MovieClipRef into mAptRef, and reset the
        // bound clip's timeline. The subclasses forward up to this base
        // implementation. The X360 returns the timeline-reset result in r3 as a
        // tail-call artifact; logically (and per the DecFIGS declaration shape) the
        // method returns void. Non-virtual: the base has no vtable (see LAYOUT).
        void Prepare(const char* lacName,
                     const BrnFlapt::FileRef& lFile,
                     const char* lacParentName);

        // SetInvalid -- drop the bound clip. ADDITIVE GROW (the DWARF declares it,
        // BrnGuiFlaptComponent.h:30; the X360 always inlines it -- the per-component
        // mAptRef zero-pairs in BrnGui::BaseOverlayState's reset path
        // @0x824B1F0C/@0x824B2020/@0x824B2E50).
        void SetInvalid() { mAptRef.SetInvalid(); }

    protected:
        // +0x00 : the GUI state channel this component talks to (subclass Construct
        // stores lpStateInterface here; only stored in this cluster).
        CgsGui::StateInterface* mpStateInterface;

        // +0x04 : the live apt movie clip this component drives.
        BrnFlapt::MovieClipRef mAptRef;
    };
}

#endif // BRN_GUI_FLAPT_COMPONENT_H
