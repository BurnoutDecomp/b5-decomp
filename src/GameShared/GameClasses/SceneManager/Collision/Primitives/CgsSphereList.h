#pragma once

// CgsSceneManager::CgsCollision::SphereList — a contiguous list of collision spheres:
// a 16-byte-aligned base pointer to a packed sphere array plus the count.
//
// Layout recovered from the descriptor Prepare() asm that copies it into a job desc
// (SphereListWithSphereListJobDesc::Prepare @0x82810198,
//  SphereListWithTriangleListJobDesc::Prepare @0x82810100). Two dwords are copied
// verbatim (0,4) and the base pointer at +0x00 is alignment-checked:
//   `*a1 % 16` must be 0 ("Spheres not aligned to 16 bytes" /
//    "Spheres A/B not aligned to 16 bytes").
//
//   +0x00  u8*  base pointer (16-byte-aligned packed sphere array, alignment-checked)
//   +0x04  s32  sphere count
//
// The per-sphere element layout is not attested by these copy/validate paths, so only
// the asm-observed header fields are named.

#include "types.hpp"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SphereList
    {
        u8* mpSpheres;     // +0x00  16-byte-aligned packed sphere array (alignment-checked)
        s32 miNumSpheres;  // +0x04  number of spheres
    };
}
}
