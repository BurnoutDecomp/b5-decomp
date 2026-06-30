#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"  // CgsSceneManager::SceneManagerIO::SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h" // InEventLineTestNearest + ENearestExclusionMode

// CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestNearest  @ X360 0x82216FD0
//
// Builds an InEventLineTestNearest on the stack (the two Vector3 line endpoints arrive in the
// vector regs v1/v2 -> mLineStart/mLineEnd; the five trailing scalar GPR args a2..a6 fill
// mQueryId/mx32EntityTypeFlags/mxVolumeTypeFlags/mExcludeEntityId/meExclusionMode) and pushes it
// onto this interface's nearest-line-test input queue (mpFineLineTestNearestQueue,
// BaseEventQueue<InEventLineTestNearest>* at this+0x04). Store-for-store sibling of LineTestFine
// @ 0x82216EF0, which reads its queue pointer from this+0x00 instead -- the only structural
// difference.
//
// Both asserts are non-gating tripwires (CGS_ASSERT collapses BeginAssert/FireAssert/EndAssert):
// the X360 falls through and performs the AddEvent regardless. Messages verbatim from rodata;
// file+line args dropped per project convention. The exclusion-mode assert is gated on a6 (r31),
// i.e. leExclusionMode IS the LAST scalar arg, stored at the event's +0x2C; the byte arg a4 (r26)
// is mxVolumeTypeFlags at +0x30.
//
// X360 store-for-store (event base = sp+0x50, var_A0):
//   stvx128 v127(=v1)  -> +0x00  mLineStart
//   stvx128 v126(=v2)  -> +0x10  mLineEnd
//   stw     r28(=a2)   -> +0x20  mQueryId            (var_80)
//   stw     r27(=a3)   -> +0x24  mx32EntityTypeFlags (var_7C)
//   stw     r25(=a5)   -> +0x28  mExcludeEntityId    (var_78)
//   stw     r31(=a6)   -> +0x2C  meExclusionMode     (var_74)
//   stb     r26(=a4)   -> +0x30  mxVolumeTypeFlags   (var_70)
//   lwz r3,4(r29); bl ...InEventLineTestNearest_::AddEvent  -> result in r3, returned straight through.
bool CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestNearest(
    const Vector3&                lLineStart,
    const Vector3&                lLineEnd,
    CgsSceneManager::SceneQueryId lQueryId,
    u32                           lx32EntityTypeFlags,
    u8                            lxVolumeTypeFlags,
    CgsSceneManager::EntityId     lExcludeEntityId,
    ENearestExclusionMode         leExclusionMode)
{
    CGS_ASSERT(
        leExclusionMode == E_NEAREST_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_NEAREST_EXCLUDE_ENTITY_ONLY,
        "leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY");
    CGS_ASSERT(mpFineLineTestNearestQueue != nullptr, "mpFineLineTestNearestQueue");

    InEventLineTestNearest lEvent;
    lEvent.mLineStart          = lLineStart;           // +0x00  (v1)
    lEvent.mLineEnd            = lLineEnd;              // +0x10  (v2)
    lEvent.mQueryId            = lQueryId;              // +0x20  (a2 / r28)
    lEvent.mx32EntityTypeFlags = lx32EntityTypeFlags;  // +0x24  (a3 / r27)
    lEvent.mExcludeEntityId    = lExcludeEntityId;      // +0x28  (a5 / r25)
    lEvent.meExclusionMode     = leExclusionMode;       // +0x2C  (a6 / r31)
    lEvent.mxVolumeTypeFlags   = lxVolumeTypeFlags;     // +0x30  (a4 / r26, byte)

    return mpFineLineTestNearestQueue->AddEvent(lEvent);
}
