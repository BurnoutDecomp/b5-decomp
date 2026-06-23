#pragma once

#include "types.hpp"

// BrnGui::DistrictMarkerComponent - GUI component that draws a single district
// county marker on the map. It owns an apt-driven visual element (an apt view
// transition target) and a small state machine tracking whether the county icon
// is currently shown or being hidden. Layout/method reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x824733B8 (no prior source, no DecFIGS DWARF for
// this TU).
//
// The marker drives its apt element through a polymorphic transition target held
// at @this+0xC. The SetHideCountyIcon hook calls that target's view-state setter,
// which the asm reaches through vtable slot index 3 (byte offset 0xC) with a
// single string arg. The three preceding virtuals belong to the same target
// interface but are not exercised by this TU, so they are declared as opaque
// slots to preserve the observed vtable layout. Only the offsets this TU touches
// (the apt target @0xC and the icon state @0x5C) are modelled by name; the
// surrounding component storage that this TU does not read is left as named
// reserved regions so the named members land at their observed offsets without
// any raw-offset casts.
namespace BrnGui
{
    // Apt view-transition target driven by district markers. Only the view-state
    // setter at vtable index 3 is reached from this TU; earlier slots are reserved
    // to preserve the observed vtable layout (call site @0x824733F4 reads slot 0xC).
    // Embedded polymorphic apt-transition subobject (its first word is a vptr). The X360
    // vcall is `(*(*(this+0xC)+12))(this+0xC, ...)` -- it dispatches through THIS
    // subobject's vtable slot 3 with this = &(the subobject) = outer+0xC. Modelled as an
    // embedded-by-value member (NOT a pointer) so the call passes &mAptTarget == outer+0xC,
    // matching the asm this-register. Concrete stub virtuals (real bodies in the target's
    // own TU); SetViewState is a no-op stub here.
    struct IDistrictMarkerAptTarget
    {
        virtual void ReservedSlot0() {}
        virtual void ReservedSlot1() {}
        virtual void ReservedSlot2() {}
        // @0xC: drive the apt element to a named view state ("transin"/"transout").
        virtual void SetViewState(const char* /*lpacViewState*/) {}
    };

    class DistrictMarkerComponent
    {
    public:
        // County-icon visibility state machine. Values match the stored codes the
        // X360 build writes to the state field (@this+0x5C).
        enum EIconState : s32
        {
            E_ICON_STATE_HIDDEN  = 5, // fully hidden / "transout" issued
            E_ICON_STATE_VISIBLE = 2, // shown / "transin" issued
        };

        // @ 0x824733B8 : drive the county icon hide/show transition.
        //   lbHide==true  & not already hidden  -> "transout", state := HIDDEN
        //   lbHide==false & currently hidden     -> "transin",  state := VISIBLE
        void SetHideCountyIcon(bool lbHide);

    private:
        // @this+0x0 .. +0x8 : component header this TU does not read by name.
        u8                        maReservedHeader[0xC];
        // @this+0xC : owned apt transition target, EMBEDDED by value (vptr at +0xC).
        IDistrictMarkerAptTarget  mAptTarget;
        // @this+0x10 .. +0x58 : component body this TU does not read by name.
        u8                        maReservedBody[0x5C - 0x10];
        // @this+0x5C : current county-icon state (EIconState).
        s32                       miIconState;
    };
}
