// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.cpp
//
// ICE::ICEWidgetWideScrMarker::Render @0x82537B88 -- the dev-only ICE camera-take
// editor's WIDESCREEN / letterbox marker draw. It builds TWO screen-space marker
// quads from inline constants and pushes each through ICE::ICERender::RenderPoly
// (two RenderPoly calls). RenderPoly consumes vertex 0 (the corner, +menu shift)
// and vertex 2 (the opposite corner, for the V2-V0 extent), so each quad is a
// box spanning V0..V2.
//
// RECONSTRUCTION (from the X360 asm; there is no Feb-2007 source and no
// ICEWidgetWideScrMarker DWARF):
//
//   * NO `this` ACCESS -- the Render body never dereferences r3(`this`); every
//     coordinate and the colour is an inline rodata constant. The only
//     register-passed input is the render context (r4), forwarded to RenderPoly.
//     The widget is therefore an empty struct (see the .hpp).
//
//   * GEOMETRY -- the X360 composes the quad vertices from inline `flt_*` scalars
//     via vrlimi128 lane merges. Simulating that lane dataflow gives the exact
//     four (x, y, z) vertices per quad (w lane unused by RenderPoly):
//
//       QUAD 0 (z = 1.0):
//         V0 = (-2.0, 480.0, 1.0)   V1 = ( 0.0, 480.0, 1.0)
//         V2 = ( 0.0,   0.0, 1.0)   V3 = (-2.0,   0.0, 1.0)
//         -> RenderPoly corner V0 = (-2, 480), extent V2-V0 = (+2, -480):
//            a 2-wide x 480-tall marker rectangle down the left edge.
//
//       QUAD 1 (z = 0.0):
//         V0 = (480.0,   0.0, 0.0)  V1 = (  1.0,   0.0, 0.0)
//         V2 = (  1.0, 480.0, 0.0)  V3 = (480.0, 480.0, 0.0)
//         -> RenderPoly corner V0 = (480, 0), extent V2-V0 = (-479, +480):
//            the complementary marker rectangle spanning x 1..480, y 0..480.
//
//     The constants: flt_82006D70 = -2.0, flt_82001CC0 = 0.0, flt_82056DC0 /
//     flt_8207B974 = 480.0, flt_82001C98 / flt_82057930 = 1.0, and
//     flt_8201A1F0 = 200.0 for the colour alpha.
//
//   * COLOUR -- the X360 passes {0, 0, 0, 200} as RenderPoly's colour vector.
//     RenderPoly clamp-packs colour0 to RGBA8, so the marker is black with alpha
//     200. The -2.0 constant supplies the first quad's left edge, not its colour.
//
//   * CALLS -- ICE::ICERender::RenderPoly(poly, colour, colour, localWorld) twice.
//     The X360 marshals the render context (r4) as RenderPoly's first arg; the
//     committed all-static RenderPoly drops the context, so `lpRender` is mapped
//     to the localWorld/context slot (which RenderPoly ignores) for call-shape
//     parity, exactly as ICEWidgetIcon::Render does.
//
// FLAG: the empty layout, every quad vertex/colour constant, and the two-call
//       structure are DERIVED FROM ASM (DWARF-sparse: the only ICE widget DWARF is
//       ICEWidgetColorPicker.hpp, which does not cover this widget). The quad lanes
//       were recovered by simulating the X360 vrlimi128/vspltw composition.
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.hpp"
#include "SDKs/Packages/ICE/ICEMath.hpp"   // ICE::Matrix4 (the take->screen context slot)

namespace ICE
{
    // RenderPoly's colour vector alpha (flt_8201A1F0).  FLAG: asm-derived constant.
    static const f32 KF_ICE_WIDESCR_COLOUR_ALPHA = 200.0f;

    // The two screen-space marker quads, with the exact vertex lanes recovered from
    // the X360 vrlimi128 lane composition (z lane carries the per-quad depth; the
    // w lane is unused by RenderPoly).  FLAG: all values asm-derived inline rodata.
    static const f32 KF_ICE_WIDESCR_X_LEFT   = -2.0f;   // flt_82006D70
    static const f32 KF_ICE_WIDESCR_X_ZERO   = 0.0f;    // flt_82001CC0
    static const f32 KF_ICE_WIDESCR_X_ONE    = 1.0f;    // flt_82057930 (quad 1)
    static const f32 KF_ICE_WIDESCR_EXTENT   = 480.0f;  // flt_82056DC0 / flt_8207B974
    static const f32 KF_ICE_WIDESCR_Z_NEAR   = 1.0f;    // flt_82001C98 (quad 0 depth)
    static const f32 KF_ICE_WIDESCR_Z_FAR    = 0.0f;    // (quad 1 depth)

    // ------------------------------------------------------------------------
    // ICEWidgetWideScrMarker::Render @0x82537B88
    // ------------------------------------------------------------------------
    void ICEWidgetWideScrMarker::Render(ICERender* lpRender)
    {
        // The X360's {0, 0, 0, 200} marker colour, with every RGBA lane initialized.
        Vector4 lColour(0.0f, 0.0f, 0.0f, KF_ICE_WIDESCR_COLOUR_ALPHA);

        // The take->screen context. The committed RenderPoly does not read it; an
        // identity transform stands in for the X360's editor-render context carried
        // through `lpRender` (the X360 passes r4 as RenderPoly's first arg).
        Matrix4 lLocalWorld;
        ICEMath::Identity(&lLocalWorld);
        (void)lpRender;   // editor-render context; RenderPoly's static path ignores it

        // --- Quad 0 (depth 1.0): left-edge marker rectangle ------------------
        // V0 corner = (-2, 480), V2 opposite = (0, 0); V1/V3 close the box.
        Poly lQuad0;
        lQuad0.maVertices[0] = Vector3(KF_ICE_WIDESCR_X_LEFT, KF_ICE_WIDESCR_EXTENT, KF_ICE_WIDESCR_Z_NEAR);
        lQuad0.maVertices[1] = Vector3(KF_ICE_WIDESCR_X_ZERO, KF_ICE_WIDESCR_EXTENT, KF_ICE_WIDESCR_Z_NEAR);
        lQuad0.maVertices[2] = Vector3(KF_ICE_WIDESCR_X_ZERO, KF_ICE_WIDESCR_X_ZERO, KF_ICE_WIDESCR_Z_NEAR);
        lQuad0.maVertices[3] = Vector3(KF_ICE_WIDESCR_X_LEFT, KF_ICE_WIDESCR_X_ZERO, KF_ICE_WIDESCR_Z_NEAR);
        ICERender::RenderPoly(&lQuad0, &lColour, &lColour, &lLocalWorld);

        // --- Quad 1 (depth 0.0): complementary marker rectangle --------------
        // V0 corner = (480, 0), V2 opposite = (1, 480); V1/V3 close the box.
        Poly lQuad1;
        lQuad1.maVertices[0] = Vector3(KF_ICE_WIDESCR_EXTENT, KF_ICE_WIDESCR_X_ZERO, KF_ICE_WIDESCR_Z_FAR);
        lQuad1.maVertices[1] = Vector3(KF_ICE_WIDESCR_X_ONE, KF_ICE_WIDESCR_X_ZERO, KF_ICE_WIDESCR_Z_FAR);
        lQuad1.maVertices[2] = Vector3(KF_ICE_WIDESCR_X_ONE, KF_ICE_WIDESCR_EXTENT, KF_ICE_WIDESCR_Z_FAR);
        lQuad1.maVertices[3] = Vector3(KF_ICE_WIDESCR_EXTENT, KF_ICE_WIDESCR_EXTENT, KF_ICE_WIDESCR_Z_FAR);
        ICERender::RenderPoly(&lQuad1, &lColour, &lColour, &lLocalWorld);
    }
}
