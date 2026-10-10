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

namespace CgsGeometric
{
    struct Triangle
    {
    public:
        Vector3 Get(s32 liIndex) const { return maVertices[liIndex]; }

        // Vertex 1 minus vertex 0, all four lanes.
        Vector3 CalcEdge01() const
        {
            Vector3 lEdge;
            lEdge.x = maVertices[1].x - maVertices[0].x;
            lEdge.y = maVertices[1].y - maVertices[0].y;
            lEdge.z = maVertices[1].z - maVertices[0].z;
            lEdge.w = maVertices[1].w - maVertices[0].w;
            return lEdge;
        }

        // Vertex 2 minus vertex 1, all four lanes.
        Vector3 CalcEdge12() const
        {
            Vector3 lEdge;
            lEdge.x = maVertices[2].x - maVertices[1].x;
            lEdge.y = maVertices[2].y - maVertices[1].y;
            lEdge.z = maVertices[2].z - maVertices[1].z;
            lEdge.w = maVertices[2].w - maVertices[1].w;
            return lEdge;
        }

    private:
        Vector3 maVertices[3];
    };
}

#endif // CGS_TRIANGLE_H
