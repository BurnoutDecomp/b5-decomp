#ifndef CGS_TRIANGLE_H
#define CGS_TRIANGLE_H

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h
//
// CgsGeometric::Triangle -- three vertices by value (declared `Vector3 maVertices[3]`,
// 48 bytes). TriangleArg (`const Triangle &`) is what
// IntersectTriangleSweptSphere takes; it reads the vertices at +0x00 / +0x10 /
// +0x20 and forms the two edges with vsubfp128, which is all of the class it
// uses. The console keeps no out-of-line member, so only the accessors that body
// inlines are provided here.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3
#include "rw/math/vpu/vector3_operation.h"

namespace CgsGeometric
{
    struct Triangle
    {
    public:
        // Set(Vector3, Vector3, Vector3) -- DecFIGS CgsTriangle.h:49. The
        // three vertex stores are the record copied to IntersectTriangleBox.
        void Set(Vector3 lVertex0, Vector3 lVertex1, Vector3 lVertex2)
        {
            maVertices[0] = lVertex0;
            maVertices[1] = lVertex1;
            maVertices[2] = lVertex2;
        }

        Vector3 Get(s32 liIndex) const { return maVertices[liIndex]; }

        // Vertex 1 minus vertex 0, all four lanes.
        Vector3 CalcEdge01() const
        {
            return rw::math::vpu::Subtract(maVertices[1], maVertices[0]);
        }

        // Vertex 2 minus vertex 1, all four lanes.
        Vector3 CalcEdge12() const
        {
            return rw::math::vpu::Subtract(maVertices[2], maVertices[1]);
        }

    private:
        Vector3 maVertices[3];
    };
}

#endif // CGS_TRIANGLE_H
