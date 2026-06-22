// BrnMapUtils.h
// Home of BrnGui::MapTransform, the 2D sat-nav / mini-map coordinate-space helper.
// MapTransform is a namespace-like struct of STATIC functions over a set of static
// map-space rects and 3x3 transforms (world / normalised / device / zoomed spaces).
//
// This slice reconstructs the six out-of-line functions the X360 ARTIST build emits:
//   MakeCoordSpaceFromRect @ 0x82450460  Matrix33 from a (minX,minY,maxX,maxY) rect
//   Transform              @ 0x824503C0  Vector2 = apply Matrix33 to a Vector2 point
//   MakeTransform          @ 0x824435C8  Matrix33 = inverse(from) composed with `to`
//   SetZoomedWorldRect     @ 0x824504E8  writes smm33ZoomedWorldTransform from 3 points
//   SetZoomedViewportRect  @ 0x82450608  writes the zoomed viewport/screen transforms
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
    // ---- reconstructed (X360 out-of-line) ----
    static Matrix33 MakeCoordSpaceFromRect(Vector4 lv4Rect);              // @0x82450460
    static Vector2  Transform(Vector2 lv2Point, Matrix33 lm33Transform);  // @0x824503C0
    static Matrix33 MakeTransform(Matrix33 lm33From, Matrix33 lm33To);    // @0x824435C8
    static void     SetZoomedWorldRect(Vector2 lv2Min, Vector2 lv2Max, Vector2 lv2Centre); // @0x824504E8
    static void     SetZoomedViewportRect(Vector4 lv4Rect);              // @0x82450608
    static Vector3  DeviceToWorld(Vector2 lv2Device);                     // @0x824BAB78

    // ---- declared-only (DWARF :81-314), out of scope for this slice ----
    static Vector2  WorldToNormalised(Vector3 lv3World);
    static Vector2  WorldToDevice(Vector3 lv3World, bool lbClamp);
    static Vector2  Transform(Vector2 lv2Point, Vector4 lv4From, Vector4 lv4To);
    static Vector2  Transform(Vector2 lv2Point, Matrix33 lm33From, Matrix33 lm33To);
    static Matrix33 MakeCoordSpaceFromPoints(Vector2 lv2A, Vector2 lv2B, Vector2 lv2C);
    static Vector2  Flatten(Vector3 lv3In);
    static Vector3  Unflatten(Vector2 lv2In);
    static bool     IsWithinViewport(Vector3 lv3World, f32 lfMargin, f32 lfScale);
    static f32      CalculateZoomFactor(Vector2 lv2A, Vector2 lv2B, Vector2 lv2C, f32 lfBase);
    static void     SetIsHighDef(bool lbHighDef);
    static void     SetSatNavRect(Vector4 lv4Rect);

    static const Vector4&  GetZoomedWorldRect();
    static const Vector4&  GetWorldRect();
    static const Vector4&  GetNormalisedRect();
    static const Vector4&  GetDeviceRect();
    static const Matrix33& GetWorldSpace();
    static const Matrix33& GetNormalisedSpace();
    static const Matrix33& GetDeviceSpace();
    static const Vector4&  GetSatNavViewRect();
    static const Matrix33& GetZoomedWorldSpace();
    static const Matrix33& GetZoomedViewportScreen();
    static const Matrix33& GetZoomedViewport();
    static const Vector4&  GetMainMapViewRect();

private:
    static Matrix33 MakeCoordSpaceFromAxes(Vector2 lv2X, Vector2 lv2Y, Vector2 lv2Origin);

    // ---- static map-space state (DWARF static members; defined in the .cpp) ----
    static const Vector4  smv4WorldRect;       // fixed world-space rect
    static const Vector4  smv4NormalizedRect;  // (0,0,1,1)
    static const Vector4  smv4DeviceRect;      // device/screen rect
    static const Matrix33 smm33WorldSpace;     // coord space of smv4WorldRect
    static const Matrix33 smm33NormalisedSpace;
    static const Matrix33 smm33DeviceSpace;

    static Vector4  smv4ZoomedWorldRect;             // current zoomed world rect
    static Matrix33 smm33ZoomedWorldTransform;       // written by SetZoomedWorldRect
    static Matrix33 smm33ZoomedViewportTransform;    // written by SetZoomedViewportRect
    static Matrix33 smm33ZoomedViewportScreenTransform;
};

} // namespace BrnGui
