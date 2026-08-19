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
        SphereList mSphereListA;  // +0x00
        SphereList mSphereListB;  // +0x08

        // Prepare @0x82810198 — returns true (the X360 `return 1;`). lfPadding is the
        // radius/padding stored at +0xF4 (f1 -> stfs).
        bool Prepare(const SphereList*    lpSphereListA,
                     const SphereList*    lpSphereListB,
                     CollisionResultList* lpResultsList,
                     f32                  lfPadding);
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
        struct StreamCommand
        {
            SphereList           mSphereListA;  // DWARF h:110 (console 8B pair; host 16)
            SphereList           mSphereListB;  // DWARF h:111 (console 8B pair; host 16)
            f32                  mfPadding;     // DWARF h:112
            CollisionResultList* mpResultList;  // DWARF h:113
        };

        // X360 +0x00, the only member of the derived part (DWARF Data h:136-138).
        CgsMemory::SimpleDataStreamProducer* mpStreamProducer;

        CgsMemory::SimpleDataStreamProducer* GetStreamProducer() const { return mpStreamProducer; }

        // DWARF h:125 `bool Prepare(SimpleDataStreamProducer*)` — no debug reader.
        void Prepare(CgsMemory::SimpleDataStreamProducer* lpStreamProducer)
        {
            mpStreamProducer = lpStreamProducer;   // 0x82811CB8  stw r22, 0x3D0(batch)

            mpResultsList = 0;                     // stw r27, 0x4C0
            mfRadius      = 0.0f;                  // stfs f31, 0x4C4 (flt_82001CC0)
            mpDebugStream = 0;                     // stw r27, 0x4C8
            muJobType     = static_cast<u8>(E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST_STREAM); // stb r23(8), 0x4CF
        }
    };

    static_assert(sizeof(SphereListWithSphereListStreamJobDesc::StreamCommand) % 16 == 0,
                  "collide-stream command size must stay a multiple of 16 "
                  "(DataStreamCommandPoster::Construct tripwire :52)");
}
}
