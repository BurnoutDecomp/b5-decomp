#pragma once

// CgsSceneManager::CgsCollision::SphereListWithTriangleListJobDesc — collision
// contact-generator job descriptor for a sphere-list vs triangle-list test
// (job-type id 5).
//
// DWARF (CgsSphereListWithTriangleListJobDesc.h): derives from CollisionJobDescription
// and owns Data { SphereList mSphereList; TriangleList mTriangleList; }. Recovered fn:
//   Prepare @0x82810100 — store the sphere list + triangle list + results list + a
//   radius/padding float into the descriptor and assert the sphere base pointer is
//   16-byte aligned.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSphereList.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SphereListWithTriangleListJobDesc : public CollisionJobDescription
    {
        // Nested payload (DWARF CgsSphereListWithTriangleListJobDesc.h:89), placed by the
        // X360 at the front of the object (sphere @0x00, triangle @0x08); named members.
        SphereList   mSphereList;    // +0x00
        TriangleList mTriangleList;  // +0x08

        // Prepare @0x82810100 — returns true (the X360 `return 1;`). lfPadding is the
        // radius/padding stored at +0xF4 (f1 -> stfs).
        bool Prepare(const SphereList*    lpSphereList,
                     const TriangleList*  lpTriangleList,
                     CollisionResultList* lpResultsList,
                     f32                  lfPadding);
    };
}
}
