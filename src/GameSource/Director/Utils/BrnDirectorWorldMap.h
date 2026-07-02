#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu)

// BrnDirector::WorldMap - the director's queryable world map (safe camera
// positions, road topology, ...). DWARF home BrnDirectorWorldMap.h. FLAG: minimal
// slice -- only the surface the in-scope camera utilities consume is declared;
// the class's own TU (BrnDirectorWorldMap.cpp) owns the full shape and bodies.
namespace BrnDirector
{
    class WorldMap
    {
    public:
        // DWARF BrnDirectorWorldMap.h:80 (body BrnDirectorWorldMap.cpp:426, its own
        // ledger function; declaration-only here). Find the nearest safe position to
        // lPosition along lDisplacement; true on success with the result in lOut.
        // X360 ABI at the PositionFinder::Update call site @0x8223FD50: r3 = this,
        // r4 = &lOut, v1/v2 = the two vectors.
        bool GetSafePositionNearestPointWithDisplacement(Vector3 lPosition,
                                                         Vector3 lDisplacement,
                                                         Vector3& lOut) const;
    };
}
