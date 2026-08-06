// ============================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/
// CgsCollisionGenerator_StreamStubs.cpp
//
// ⚠⚠ TRAP-STUB TU (closure enforcement, 2026-08-06 big-five #2 wave). The seven
// collide-stream methods VehicleManager::StartVehicleContactGeneration @0x8262AEE8 calls on
// BaseCollisionGenerator, each declared in CgsCollisionGenerator.h with its X360 address --
// NONE of the real bodies (39..97 asm lines each, dense job/stream plumbing) is
// reconstructed yet. Every stub traps loudly; all are dead code today (the caller chain tops
// out at PhysicsModule::Update @0x825B0640, still a link stub; /OPT:REF strips this TU).
// RECONSTRUCT-NEXT, together as a family -- the three Create* share one shape (the
// CreateStreamProducer factory + per-type command geometry) and the three Run* share another
// (AllocateJob + per-batch dependency wiring).
// ============================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsSceneManager
{
namespace CgsCollision
{
    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateCollideSphereListWithTriangleListStream(s32 /*liMaxCommands*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::CreateCollideSphereListWithTriangleListStream "
                          "@0x828113C8 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }

    EA::Jobs::Job*
    BaseCollisionGenerator::RunCollideSphereListWithTriangleListStream(
        CgsMemory::SimpleDataStreamProducer* /*lpProducer*/, CgsDev::DebugRenderStreamReader* /*lpDebugReader*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::RunCollideSphereListWithTriangleListStream "
                          "@0x82811550 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }

    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateCollideSweptSphereListWithTriangleListStream(s32 /*liMaxCommands*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::CreateCollideSweptSphereListWithTriangleListStream "
                          "@0x82811720 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }

    EA::Jobs::Job*
    BaseCollisionGenerator::RunCollideSweptSphereListWithTriangleListStream(
        CgsMemory::SimpleDataStreamProducer* /*lpProducer*/, CgsDev::DebugRenderStreamReader* /*lpDebugReader*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::RunCollideSweptSphereListWithTriangleListStream "
                          "@0x828118A8 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }

    CgsMemory::SimpleDataStreamProducer*
    BaseCollisionGenerator::CreateCollideSphereListWithSphereListStream(s32 /*liMaxCommands*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::CreateCollideSphereListWithSphereListStream "
                          "@0x82811A78 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }

    EA::Jobs::Job*
    BaseCollisionGenerator::RunCollideSphereListWithSphereListStream(
        CgsMemory::SimpleDataStreamProducer* /*lpProducer*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::RunCollideSphereListWithSphereListStream "
                          "@0x82811C00 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }

    u16 BaseCollisionGenerator::CollidePrimitivePairList(const PrimitivePairList* /*lpPairList*/,
                                                         u16 /*lu16MaxResults*/, u32 /*luFlags*/, u16 /*lu16Tag*/)
    {
        CGS_ASSERT(false, "TRAP: BaseCollisionGenerator::CollidePrimitivePairList "
                          "@0x82814138 not reconstructed (big-five #2 closure stub)\n");
        return 0;
    }
}
}
