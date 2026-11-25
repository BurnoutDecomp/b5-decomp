#pragma once

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // ARTIST 0x822B15F8 stages the named handle and the aligned volume slot.
    class InEventReplaceDynamicVolume : public CgsModule::Event
    {
    public:
        VolumeId mVolumeId;
        VolumeSlot mVolumeBuffer;
    };

    CGS_STATIC_ASSERT((offsetof(InEventReplaceDynamicVolume, mVolumeId) == 0x00), "ARTIST dynamic volume id offset");
    CGS_STATIC_ASSERT((offsetof(InEventReplaceDynamicVolume, mVolumeBuffer) == 0x10), "ARTIST volume slot offset");
    CGS_STATIC_ASSERT((sizeof(InEventReplaceDynamicVolume) == 0x90), "ARTIST dynamic volume event stride");
    CGS_STATIC_ASSERT((__alignof(InEventReplaceDynamicVolume) == 16), "ARTIST dynamic volume event alignment");
}
}
