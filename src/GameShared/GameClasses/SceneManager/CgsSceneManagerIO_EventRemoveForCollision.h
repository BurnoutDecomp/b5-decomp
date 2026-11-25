#pragma once

#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct EventBaseRemoveForCollision {};

    // DecFIGS CgsSceneManagerIO_SceneUpdate.h:213. The ARTIST queue constructor
    // (0x822E2020) aligns the event array to eight bytes, and AddEvent
    // (0x822AB2C8) copies each full VolumeInstanceId as one eight-byte value.
    class CGS_ALIGN(8) InEventRemoveForCollision : public EventBaseRemoveForCollision
    {
    public:
        VolumeInstanceId mVolumeInstanceId;
    };
}
}
