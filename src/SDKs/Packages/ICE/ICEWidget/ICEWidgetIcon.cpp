// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.cpp
//
// ICE::ICEWidgetIcon::Render @0x82534478 -- the dev-only ICE camera-take editor
// icon widget's draw. It turns the icon's (mfX, mfY) position + (mfWidth,
// mfHeight) size into a 2D screen-space quad and draws it as a flat grey box
// through ICE::ICERender::RenderPoly.
//
// RECONSTRUCTION (from the X360 asm; there is no Feb-2007 source and no
// ICEWidgetIcon DWARF for this TU):
//
//   * GEOMETRY -- the asm reads four floats off `this`:
//       lfs f0, 0(r10)   -> mfX                (left  = X)
//       lfs f13, 4(r10)  -> mfY                (top   = Y)
//       lfs f12, 8(r10)  -> mfWidth ; fadds X  (right  = X + W)
//       lfs f11, 0xC(r10)-> mfHeight; fadds Y  (bottom = Y + H)
//     and builds the four quad corners (the X360 vrlimi128 lane shuffles compose
//     the (left/right) x (top/bottom) cross product into Poly::maVertices[0..3]):
//       V0 = (left,  top   )
//       V1 = (right, top   )
//       V2 = (right, bottom)   <- the opposite corner RenderPoly reads as the extent
//       V3 = (left,  bottom)
//     Each vertex's Z lane gets the constant 7.0 (flt_820054D0; the icon's fixed
//     2D depth/scale). RenderPoly only consumes V0 and V2 (corner + V2-V0 delta),
//     so this is the standard top-left-origin screen quad.
//
//   * COLOUR -- a flat grey at full alpha, (128, 128, 128, 255):
//       flt_820080E4 = 128.0 written to colour x/y/z (var_70/var_6C/var_68)
//       flt_82010C20 = 255.0 written to colour w     (var_64)
//     passed as RenderPoly's colour0 (and colour1; RenderPoly blends from colour0
//     only). Channels are in the [0,255] space RenderPoly clamp-packs to RGBA8.
//
//   * CALL -- ICE::ICERender::RenderPoly(poly, colour, colour, localWorld). The
//     committed RenderPoly is the all-static debug-draw helper; the icon's
//     `lpRender` editor-render context maps to the localWorld/context slot (the
//     X360 marshals it in r3 but the committed RenderPoly ignores localWorld, so
//     it is passed for call-shape parity). An identity Matrix4 stands in for the
//     take->screen transform the X360 leaves to the debug renderer.
//
// FLAG: ICEWidgetIcon field names/offsets, the grey-colour and 7.0-Z constants,
//       and the quad winding are all DERIVED FROM ASM (DWARF-sparse: the only ICE
//       widget DWARF is ICEWidgetColorPicker.hpp, which does not cover the icon).
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.hpp"
#include "SDKs/Packages/ICE/ICEMath.hpp"   // ICE::Matrix4 (the take->screen context slot)

namespace ICE
{
    // The icon's fixed 2D depth/scale lane, written into every quad vertex's Z
    // (flt_820054D0 in the X360 build).
    static const f32 KF_ICE_ICON_QUAD_Z = 7.0f;

    // The icon box colour: flat grey at full alpha, per-channel in [0,255]
    // (flt_820080E4 = 128.0 RGB, flt_82010C20 = 255.0 A).
    static const f32 KF_ICE_ICON_GREY  = 128.0f;
    static const f32 KF_ICE_ICON_ALPHA = 255.0f;

    // ------------------------------------------------------------------------
    // ICEWidgetIcon::Render @0x82534478
    // ------------------------------------------------------------------------
    void ICEWidgetIcon::Render(ICERender* lpRender)
    {
        // Screen-space box edges from position + size.
        const f32 lfLeft   = mfX;
        const f32 lfTop    = mfY;
        const f32 lfRight  = mfX + mfWidth;
        const f32 lfBottom = mfY + mfHeight;

        // The four quad corners (Z = the fixed icon depth lane). Wound so that V0
        // is the top-left origin and V2 is the opposite (bottom-right) corner --
        // RenderPoly reads exactly V0 and V2 to form the box.
        Poly lQuad;
        lQuad.maVertices[0] = { lfLeft,  lfTop,    KF_ICE_ICON_QUAD_Z, 0.0f }; // top-left
        lQuad.maVertices[1] = { lfRight, lfTop,    KF_ICE_ICON_QUAD_Z, 0.0f }; // top-right
        lQuad.maVertices[2] = { lfRight, lfBottom, KF_ICE_ICON_QUAD_Z, 0.0f }; // bottom-right
        lQuad.maVertices[3] = { lfLeft,  lfBottom, KF_ICE_ICON_QUAD_Z, 0.0f }; // bottom-left

        // Flat grey box colour (per-channel [0,255]); RenderPoly clamp-packs it.
        Vector4 lColour;
        lColour.x = KF_ICE_ICON_GREY;
        lColour.y = KF_ICE_ICON_GREY;
        lColour.z = KF_ICE_ICON_GREY;
        lColour.w = KF_ICE_ICON_ALPHA;

        // The take->screen context. The committed RenderPoly does not read it; an
        // identity transform stands in for the X360's editor-render context (carried
        // through `lpRender`, which RenderPoly's static box-draw path ignores).
        Matrix4 lLocalWorld;
        ICEMath::Identity(&lLocalWorld);
        (void)lpRender;   // editor-render context; RenderPoly's static box-draw ignores it

        ICERender::RenderPoly(&lQuad, &lColour, &lColour, &lLocalWorld);
    }
}
