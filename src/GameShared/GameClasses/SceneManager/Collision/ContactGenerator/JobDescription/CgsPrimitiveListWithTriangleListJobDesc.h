#pragma once

// CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListJobDesc — collision
// contact-generator job descriptor for a primitive-pair list vs triangle-list test
// (job-type id 11).
//
// DWARF (CgsPrimitiveListWithTriangleListJobDesc.h): derives from CollisionJobDescription
// and owns Data { PrimitivePairList mPrimitivePairList; TriangleList mTriangleList;
// bool mbUseOptimizedBoxTests; }. Recovered function:
//   Prepare @0x82810278 — store the pair list + triangle list + results list + the
//   use-optimized-box-tests flag into the descriptor, then assert the pair-list base is
//   16-byte aligned and the pair-list size rounds to a multiple of 16.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitiveListWithTriangleListJobDesc : public CollisionJobDescription
    {
        // Nested payload (DWARF CgsPrimitiveListWithTriangleListJobDesc.h:90), placed by
        // the X360 at the front of the object (pair list @0x00, triangle list @0x0C,
        // flag @0x14); modelled as named members.
        PrimitivePairList mPrimitivePairList;    // +0x00
        TriangleList      mTriangleList;         // +0x0C
        bool              mbUseOptimizedBoxTests; // +0x14

        // Prepare @0x82810278 — returns true (the X360 `return 1;`).
        bool Prepare(const PrimitivePairList* lpPairList,
                     const TriangleList*      lpTriangleList,
                     CollisionResultList*     lpResultsList,
                     bool                     lbUseOptimizedBoxTests);
    };
}
}
