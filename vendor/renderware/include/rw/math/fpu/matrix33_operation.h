#pragma once

// Portable PC reconstruction of the RenderWare rwmath scalar (FPU) 3x3-matrix helpers
// (EARenderWare rwmath 1.02.00, rw/math/fpu/*). The console SDK spells these
// `rw::math::fpu::Matrix33FromYRotationAngle<float>` and `rw::math::fpu::Mult<float>` at the
// call sites; the X360 image emits them out-of-line (the mangled names
// ?...Matrix33FromYRotationAngle... / ?...Mult... over Matrix33Template<float>). No committed
// home existed for them, so they are reconstructed here store-for-store against their observable
// behaviour at the one attested caller, BrnSkyDomeManager::CreateGeometry @0x824076D8:
//
//   * Matrix33FromYRotationAngle(out, angle) fills a column-major 3x3 rotation about +Y and
//     returns the buffer. The caller applies the result as N = M * (cos, sin, 0); with the
//     identity matrix that yields N = (cos, sin, 0) -- a meridian in the XY plane sweeping from
//     +X (horizon) to +Y (zenith) -- and each Mult by the per-sector Y-rotation sweeps that
//     meridian around +Y, tracing the sky dome. The column-major layout and the +Y axis are the
//     facts the caller's vertex maths pins (M[0]*cos + M[3]*sin for x, M[1]/M[4] for y,
//     M[2]/M[5] for z, with the third column multiplied by the vector's zero z lane).
//   * Mult(result, a, b) is the plain 3x3 * 3x3 product (result = a * b) in the same
//     column-major convention; the caller does new_current = rotSector * current each sector.
//
// ADDITIVE GROW: first reconstruction of the fpu 3x3-matrix ops; the fpu scalar home
// (scalar_operation.h) is untouched. Templated so only the instantiations the game references
// are emitted.

#include <cmath>

namespace rw
{
namespace math
{
namespace fpu
{
    // Build a column-major Y-rotation matrix (9 elements) into lpOut; returns lpOut.
    // Column 0 = (cos, 0, -sin), column 1 = (0, 1, 0), column 2 = (sin, 0, cos).
    template <typename T>
    inline T* Matrix33FromYRotationAngle(T* lpOut, T lfAngle)
    {
        const T lfCos = static_cast<T>(std::cos(lfAngle));
        const T lfSin = static_cast<T>(std::sin(lfAngle));
        lpOut[0] = lfCos;  lpOut[1] = static_cast<T>(0);  lpOut[2] = -lfSin;
        lpOut[3] = static_cast<T>(0);  lpOut[4] = static_cast<T>(1);  lpOut[5] = static_cast<T>(0);
        lpOut[6] = lfSin;  lpOut[7] = static_cast<T>(0);  lpOut[8] = lfCos;
        return lpOut;
    }

    // Column-major 3x3 product: lpResult = lpA * lpB. Returns lpResult.
    // result[col*3 + row] = sum_k A[k*3 + row] * B[col*3 + k].
    template <typename T>
    inline T* Mult(T* lpResult, const T* lpA, const T* lpB)
    {
        T laTemp[9];
        for (int liCol = 0; liCol < 3; ++liCol)
        {
            for (int liRow = 0; liRow < 3; ++liRow)
            {
                laTemp[liCol * 3 + liRow] =
                    lpA[0 * 3 + liRow] * lpB[liCol * 3 + 0]
                    + lpA[1 * 3 + liRow] * lpB[liCol * 3 + 1]
                    + lpA[2 * 3 + liRow] * lpB[liCol * 3 + 2];
            }
        }
        for (int liIndex = 0; liIndex < 9; ++liIndex)
        {
            lpResult[liIndex] = laTemp[liIndex];
        }
        return lpResult;
    }
}
}
}
