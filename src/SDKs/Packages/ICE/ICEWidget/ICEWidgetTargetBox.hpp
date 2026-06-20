#ifndef SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETTARGETBOX_HPP
#define SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETTARGETBOX_HPP

#include "types.hpp"
#include <cstddef>                                       // offsetof
#include "SDKs/Packages/ICE/ICEWidget/ICEWidget.hpp"     // ICE::ICEWidget base
#include "rw/math/vpu/types.h"                            // rw::math::vpu::Matrix44Affine (the world transform @+0x10)
#include "SDKs/Packages/ICE/ICEMath.hpp"                 // ICE::Vector3 / Vector4
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface (3D GetRender)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"         // CgsDev::DebugRender::DrawBox (3D)

// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.hpp
//
// ICE::ICEWidgetTargetBox -- the dev-only ICE camera-take editor's "target box"
// widget: a small fixed-size 3D box (a reticle / bounding cube) drawn in WORLD
// space around the take's look-at / target point, so the editor can see where the
// camera is aiming. Unlike its 2D siblings (ICEWidgetIcon / ICEWidgetLogo, which
// draw screen-space quads through ICERender), this widget draws a 3D box through
// the CgsDev debug system's 3D renderer (DebugInterface::GetRender, NOT Get2dRender).
//
// Built like its siblings as a small flat struct embedded in the owning controller
// (ICEController::RenderTargetBox passes `this(controller) + 0x7170` as the widget),
// so it is NON-polymorphic -- see ICEWidgetIcon for the asm-derived-widget pattern.
//
// LAYOUT (Render passes the world transform at +0x10 to DrawBox):
//   <0x00..0x0F>   16-byte leading region -- the widget's pre-transform members,
//                  NOT attested by this TU (Render never reads it); modelled as
//                  padding so the proven transform offset is preserved.
//   mTransform @0x10  -- the world-to-screen transform the box is drawn IN. Render
//                  passes `this + 0x10` to DrawBox (asm `addi r31, r31, 0x10`). The
//                  caller ICEController::RenderTargetBox writes a 4-row transform
//                  here: row0->+0x10, row1->+0x20, row2->+0x30, and the transformed
//                  look-at translation (row3 / wAxis) -> +0x40. So the box is the
//                  fixed +/-0.05 cube placed at the take's look-at point in world space.
//
// FLAG: the widget layout (the transform at 0x10, a 16-byte leading pad) is DERIVED
//       FROM ASM (this Render @0x8252D2B8 passes only `this+0x10`; the caller
//       ICEController::RenderTargetBox @0x8253A008 stores the 4-row transform at
//       widget+0x10..+0x4F, look-at point at +0x40). There is NO ICEWidgetTargetBox
//       DWARF (the only ICE widget DWARF in DecFIGS is ICEWidgetColorPicker.hpp).
//       The offset is PINNED below with static_assert.
// ============================================================================

namespace ICE
{
    struct ICEWidgetTargetBox : public ICEWidget
    {
        // ------------------------------------------------------------------
        // Draw the fixed-size target box: build a stack DebugInterface, acquire
        // its 3D DebugRender, DrawBox a +/-0.05 cube in mTransform's space (i.e. at
        // the take's look-at point) in white, then thread-safe-release the manager.
        // OWNED here (body in the .cpp). @0x8252D2B8.
        // ------------------------------------------------------------------
        void Render();

        // --- layout (offset proven by the Render asm + the caller's matrix stores) ---
        u8                          mPad0[0x10];   // 0x00  leading region (caller-filled; unread here)
        rw::math::vpu::Matrix44Affine mTransform;  // 0x10  world transform (row3/wAxis @+0x40 = look-at point)
    };

    // Pin the offset the Render body passes to DrawBox (`addi r31, r31, 0x10`).
    // The base ICEWidget is empty (EBO), so the first member sits at offset 0.
    static_assert(offsetof(ICEWidgetTargetBox, mTransform) == 0x10,
                  "ICEWidgetTargetBox::mTransform offset");
}

#endif // SDKS_PACKAGES_ICE_ICEWIDGET_ICEWIDGETTARGETBOX_HPP
