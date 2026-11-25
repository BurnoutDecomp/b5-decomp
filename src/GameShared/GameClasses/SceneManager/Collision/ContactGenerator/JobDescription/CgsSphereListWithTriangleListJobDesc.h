#pragma once

#include <new>

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

// Stream producer (pointer-only use in the stream descriptor below).
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SphereListWithTriangleListJobDesc : public CollisionJobDescription
    {
        // Nested payload (DWARF CgsSphereListWithTriangleListJobDesc.h:89), placed by the
        // X360 at the front of the object (sphere @0x00, triangle @0x08); named members.

        // Prepare @0x82810100 — returns true (the X360 `return 1;`). lfPadding is the
        // radius/padding stored at +0xF4 (f1 -> stfs).
        bool Prepare(const SphereList*    lpSphereList,
                     const TriangleList*  lpTriangleList,
                     CollisionResultList* lpResultsList,
                     f32                  lfPadding);

        const SphereList& GetSphereList() const { return GetData()->mSphereList; }
        const TriangleList& GetTriangleList() const { return GetData()->mTriangleList; }
    private:
        struct Data
        {
            SphereList mSphereList;
            TriangleList mTriangleList;
        };

        Data* GetData() { return reinterpret_cast<Data*>(maData); }
        const Data* GetData() const { return reinterpret_cast<const Data*>(maData); }
        CGS_STATIC_ASSERT(sizeof(Data) <= KU_MAX_DESC_DATA_SIZE, "SphereListWithTriangleListJobDesc payload size");
    };

    // =============================================================================================
    // ⭐ ADDED 2026-08-14 (walls leg 1): the STREAM sibling (job-type id 6), DWARF-homed in this
    // same header (CgsSphereListWithTriangleListJobDesc.h:105). This is the descriptor
    // BaseCollisionGenerator::RunCollideSphereListWithTriangleListStream @0x82811550 prepares
    // (inlined stores: +0x00 producer / +0xF4 0.0f / +0xF8 the debug reader / +0xFF type 6) and
    // ContactGeneratorJob::ExecuteSphereListWithTriangleListStream @0x829235C8 will consume.
    //
    // StreamCommand is DWARF-verbatim (h:109-114) and is EXACTLY what the poster
    // AddSphereListWithTriangleListToStream @0x82811340 writes: the two 8-byte console
    // {ptr,count} list pairs (`ld`/`std`), the padding float, and the CollisionResultList the
    // poster just allocated via PrepareNewPrimitiveTestResultsList.
    //
    // ⚠️⚠️ THE COMMAND SIZE IS A CONTRACT, AND ON THIS HOST IT WIDENS. The console constructs
    // the stream with miCommandSize == 32 (`li r4, 0x20` in the Create* factory); the host
    // record's two pointers widen 4 -> 8 so sizeof == 48. These commands are RUNTIME-CARVED
    // (never serialized), so per the standing rule they widen, and the Create* factory passes
    // sizeof(StreamCommand) -- poster and consumer stride by the same host number. The poster
    // asserts commandSize % 16 == 0 (DataStreamCommandPoster::Construct :52); gated below.
    // =============================================================================================
    struct SphereListWithTriangleListStreamJobDesc : public CollisionJobDescription
    {
        // ARTIST stream factories pass a 32-byte, 16-aligned command.
        struct CGS_ALIGN(16) StreamCommand
        {
            SphereList           mSphereList;   // DWARF h:111 (console 8B pair; host 16)
            TriangleList         mTriList;      // DWARF h:112 (console 8B pair; host 16)
            f32                  mfPadding;     // DWARF h:113 (2.0f on the race-car world path)
            CollisionResultList* mpResultList;  // DWARF h:114 (the poster's fresh result list)
        };

        // X360 +0x00, the only member of the derived part (DWARF Data h:138-140).

        CgsMemory::SimpleDataStreamProducer* GetStreamProducer() const { return GetData()->mpStreamProducer; }

        // DWARF h:127 `bool Prepare(SimpleDataStreamProducer*, DebugRenderStreamReader*)`.
        // The X360 inlines it into the Run dispatcher; named members, same shape as the
        // committed LineWithTriangleListStreamJobDesc::Prepare.
        bool Prepare(CgsMemory::SimpleDataStreamProducer* lpStreamProducer,
                     CgsDev::DebugRenderStreamReader*     lpDebugReader)
        {
            new (maData) Data;
            GetData()->mpStreamProducer = lpStreamProducer;   // 0x828115F8  stw r23, 0x3D0(batch)

            return CollisionJobDescription::Prepare(E_COLLISIONJOB_SPHERE_LIST_WITH_TRIANGLE_LIST_STREAM, 0, 0.0f,
                                                    lpDebugReader);
        }

    private:
        struct Data
        {
            CgsMemory::SimpleDataStreamProducer* mpStreamProducer;
        };

        Data* GetData() { return reinterpret_cast<Data*>(maData); }
        const Data* GetData() const { return reinterpret_cast<const Data*>(maData); }
        CGS_STATIC_ASSERT(sizeof(Data) <= KU_MAX_DESC_DATA_SIZE, "SphereListWithTriangleListStreamJobDesc payload size");
    };

    CGS_STATIC_ASSERT((sizeof(SphereListWithTriangleListStreamJobDesc::StreamCommand) % 16 == 0),
                  "collide-stream command size must stay a multiple of 16 "
                  "(DataStreamCommandPoster::Construct tripwire :52)");
}
}
