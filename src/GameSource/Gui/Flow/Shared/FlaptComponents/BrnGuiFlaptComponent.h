#ifndef BRN_GUI_FLAPT_COMPONENT_H
#define BRN_GUI_FLAPT_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef (embedded by value)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h
//
// BrnGui::BrnFlaptComponent -- the shared base of every apt-driven GUI flow
// component (FlaptButtonIconComponent, FlaptIconComponent, FlaptAnimatorComponent,
// FlaptHelpItem, JunctionInfoComponent, FlaptTimerFieldComponent, ...). It owns a
// single MovieClipRef handle (mAptRef) onto the live apt movie clip the component
// drives, and exposes the virtual Prepare() that resolves+binds that clip out of a
// loaded Flapt file. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This is the single declaration home for the base so the FlaptComponents subclass
// TUs share one definition (no ODR fork). It is the hard base/contained-type
// dependency the subclasses #include.
//
// LAYOUT (proven by the X360 asm of Prepare @ 0x8240E670 and the subclass Setup
// bodies, e.g. FlaptButtonIconComponent::Setup @ 0x8241C788):
//   +0x00  vptr            -- the class is polymorphic; Prepare is virtual and the
//                             subclasses both override it and call up to this base
//   +0x04  mAptRef         -- BrnFlapt::MovieClipRef (mpMovieClipInst @ +0x04,
//                             mpTransform @ +0x08); Prepare fills it from the file
// sizeof(BrnFlaptComponent) == 0x0C; subclass members follow at +0x0C
// (FlaptButtonIconComponent::meButton @ +0x0C, mAptButtonRef @ +0x10).
//
// Only Prepare is X360-attested for the base itself; grow this header additively
// (Construct, the destructor, any further shared accessors) as those member TUs
// land. All member access is BY NAME; there are no raw-offset hacks.
// ============================================================================

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
        // bound clip's timeline. Virtual: every subclass overrides it and forwards
        // up to this base implementation. The X360 returns the timeline-reset
        // result in r3 as a tail-call artifact; logically (and per the DecFIGS
        // declaration shape of the subclass overrides) the method returns void.
        virtual void Prepare(const char* lacName,
                             const BrnFlapt::FileRef& lFile,
                             const char* lacParentName);

        // +0x04 : the live apt movie clip this component drives.
        BrnFlapt::MovieClipRef mAptRef;
    };
}

#endif // BRN_GUI_FLAPT_COMPONENT_H
