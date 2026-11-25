#pragma once

#include <new>

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

// Stream producer (pointer-only use in the stream descriptor below).
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SphereListWithSphereListJobDesc : public CollisionJobDescription
    {
        // Nested payload (DWARF CgsSphereListWithSphereListJobDesc.h:88), placed by the
        // X360 at the front of the object (A @0x00, B @0x08); modelled as named members.

        // Prepare @0x82810198 — returns true (the X360 `return 1;`). lfPadding is the
        // radius/padding stored at +0xF4 (f1 -> stfs).
        bool Prepare(const SphereList*    lpSphereListA,
                     const SphereList*    lpSphereListB,
                     CollisionResultList* lpResultsList,
                     f32                  lfPadding);

        const SphereList& GetSphereListA() const { return GetData()->mSphereListA; }
        const SphereList& GetSphereListB() const { return GetData()->mSphereListB; }
    private:
        struct Data
        {
            SphereList mSphereListA;
            SphereList mSphereListB;
        };

        Data* GetData() { return reinterpret_cast<Data*>(maData); }
        const Data* GetData() const { return reinterpret_cast<const Data*>(maData); }
        CGS_STATIC_ASSERT(sizeof(Data) <= KU_MAX_DESC_DATA_SIZE, "SphereListWithSphereListJobDesc payload size");
    };

    // =============================================================================================
    // ⭐ ADDED 2026-08-14 (walls leg 1): the STREAM sibling (job-type id 8), DWARF-homed in this
    // same header (CgsSphereListWithSphereListJobDesc.h:104). Prepared inline by
    // BaseCollisionGenerator::RunCollideSphereListWithSphereListStream @0x82811C00 (+0x00
    // producer / +0xF4 0.0f / +0xF8 NULL — this Run takes NO debug reader, matching the DWARF
    // Prepare(SimpleDataStreamProducer*) single-argument signature at h:125 — / +0xFF type 8);
    // consumed by ContactGeneratorJob::ExecuteSphereListWithSphereListStream @0x82923758.
    //
    // ⭐ THE WHOLE LEG IS LIVE AS OF WAVE Q7 (2026-08-19) — every clause of the old banner here
    // ("its poster is NOT reconstructed … DoCarCarContactGeneration is still a gate … this stream
    // carries zero commands … its Run returns null before dispatching") is now FALSE. The poster
    // AddSphereListWithSphereListToStream @0x828119F0 landed in
    // CgsCollisionGenerator_CollideStreams.cpp; its producer VehicleManager::
    // DoCarCarContactGeneration @0x8261BB38 is a real body; and the consumer @0x82923758 is a real
    // body in ContactGeneratorJob_wQ7_01.cpp. The stream carries one command per overlapping
    // non-simple-traffic car pair per frame.
    // StreamCommand shape is DWARF h:108-113 ({A, B, padding, result list}); declared with the
    // family for the Create* factory's sizeof, same widening contract as the siblings.
    // =============================================================================================
    struct SphereListWithSphereListStreamJobDesc : public CollisionJobDescription
    {
        // ARTIST stream factories pass a 32-byte, 16-aligned command.
        struct CGS_ALIGN(16) StreamCommand
        {
            SphereList           mSphereListA;  // DWARF h:110 (console 8B pair; host 16)
            SphereList           mSphereListB;  // DWARF h:111 (console 8B pair; host 16)
            f32                  mfPadding;     // DWARF h:112
            CollisionResultList* mpResultList;  // DWARF h:113
        };

        // X360 +0x00, the only member of the derived part (DWARF Data h:136-138).

        CgsMemory::SimpleDataStreamProducer* GetStreamProducer() const { return GetData()->mpStreamProducer; }

        // DWARF h:125 `bool Prepare(SimpleDataStreamProducer*)` — no debug reader.
        bool Prepare(CgsMemory::SimpleDataStreamProducer* lpStreamProducer)
        {
            new (maData) Data;
            GetData()->mpStreamProducer = lpStreamProducer;   // 0x82811CB8  stw r22, 0x3D0(batch)

            return CollisionJobDescription::Prepare(E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST_STREAM, 0, 0.0f,
                                                    0);
        }

    private:
        struct Data
        {
            CgsMemory::SimpleDataStreamProducer* mpStreamProducer;
        };

        Data* GetData() { return reinterpret_cast<Data*>(maData); }
        const Data* GetData() const { return reinterpret_cast<const Data*>(maData); }
        CGS_STATIC_ASSERT(sizeof(Data) <= KU_MAX_DESC_DATA_SIZE, "SphereListWithSphereListStreamJobDesc payload size");
    };

    CGS_STATIC_ASSERT((sizeof(SphereListWithSphereListStreamJobDesc::StreamCommand) % 16 == 0),
                  "collide-stream command size must stay a multiple of 16 "
                  "(DataStreamCommandPoster::Construct tripwire :52)");
}
}
