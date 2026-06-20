#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETLOGO_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETLOGO_HPP

#include "types.hpp"
#include <cstddef>                                       // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base
#include "SDKs/Packages/ICE/ICEMath.hpp"                 // ICE::ICEMath::Sin / ICE::Angle
#include "SDKs/Packages/ICE/ICERender.hpp"               // ICE::ICERender::ScrShadowPrintf

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetLogo.hpp
//
// ICE::ICEWidgetLogo -- the dev-only ICE camera-take editor's animated "ICE" logo
// widget. It draws the literal text "ICE" as a shadowed on-screen string whose
// vertical span pulses/wobbles each frame: a per-frame timer drives a full Sin
// cycle (one second at the ICE 60-step rate) and the Sin oscillation modulates the
// logo's drawn height. Built like its siblings as a small flat struct on the
// caller's stack (ICEController::Render fills it and calls Render directly), so it
// is NON-polymorphic -- see ICEWidgetIcon for the asm-derived-widget pattern.
//
// LAYOUT (all offsets PROVEN by the Render asm @0x825346E0):
//   mfX      @0x00  -- `lfs f13, 0(r31)`  (screen X; converted to int for the draw)
//   mfY      @0x04  -- `lfs f12, 4(r31)`  (screen Y; the pulsed text height anchor)
//   miTimer  @0x08  -- `lwz r11, 8(r31)`  (animation phase, advanced each frame and
//                                          wrapped: `*(this+8) = (timer + 1) % 60`)
// The struct is a flat 12 bytes; the base ICEWidget is empty (EBO), so mfX is at 0.
//
// FLAG: the three members (names mfX/mfY/miTimer, types f32/f32/s32, at the offsets
//       above) are DERIVED FROM ASM. There is NO ICEWidgetLogo DWARF (the only ICE
//       widget DWARF in DecFIGS is ICEWidgetColorPicker.hpp). Names follow the
//       project Hungarian convention + the ICEWidgetIcon/ColorPicker mfX/mfY
//       precedent. Offsets are PINNED below with static_assert.
// ============================================================================

namespace ICE
{
    struct ICEWidgetLogo : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Draw the animated "ICE" logo. `liScreenX` is the caller-supplied base
        // X (r4 in the X360 call from ICEController::Render). OWNED here (body in
        // ICEWidgetLogo.cpp). @0x825346E0.
        // ------------------------------------------------------------------
        void Render(s32 liScreenX);

        // --- layout (offsets proven by the Render asm) --------------------
        f32 mfX;        // @0x00  screen-space X
        f32 mfY;        // @0x04  screen-space Y (the pulsed text-height anchor)
        s32 miTimer;    // @0x08  animation phase counter (advanced + wrapped at 60)
    };

    // Pin the offsets the Render body reads (`lfs 0/4`, `lwz 8` off `this`). The
    // base is empty (EBO), so mfX must land at offset 0.
    static_assert(offsetof(ICEWidgetLogo, mfX)     == 0x00, "ICEWidgetLogo::mfX offset");
    static_assert(offsetof(ICEWidgetLogo, mfY)     == 0x04, "ICEWidgetLogo::mfY offset");
    static_assert(offsetof(ICEWidgetLogo, miTimer) == 0x08, "ICEWidgetLogo::miTimer offset");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETLOGO_HPP
