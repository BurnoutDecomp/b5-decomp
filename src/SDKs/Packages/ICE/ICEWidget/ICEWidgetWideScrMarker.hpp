#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETWIDESCRMARKER_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETWIDESCRMARKER_HPP

#include "types.hpp"
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base
#include "SDKs/Packages/ICE/ICERender.hpp"               // ICE::ICERender / Poly / Vector4 / Matrix4

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.hpp
//
// ICE::ICEWidgetWideScrMarker -- the dev-only ICE camera-take editor's WIDESCREEN
// / letterbox marker widget. It draws the cinematic-crop guide rectangles the
// editor overlays on the take preview, as two flat screen-space quads pushed
// through ICE::ICERender::RenderPoly (one per RenderPoly call). Built like its
// siblings as a small flat struct the owning controller renders directly
// (ICEController::Render), so it is NON-polymorphic -- see ICEWidgetIcon for the
// asm-derived-widget pattern.
//
// LAYOUT -- NO DATA MEMBERS. Unlike its siblings (ICEWidgetIcon/Logo, which read
// per-instance position/size off `this`), the Render asm @0x82537B88 reads NOTHING
// off `this`: the entire marker geometry and colour are INLINE rodata constants
// (the -2.0 / 0.0 / 480.0 / 1.0 quad corners + the black marker colour), and the
// only register-passed input is the render context (r4), forwarded straight to
// RenderPoly. So this widget is an empty struct -- the base ICEWidget is empty
// (EBO), giving sizeof == 1 with no attested fields.
//
// FLAG: the empty layout (no data members) is DERIVED FROM ASM -- the Render body
//       never dereferences `this` (no `lfs/lwz N(r3)`); every coordinate/colour is
//       an inline `flt_*` constant. There is NO ICEWidgetWideScrMarker DWARF (the
//       only ICE widget DWARF in DecFIGS is ICEWidgetColorPicker.hpp). Grow this
//       header (adding fields BEFORE any future use) only if an X360-attested
//       member is later proven.
// ============================================================================

namespace ICE
{
    struct ICEWidgetWideScrMarker : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Draw the widescreen/letterbox marker quads. `lpRender` is the editor
        // render context the X360 passes in r4 and forwards to RenderPoly (mapped
        // to RenderPoly's localWorld/context slot, which the committed all-static
        // RenderPoly ignores). OWNED here (body in the .cpp). @0x82537B88.
        // ------------------------------------------------------------------
        void Render(ICERender* lpRender);
    };
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETWIDESCRMARKER_HPP
