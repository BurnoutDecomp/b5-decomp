// BrnMapUtils.h
// Home of BrnGui::MapTransform, the 2D sat-nav / mini-map coordinate-space helper.
// MapTransform is a namespace-like struct of STATIC functions over a set of static
// map-space rects and 3x3 transforms (world / normalised / device / zoomed spaces).
//
// Reconstructed out-of-line X360 functions (this TU):
//   MakeCoordSpaceFromRect @ 0x82450460  Matrix33 from a (minX,minY,maxX,maxY) rect
//   Transform (one-matrix) @ (inlined)   Vector2 = apply Matrix33 to a Vector2 point
//   Transform (from,to)    @ 0x824503C0  MakeTransform(from,to) then apply (H3b: the
//                                        X360 body at this address takes TWO matrix
//                                        pointers in r3/r4 + the point in v1 -- the
//                                        old single-matrix attribution was wrong)
//   MakeTransform          @ 0x824435C8  Matrix33 = inverse(from) composed with `to`
//   MakeCoordSpaceFromPoints @ (inlined) Matrix33 from origin + two axis points
//   SetZoomedWorldRect     @ 0x824504E8  3 zoomed-rect corners -> smv4ZoomedWorldRect
//                                        + smm33ZoomedWorldTransform (world -> zoomed-unit)
//   SetZoomedViewportRect  @ 0x82450608  viewport rect -> smm33ZoomedViewportTransform
//                                        + smm33ZoomedViewportScreenTransform
//   WorldToDevice          @ 0x82428878  world point -> zoomed-unit (optional direction-
//                                        preserving clamp) -> device space
//   DeviceToWorld          @ 0x824BAB78  Vector3 = device-point -> world-space point
//
// Sources:
//   * X360 BURNOUT_X360_ARTIST.XEX (binary). These functions are hand-vectorised
//     VMX128 (vmsum3fp128 dot products, vrefp + Newton-Raphson reciprocal, vperm
//     lane shuffles). Those intrinsics have NO portable PC equivalent, so the bodies
//     are reconstructed at the SEMANTIC level (faithful named-member matrix math that
//     produces the same result), not store-for-store -- consistent with the policy
//     already stated in rw/math/vpu/types.h ("the SIMD operations ... are not
//     reproduced here"). The project target is faithful, compilable C++, not a byte match.
//   * The static map-space VALUES are read off the image's static-init thunks
//     (H3b scratch h3b_dump.txt, cinit region 0x82C51F90..0x82C52480):
//       smv4WorldRect       @0x82FB31F0 = {-4375.42, -5842.42, 5363.15, 3904.74}
//       smv4NormalizedRect  @0x82FB3660 = {0, 0, 1, 1}
//       smv4DeviceRect      @0x82FB30C0 = {0, 0, 1280, 720}
//       smm33WorldSpace     @0x82FB3610, smm33NormalisedSpace @0x82FB2FA0,
//       smm33DeviceSpace    @0x82FB3050 = MakeCoordSpaceFromRect of each rect
//       smv4SatNavViewRect  @0x82FB36A0 (live) <- HD default @0x82FB30A0 =
//         {0.778125, 0.66527778, 0.93125, 0.86666667} (SD alt @0x82FB3130;
//         GuiModule::Construct @0x82518A2C picks by the isHighDef argument)
//       smv4ZoomedWorldRect @0x82FB3440, smm33ZoomedWorldTransform @0x82FB32E0,
//       smm33ZoomedViewportTransform @0x82FB3330, ViewportScreen @0x82FB3140
//     The old "numeric contents owned by an out-of-scope init" boundary is RETIRED.
//   * references/DecFIGS/dwarfdump/SharedClasses/Gui/SatNav/BrnMapUtils.h
//     (function signatures, static-member names, the World/Normalised/Device spaces).

#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                       // Vector2/Vector3/Vector4, Matrix33
#include "rw/math/vpu/types.h"                    // rw::math::vpu::{Vector4,Matrix33}

namespace BrnGui
{

// All members are static: MapTransform owns global map-space state, not per-instance.
struct MapTransform
{
    // ---- reconstructed (X360 out-of-line; see the header banner) ----
    static Matrix33 MakeCoordSpaceFromRect(Vector4 lv4Rect);              // @0x82450460
    static Vector2  Transform(Vector2 lv2Point, Matrix33 lm33Transform);  // (X360 inlines the one-matrix apply)
    static Vector2  Transform(Vector2 lv2Point, Matrix33 lm33From, Matrix33 lm33To); // @0x824503C0
    static Matrix33 MakeTransform(Matrix33 lm33From, Matrix33 lm33To);    // @0x824435C8
    static Matrix33 MakeCoordSpaceFromPoints(Vector2 lv2Origin, Vector2 lv2XPoint, Vector2 lv2YPoint); // (X360 inlines it; attested by SetZoomedWorldRect / SatNavRenderer::RenderComponent)
    static void     SetZoomedWorldRect(Vector2 lv2CornerA, Vector2 lv2CornerB, Vector2 lv2CornerC); // @0x824504E8
    static void     SetZoomedViewportRect(Vector4 lv4Rect);               // @0x82450608
    static Vector2  WorldToDevice(Vector3 lv3World, bool lbClamp);        // @0x82428878
    static Vector3  DeviceToWorld(Vector2 lv2Device);                     // @0x824BAB78
    static void     SetSatNavRect(Vector4 lv4Rect);                       // (X360 inlines the 16-byte store to 0x82FB36A0; GuiModule::Construct / SatNavDebugComponent)

    // ---- accessors over the named statics (X360 inlines every one) ----
    static const Vector4&  GetWorldRect()            { return smv4WorldRect; }
    static const Vector4&  GetNormalisedRect()       { return smv4NormalizedRect; }
    static const Vector4&  GetDeviceRect()           { return smv4DeviceRect; }
    static const Matrix33& GetWorldSpace()           { return smm33WorldSpace; }
    static const Matrix33& GetNormalisedSpace()      { return smm33NormalisedSpace; }
    static const Matrix33& GetDeviceSpace()          { return smm33DeviceSpace; }
    static const Vector4&  GetSatNavViewRect()       { return smv4SatNavViewRect; }
    static const Vector4&  GetZoomedWorldRect()      { return smv4ZoomedWorldRect; }
    static const Matrix33& GetZoomedWorldSpace()     { return smm33ZoomedWorldTransform; }
    static const Matrix33& GetZoomedViewport()       { return smm33ZoomedViewportTransform; }
    static const Matrix33& GetZoomedViewportScreen() { return smm33ZoomedViewportScreenTransform; }

    // ---- declared-only (DWARF :81-314), out of scope for this slice ----
    static Vector2  WorldToNormalised(Vector3 lv3World);
    static Vector2  Transform(Vector2 lv2Point, Vector4 lv4From, Vector4 lv4To);
    static Vector2  Flatten(Vector3 lv3In);
    static Vector3  Unflatten(Vector2 lv2In);
    static bool     IsWithinViewport(Vector3 lv3World, f32 lfMargin, f32 lfScale);
    static f32      CalculateZoomFactor(Vector2 lv2A, Vector2 lv2B, Vector2 lv2C, f32 lfBase);
    static void     SetIsHighDef(bool lbHighDef);
    static const Vector4& GetMainMapViewRect();

private:
    static Matrix33 MakeCoordSpaceFromAxes(Vector2 lv2X, Vector2 lv2Y, Vector2 lv2Origin);

    // ---- static map-space state (DWARF static members; values in the banner) ----
    static const Vector4  smv4WorldRect;       // @0x82FB31F0 fixed world-space rect
    static const Vector4  smv4NormalizedRect;  // @0x82FB3660 (0,0,1,1)
    static const Vector4  smv4DeviceRect;      // @0x82FB30C0 device/screen rect (1280x720)
    static const Matrix33 smm33WorldSpace;     // @0x82FB3610 coord space of smv4WorldRect
    static const Matrix33 smm33NormalisedSpace;// @0x82FB2FA0
    static const Matrix33 smm33DeviceSpace;    // @0x82FB3050

    // The live sat-nav on-screen viewport rect (@0x82FB36A0; runtime-writable -- the
    // debug component's sliders poke it on console). Defaults to the HD rect.
    static Vector4  smv4SatNavViewRect;

    static Vector4  smv4ZoomedWorldRect;             // @0x82FB3440 (SetZoomedWorldRect: {C.xy, B.xy})
    static Matrix33 smm33ZoomedWorldTransform;       // @0x82FB32E0 world -> zoomed-unit (INVERSE corner space)
    static Matrix33 smm33ZoomedViewportTransform;    // @0x82FB3330 zoomed-unit -> viewport
    static Matrix33 smm33ZoomedViewportScreenTransform; // @0x82FB3140 zoomed-unit -> device
};

} // namespace BrnGui
