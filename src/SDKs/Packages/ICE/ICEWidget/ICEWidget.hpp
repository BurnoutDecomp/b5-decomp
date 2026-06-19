#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGET_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGET_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp
//
// ICE::ICEWidget -- the common base of the ICE camera-take editor's on-screen
// widgets (the dev-only ICE editor overlay). Siblings derived from it are the
// menu (ICEWidgetMenu), the time bar (ICEWidgetTimeBar), the colour picker
// (ICEWidgetColorPicker, the one DecFIGS DWARF we have) and the icon
// (ICEWidgetIcon). Each widget draws itself through ICE::ICERender.
//
// **MINIMAL, X360-DERIVED (DWARF-SPARSE).** The ONLY widget DWARF in DecFIGS is
// ICEWidgetColorPicker.hpp, which lists no base members and no vtable for the
// widget family -- its first member is mfX at hpp:83. The X360 asm confirms the
// widgets are NON-polymorphic, flat structs: ICEWidgetIcon::Render reads a float
// at this+0 (`lfs f0, 0(r10)`), so there is no vtable pointer at offset 0, and
// its callers (ICEWidgetMenu::Render, ICEWidgetTimeBar::Render) build an
// ICEWidgetIcon on the stack and call Render as a DIRECT (non-virtual) call.
//
// We therefore model ICEWidget as an EMPTY, layout-neutral base: derived widgets
// place their own fields starting at offset 0. The widget-shared methods
// (visibility / parent wiring that the family presumably has) are NOT attested by
// the X360 ledger for this TU and have no DWARF here, so none are declared -- the
// base exists only to express the `: public ICEWidget` family relationship the
// DWARF/asm imply. Grow this header (adding base members BEFORE the derived
// fields, plus padding) only when an X360-attested using-TU proves a base member.
//
// FLAG: the entire ICEWidget base shape (emptiness, non-polymorphic) is DERIVED
//       from the ICEWidgetIcon::Render asm + its callers, NOT from DWARF (the only
//       widget DWARF, ICEWidgetColorPicker.hpp, shows no base members/vptr).
// ============================================================================

namespace ICE
{
    // The empty common base of the ICE editor widgets. Layout-neutral: it adds no
    // members and no vtable, so a derived widget's first member sits at offset 0
    // (proven by ICEWidgetIcon::Render reading mfX at this+0). EBO keeps `sizeof`
    // of the derived struct equal to its own fields.
    struct ICEWidget
    {
    };
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGET_HPP
