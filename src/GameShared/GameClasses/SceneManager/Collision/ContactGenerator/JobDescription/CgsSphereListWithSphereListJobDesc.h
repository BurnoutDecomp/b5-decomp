#pragma once

// CgsSceneManager::CgsCollision::SphereListWithSphereListJobDesc — collision
// contact-generator job descriptor for a sphere-list vs sphere-list test (job-type id 7).
//
// DWARF (CgsSphereListWithSphereListJobDesc.h): derives from CollisionJobDescription and
// owns Data { SphereList mSphereListA; SphereList mSphereListB; }. Recovered function:
//   Prepare @0x82810198 — store both sphere lists + results list + a radius/padding float
//   into the descriptor and assert both sphere base pointers are 16-byte aligned.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSphereList.h"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SphereListWithSphereListJobDesc : public CollisionJobDescription
    {
        // Nested payload (DWARF CgsSphereListWithSphereListJobDesc.h:88), placed by the
        // X360 at the front of the object (A @0x00, B @0x08); modelled as named members.
        SphereList mSphereListA;  // +0x00
        SphereList mSphereListB;  // +0x08

        // Prepare @0x82810198 — returns true (the X360 `return 1;`). lfPadding is the
        // radius/padding stored at +0xF4 (f1 -> stfs).
        bool Prepare(const SphereList*    lpSphereListA,
                     const SphereList*    lpSphereListB,
                     CollisionResultList* lpResultsList,
                     f32                  lfPadding);
    };
}
}
