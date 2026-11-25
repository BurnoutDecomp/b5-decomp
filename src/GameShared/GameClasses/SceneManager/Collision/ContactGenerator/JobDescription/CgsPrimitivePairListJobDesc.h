#pragma once

#include <new>

// CgsSceneManager::CgsCollision::PrimitivePairListJobDesc — collision contact-generator
// job descriptor for a single primitive-pair list (job-type id 10).
//
// DWARF (CgsPrimitivePairListJobDesc.h): derives from CollisionJobDescription and owns a
// nested Data { PrimitivePairList mPrimitivePairList; }. Recovered function:
//   Prepare @0x82810478 — store the pair list + results list into the descriptor and
//   assert the pair-list base pointer is 16-byte aligned.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitivePairListJobDesc : public CollisionJobDescription
    {
        // Nested payload (DWARF CgsPrimitivePairListJobDesc.h:73). The X360 places it at
        // the FRONT of the object (offsets 0x00..0x0C); modelled as a named member.

        // Prepare @0x82810478 — returns true (the X360 `return 1;`).
        bool Prepare(const PrimitivePairList* lpPairList,
                     CollisionResultList*     lpResultsList);

        const PrimitivePairList& GetPrimativePairList() const { return GetData()->mPrimitivePairList; }
    private:
        struct Data
        {
            PrimitivePairList mPrimitivePairList;
        };

        Data* GetData() { return reinterpret_cast<Data*>(maData); }
        const Data* GetData() const { return reinterpret_cast<const Data*>(maData); }
        CGS_STATIC_ASSERT(sizeof(Data) <= KU_MAX_DESC_DATA_SIZE, "PrimitivePairListJobDesc payload size");
    };
}
}
