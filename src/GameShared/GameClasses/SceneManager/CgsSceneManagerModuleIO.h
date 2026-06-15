#pragma once

// Scene-manager IO event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF. Events derive from an empty per-module Event
// base (CgsModule event-queue convention).
#include "BrnCommonTypes.h"                                          // Vector3, EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"     // CgsSceneManager::SceneQueryId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct Event {};

    // Output event: the nearest hit of a line test.
    struct alignas(16) OutEventLineTestNearestResult : public Event
    {
        Vector3          mPosition;
        Vector3          mNormal;
        VolumeInstanceId mVolumeInstanceId;
        SceneQueryId     mQueryId;
        EntityId         mEntityId;
        f32              mfLineParam;
        u16              mu16MaterialTag;
        u16              mu16GroupTag;
        bool             mbIntersection;
    };
}
}
