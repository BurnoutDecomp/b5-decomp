#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"  // CgsSceneManager::SceneManagerIO::SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h" // InEventLineTestNearest

// ARTIST 0x82216FD0 takes the two endpoints in v1/v2.
void CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestNearest(
    SceneQueryId lQueryId,
    u32 lx32EntityTypeFlags,
    u8 lxVolumeTypeFlags,
    Vector3 lLineStart,
    Vector3 lLineEnd,
    EntityId lExcludeEntityId,
    EExclusionMode leExclusionMode)
{
    static const char KAC_ARTIST_FILE[] =
        "..\\..\\..\\GameShared\\GameClasses\\SceneManager/CgsSceneManagerModuleIO.h";

    CGS_ASSERT(leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY,
               CGS_ASSERT_MESSAGE_AT("leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY",
                                     KAC_ARTIST_FILE, 1020));
    CGS_ASSERT(mpFineLineTestNearestQueue,
               CGS_ASSERT_MESSAGE_AT("mpFineLineTestNearestQueue",
                                     KAC_ARTIST_FILE, 1021));

    InEventLineTestNearest lEvent;
    lEvent.mQueryId = lQueryId;
    lEvent.mx32EntityTypeFlags = lx32EntityTypeFlags;
    lEvent.mLineStart = lLineStart;
    lEvent.mLineEnd = lLineEnd;
    lEvent.mxVolumeTypeFlags = lxVolumeTypeFlags;
    lEvent.mExcludeEntityId = lExcludeEntityId;
    lEvent.meExclusionMode = leExclusionMode;
    mpFineLineTestNearestQueue->AddEvent(lEvent);
}
