#pragma once

// CgsSceneManager::CgsCollision::SweptSphereListWithTriangleListStreamJobDesc — the descriptor
// for the streamed swept-sphere-vs-triangle-list collision pass (job-type id 14), the
// CONTINUOUS-collision leg of race-car world contact generation (a fast car sweeps its
// deformation spheres through the frame instead of testing them in place).
//
// ⭐ ADDED 2026-08-14 (walls leg 1). DWARF home: CgsSweptSphereListWithTriangleListJobDesc.h
// (the stream class at :104-146, its StreamCommand at :108-113). Written by
// BaseCollisionGenerator::RunCollideSweptSphereListWithTriangleListStream @0x828118A8
// (inlined Prepare stores: +0x00 producer / +0xF4 0.0f / +0xF8 the debug reader / +0xFF
// type 14 == `li r23, 0xE`) and consumed by ContactGeneratorJob::
// ExecuteSweptSphereListWithTriangleListStream @0x82925238 (a loud named gate until its
// kernel IntersectTriangle4SweptSphere @0x829238E8-family lands).
//
// The command record is what AddSweptSphereListWithTriangleListToStream @0x82811698 posts —
// byte-for-byte the same poster body as the sphere sibling @0x82811340, over a SweptSphereList
// instead of a SphereList. Same widening contract as the sibling (console command size 32,
// host sizeof(StreamCommand); runtime-carved ⇒ widen).
//
// ⚠️ The non-stream sibling (SweptSphereListWithTriangleListJobDesc, DWARF :51, job-type 13)
// is NOT declared here yet — nothing in the tree posts it; grow additively when its
// Collide/Test path is worked.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSweptSphereList.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"

// Stream producer (pointer-only use here).
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct SweptSphereListWithTriangleListStreamJobDesc : public CollisionJobDescription
    {
        struct StreamCommand
        {
            SweptSphereList      mSphereList;   // DWARF h:110 (console 8B pair; host 16) — DWARF's own name, list of SWEPT spheres
            TriangleList         mTriList;      // DWARF h:111 (console 8B pair; host 16)
            f32                  mfPadding;     // DWARF h:112 (0.5f on the race-car world path)
            CollisionResultList* mpResultList;  // DWARF h:113 (the poster's fresh result list)
        };

        // X360 +0x00, the only member of the derived part (DWARF Data h:137-139).
        CgsMemory::SimpleDataStreamProducer* mpStreamProducer;

        CgsMemory::SimpleDataStreamProducer* GetStreamProducer() const { return mpStreamProducer; }

        // DWARF `bool Prepare(SimpleDataStreamProducer*, DebugRenderStreamReader*)`; the X360
        // inlines it into the Run dispatcher (stores quoted in the banner).
        void Prepare(CgsMemory::SimpleDataStreamProducer* lpStreamProducer,
                     CgsDev::DebugRenderStreamReader*     lpDebugReader)
        {
            mpStreamProducer = lpStreamProducer;   // 0x82811948  stw r22, 0x3D0(batch)

            mpResultsList = 0;                     // stw r24, 0x4C0
            mfRadius      = 0.0f;                  // stfs f31, 0x4C4 (flt_82001CC0)
            mpDebugStream = lpDebugReader;         // stw r21, 0x4C8
            muJobType     = static_cast<u8>(E_COLLISIONJOB_SWEPT_SPHERE_LIST_WITH_TRIANGLE_LIST_STREAM); // stb r23(14), 0x4CF
        }
    };

    static_assert(sizeof(SweptSphereListWithTriangleListStreamJobDesc::StreamCommand) % 16 == 0,
                  "collide-stream command size must stay a multiple of 16 "
                  "(DataStreamCommandPoster::Construct tripwire :52)");
}
}
