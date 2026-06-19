#ifndef SDKS_PACKAGES_ICE_ICERENDER_HPP
#define SDKS_PACKAGES_ICE_ICERENDER_HPP

#include "types.hpp"
#include <cstdarg>                              // va_list (ScrPrintfArg's forwarded args)
#include "SDKs/Packages/ICE/ICEMath.hpp"        // ICE::Vector4 / ICE::Matrix4 (param types)

// ============================================================================
// SDKs/Packages/ICE/ICERender.hpp
//
// ICE::ICERender -- the ICE camera-take system's debug-overlay drawing surface: a
// pure all-static utility (the X360 funcs take no `this`, they only marshal their
// args into the engine debug renderer). It draws the editor's 2D debug boxes and
// the on-screen formatted text the camera-take editor overlays, routed through the
// process-wide CgsDev debug-render subsystem (DebugInterface -> Get2dRender ->
// DebugRender::Draw2D{Box,Text}).
//
// **MINIMAL, X360-GATED.** The DecFIGS PS3 DWARF (ICERender.hpp) lists a wider
// surface -- screenshot control (StartScreenShot / IsScreenShotDone / ...), camera
// queries (IsWidescreen / IsCameraPaused), and extra RenderPoly/ScrPrintf overloads.
// Only the FOUR functions the X360 ARTIST ledger attests for this TU are declared
// here (RenderPoly(Poly*,V4*,V4*,Matrix4*) / ScrPrintf / ScrShadowPrintf /
// ScrPrintfArg); the PS3-only members are left out (a using-TU GROWS this header
// when the X360 build attests them).
//
// `Poly` is ICE's debug triangle/quad: four projected vertices the box-draw path
// reads (DWARF ICERender.hpp:35 -- Vector3 Vertices[4]).
// ============================================================================

namespace ICE
{
    // The debug poly the box-render path draws around (four screen-projected verts).
    // RenderPoly reads the first vertex (the box origin) + a derived extent.
    struct Poly
    {
        Vector3 maVertices[4];
    };

    struct ICERender
    {
        // Format `lpcFormat` with the variadic args and draw the resulting text at
        // (liX, liY) at scale lfSize in WHITE (the ScrPrintfArg default colour).
        // The format param stays `const char*` (matches ICEFileHandler::FilePrintf;
        // /permissive- rejects passing a string literal to a `char*` vararg sink).
        static void ScrPrintf(s32 liX, s32 liY, f32 lfSize, const char* lpcFormat, ...);

        // Two-pass shadowed text: a dark/offset shadow pass then the white main pass.
        static void ScrShadowPrintf(s32 liX, s32 liY, f32 lfSize, const char* lpcFormat, ...);

        // The shared core: vsnprintf the args into a stack buffer and draw it as 2D
        // debug text in `lpColour`. `lrArgs` is the caller's started va_list.
        static void ScrPrintfArg(s32 liX, s32 liY, f32 lfSize, const Vector4* lpColour,
                                 const char* lpcFormat, va_list& lrArgs);

        // Draw a 2D debug box around `lpPoly` in a colour blended from the two colour
        // params, offset by the ICE menu shift. `lpLocalWorld` is the take->screen
        // transform context (unused by the box-draw maths, kept for the call shape).
        static void RenderPoly(const Poly* lpPoly, const Vector4* lpColour0,
                               const Vector4* lpColour1, const Matrix4* lpLocalWorld);
    };
}

#endif // SDKS_PACKAGES_ICE_ICERENDER_HPP
