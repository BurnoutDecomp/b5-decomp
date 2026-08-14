#pragma once

// CgsSceneManager::CgsCollision::SweptSphereList — a contiguous list of swept (continuous
// collision) spheres: a base pointer to a packed SweptSphere array plus the count.
//
// ⭐ ADDED 2026-08-14 (walls leg 1). Shape verbatim from the DecFIGS DWARF
// (CgsSweptSphereList.h:46: `SweptSphere * mpaSweptSpheres; int32_t miNumSpheres;`), the
// exact sibling of the committed SphereList (CgsSphereList.h). The X360 collide-stream
// poster copies it as one 8-byte {ptr,count} pair (`ld r11, 0(r28)` in
// AddSweptSphereListWithTriangleListToStream @0x82811698), and the driver builds it on the
// stack from DeformationManager::GetSweptSpheresForCar's {array, count} return pair
// (DoRaceCarWorldContactGeneration @0x825EB2EC..0x825EB2F0).
//
// The base pointer follows the committed SphereList precedent (u8*, element layout owned by
// the producer/consumer pair): the elements are CgsGeometric::SweptSphere records
// (GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h — pointer-free, no widening).

#include "types.hpp"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SweptSphereList
    {
        u8* mpaSweptSpheres;  // +0x00  packed SweptSphere array (DWARF mpaSweptSpheres)
        s32 miNumSpheres;     // +0x04  number of swept spheres
    };
}
}
