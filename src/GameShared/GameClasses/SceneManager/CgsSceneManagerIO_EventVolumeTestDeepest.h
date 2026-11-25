#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // DecFIGS CgsSceneManagerIO_FineQuery.h:101–109. ARTIST 0x822170B0 stages
    // these fields and the AddEvent at 0x82210870 copies the 224-byte record.
    class CGS_ALIGN(16) InEventVolumeTestDeepest : public CgsModule::Event
    {
    public:
        Matrix44Affine mTransform;
        SceneQueryId mQueryId;
        u32 mx32EntityTypeFlags;
        EntityId mExcludeEntityId;
        EExclusionMode meExclusionMode;
        VolumeSlot mVolumeBuffer;
        u8 mxVolumeTypeFlags;
    };

    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, mTransform) == 0x00), "ARTIST transform offset");
    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, mQueryId) == 0x40), "ARTIST query ID offset");
    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, mx32EntityTypeFlags) == 0x44), "ARTIST entity flags offset");
    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, mExcludeEntityId) == 0x48), "ARTIST excluded entity offset");
    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, meExclusionMode) == 0x4C), "ARTIST exclusion mode offset");
    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, mVolumeBuffer) == 0x50), "ARTIST volume slot offset");
    CGS_STATIC_ASSERT((offsetof(InEventVolumeTestDeepest, mxVolumeTypeFlags) == 0xD0), "ARTIST volume flags offset");
    CGS_STATIC_ASSERT((sizeof(InEventVolumeTestDeepest) == 0xE0), "ARTIST deepest query stride");
    CGS_STATIC_ASSERT((__alignof(InEventVolumeTestDeepest) == 16), "ARTIST deepest query alignment");
}
}
