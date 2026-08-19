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
// WHAT REMAINS — ONE gate:
//   CollidePrimitivePairList @0x82814138 (92) — the SYNCHRONOUS primitive-pair collide leg
//   StartVehicleContactGeneration calls for the two simple-traffic pair lists. Nothing on the
//   junkyard path posts a traffic pair (both GetNumTests() guards are 0), so this is dead at
//   runtime today; when traffic lands, the gate names it. Its closure is its own: the type-10
//   descriptor prepare + ExecutePrimitivePairList @0x82925798 (92) + the pair-list walk.
//
// ⭐⭐ GATE RETIRED 2026-08-19 (wave Q6, cluster pstream):
//   AddPrimitiveListWithTriangleListToStream @0x82811D40 (35) is REAL now, in the sibling
//   CgsCollisionGenerator.cpp (its DWARF home TU — dumpfile source line 1996). The whole
//   blocker was ONE type plus ONE enum member, and both landed with it:
//     * JobDescription/CgsPrimitiveListWithTriangleListStreamJobDesc.h — the type-12 descriptor
//       and its nested StreamCommand (DWARF-homed in the non-Stream sibling header at :107-:149;
//       landed in its own file, matching the tree's placement for every other *Stream descriptor
//       whose DWARF home is a non-Stream header);
//     * E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12 in
//       JobDescription/CgsCollisionJobDescription.h.
//   The family's Create/Run halves (@0x82811DD0 / @0x82811F58) landed in the same TU, and the
//   type-12 WORKER (ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream @0x82926650,
//   100 insns) is a real body now too — so a posted command is actually drained.
//   ⚠️ Both TUs are MOUNTED (this one bat:1001, CgsCollisionGenerator.cpp bat:1018), so the gate
//   HAD to be deleted in the same change that landed the body or the mounted build takes an
//   LNK2005 that `cl /c` cannot see.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint (the boot gate, 2026-08-09)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // (AddPrimitiveListWithTriangleListToStream's gate lived here until 2026-08-19; the real body
    //  is in CgsCollisionGenerator.cpp. LNK2005 if a definition ever comes back to this file.)

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
