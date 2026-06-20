#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETICON_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETICON_HPP

#include "types.hpp"
#include <cstddef>                                       // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base
#include "SDKs/Packages/ICE/ICERender.hpp"               // ICE::ICERender / Poly / Vector4 / Matrix4

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.hpp
//
// ICE::ICEWidgetIcon -- the dev-only ICE camera-take editor's icon widget. A tiny
// rectangle drawn as a 2D quad: position (mfX, mfY) + size (mfWidth, mfHeight),
// rendered through ICE::ICERender::RenderPoly as a flat grey box. Built ad-hoc on
// the caller's stack (ICEWidgetMenu::Render / ICEWidgetTimeBar::Render fill the
// four floats then call Render directly), so it is a small flat struct, NOT a
// polymorphic object.
//
// LAYOUT (all four offsets PROVEN by the Render asm @0x82534478):
//   mfX      @0x00  -- `lfs f0,  0(r10)`  (left edge, screen X)
//   mfY      @0x04  -- `lfs f13, 4(r10)`  (top edge, screen Y)
//   mfWidth  @0x08  -- `lfs f12, 8(r10)`  (right = mfX + mfWidth)
//   mfHeight @0x0C  -- `lfs f11, 0xC(r10)`(bottom = mfY + mfHeight)
// The caller ICEWidgetMenu::Render corroborates: it writes the stack icon's +0,
// +4, +12 lanes (`v112=i; v113=v95; v115=18.0`) before calling Render.
//
// FLAG: the four members (names mfX/mfY/mfWidth/mfHeight, all f32, at the offsets
//       above) are DERIVED from the Render asm field accesses + the caller's
//       stack writes. There is NO ICEWidgetIcon DWARF (only ICEWidgetColorPicker
//       is in DecFIGS); the names follow the project Hungarian convention and the
//       ColorPicker DWARF's mfX/mfY precedent. Offsets are PINNED below.
// ============================================================================

namespace ICE
{
    struct ICEWidgetIcon : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Draw the icon as a 2D grey quad through the ICE debug renderer.
        // OWNED here (body in ICEWidgetIcon.cpp). @0x82534478.
        // ------------------------------------------------------------------
        void Render(ICERender* lpRender);

        // --- layout (offsets proven by the Render asm) --------------------
        f32 mfX;        // @0x00  screen-space left edge
        f32 mfY;        // @0x04  screen-space top edge
        f32 mfWidth;    // @0x08  width  (right  = mfX + mfWidth)
        f32 mfHeight;   // @0x0C  height (bottom = mfY + mfHeight)
    };

    // Pin the offsets the Render body reads (lfs at 0/4/8/0xC of `this`). The base
    // is empty (EBO), so mfX must land at offset 0.
    static_assert(offsetof(ICEWidgetIcon, mfX)      == 0x00, "ICEWidgetIcon::mfX offset");
    static_assert(offsetof(ICEWidgetIcon, mfY)      == 0x04, "ICEWidgetIcon::mfY offset");
    static_assert(offsetof(ICEWidgetIcon, mfWidth)  == 0x08, "ICEWidgetIcon::mfWidth offset");
    static_assert(offsetof(ICEWidgetIcon, mfHeight) == 0x0C, "ICEWidgetIcon::mfHeight offset");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETICON_HPP
