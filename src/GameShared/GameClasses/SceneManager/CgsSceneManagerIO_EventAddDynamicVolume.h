#pragma once

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // ARTIST 0x822B1518 stages the named handle and the aligned volume slot.
    class InEventAddDynamicVolume : public CgsModule::Event
    {
    public:
        VolumeId mVolumeId;
        u8 mxVolumeTypeID;
        VolumeSlot mVolumeBuffer;
    };

    CGS_STATIC_ASSERT((offsetof(InEventAddDynamicVolume, mVolumeId) == 0x00), "ARTIST dynamic volume id offset");
    CGS_STATIC_ASSERT((offsetof(InEventAddDynamicVolume, mxVolumeTypeID) == 0x08), "ARTIST volume flags offset");
    CGS_STATIC_ASSERT((offsetof(InEventAddDynamicVolume, mVolumeBuffer) == 0x10), "ARTIST volume slot offset");
    CGS_STATIC_ASSERT((sizeof(InEventAddDynamicVolume) == 0x90), "ARTIST dynamic volume event stride");
    CGS_STATIC_ASSERT((__alignof(InEventAddDynamicVolume) == 16), "ARTIST dynamic volume event alignment");
}
}
