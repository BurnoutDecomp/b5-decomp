// ============================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/
// CgsCollisionGenerator_StreamStubs.cpp
//
// ⭐⭐ SIX OF THE SEVEN STUBS RETIRED 2026-08-14 (walls leg 1). This TU was born
// (2026-08-06, big-five #2 wave) holding trap stubs for the whole collide-stream family;
// the three Create* factories, the three Run* dispatchers and the two Add* posters are REAL
// now, in CgsCollisionGenerator_CollideStreams.cpp (bodies read from the image; the three
// Create* proved byte-identical bar assert lines; the Run* wire ContactGeneratorEntry over
// desc types 6/14/8 whose workers are loud named gates in ContactGeneratorJob.cpp).
// If a definition for any of them reappears here the link will say so (LNK2005).
//
// WHAT REMAINS — TWO gates:
//   CollidePrimitivePairList @0x82814138 (92) — the SYNCHRONOUS primitive-pair collide leg
//   StartVehicleContactGeneration calls for the two simple-traffic pair lists. Nothing on the
//   junkyard path posts a traffic pair (both GetNumTests() guards are 0), so this is dead at
//   runtime today; when traffic lands, the gate names it. Its closure is its own: the type-10
//   descriptor prepare + ExecutePrimitivePairList @0x82925798 (92) + the pair-list walk.
//
//   ⭐ ADDED 2026-08-19 (wave Q6, cluster B):
//   AddPrimitiveListWithTriangleListToStream @0x82811D40 (35) — the PRIMITIVE-PAIR-LIST vs
//   TRIANGLE-LIST stream POSTER, declared since 2026-08-18 and bodyless ever since. It is the
//   arm the console's runtime selector byte_82F2A39C actually takes (re-measured 0x01 this wave),
//   so it is the LIVE half of prop-vs-world contact generation. Gated here rather than left
//   unresolved because the two legs that call it landed this wave in the MOUNTED
//   GameSource/Physics/PropManager/PropManager_wQ2_03.cpp, and the bat's own doctrine is that
//   "LNK2019 resolves before /OPT:REF discards" (build_game_exe.bat:544).
//   ⛔ ITS CLOSURE IS SMALL AND FULLY SPECIFIED, and it is the wave's #2 follow-up:
//     * the 35-instruction body is decoded on the declaration in CgsCollisionGenerator.h
//       (seven GPR arguments, no float: r4 pair list, r5 triangle list, r6 max results,
//        r7 the bool `stb r26`, r8 tag A, r9 tag B, r10 producer);
//     * it needs ONE type -- CgsCollision::PrimitiveListWithTriangleListStreamJobDesc with its
//       nested StreamCommand -- which belongs beside the ALREADY-MOUNTED non-Stream sibling
//       JobDescription/CgsPrimitiveListWithTriangleListJobDesc.h (bat:2864). A paste-ready
//       draft sits at scratchpad/waveQ2/parked/CgsPrimitiveListWithTriangleListStreamJobDesc.PROPOSED.h;
//     * plus ONE enum member in JobDescription/CgsCollisionJobDescription.h --
//       E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12 (MEASURED: the Run half does
//       `li r23, 0xC` -> `stb r23, 0x4CF(batch)`), whose type-12 WORKER is already a loud named
//       gate at ContactGeneratorJob.cpp:230;
//     * and with that same type, the family's Create/Run halves (@0x82811DD0 / @0x82811F58) drop
//       straight in from scratchpad/waveQ2/parked/CgsCollisionGenerator_wQ2_PrimitiveStream.cpp,
//       which is what PropManager_wQ2_02.cpp (Begin/End) needs before it can mount.
//     ⚠️ THAT PARK'S BANNER IS STALE IN THE HELPFUL DIRECTION -- it says the descriptor "has no
//     home in this tree"; the non-Stream sibling header AND its .cpp exist and are mounted.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint (the boot gate, 2026-08-09)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // ⛔ LOUD NAMED GATE -- see the file banner. Returns -1, not 0: every measured call site drops
    // the value (the two prop legs, DeformableObject x3 and VehicleManager traffic x2 all discard
    // r3), and 0 is a VALID result-list index, so -1 cannot be mistaken for a real list if a future
    // caller ever starts reading it.
    s32 BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream(
        const PrimitivePairList* /*lpSPrimList*/, const TriangleList* /*lpTriangleList*/,
        u16 /*lu16MaxNumCollisions*/, bool /*lbUseOptimisedBoxTests*/,
        u32 /*luUserTagA*/, u16 /*lu16UserTagB*/,
        CgsMemory::SimpleDataStreamProducer* /*lpPrimitiveTriangleStream*/)
    {
        do { static bool s_bLogged = false;
        if (!s_bLogged) { s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream"
                       " @0x82811D40 (35) not reconstructed -- the LIVE arm of prop/part-vs-world"
                       " contact generation posts NO collision job (wave Q6 cluster B residual;"
                       " closure = PrimitiveListWithTriangleListStreamJobDesc + collision-job-type 12"
                       " + the parked Create/Run halves)\n"; } } while (0);
        return -1;
    }

    u16 BaseCollisionGenerator::CollidePrimitivePairList(const PrimitivePairList* /*lpPairList*/,
                                                         u16 /*lu16MaxResults*/, u32 /*luFlags*/, u16 /*lu16Tag*/)
    {
        do { static bool s_bLogged = false;
        if (!s_bLogged) { s_bLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "conductor gate (was TRAP): BaseCollisionGenerator::CollidePrimitivePairList "
                          "@0x82814138 not reconstructed (big-five #2 closure stub)\n"; } } while (0);
        return 0;
    }
}
}
