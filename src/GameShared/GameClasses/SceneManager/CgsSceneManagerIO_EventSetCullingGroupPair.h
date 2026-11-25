#pragma once

// ARTIST SetCullingGroupPair (0x822B1B60) stages two culling groups and a
// byte flag. The queue copies all twelve bytes (0x822AB7D0).
// DecFIGS CgsSceneManagerIO_SceneUpdate.h:226 declares the two groups and bool.

#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseSetCullingGroupPair {};

    class InEventSetCullingGroupPair : public EventBaseSetCullingGroupPair
    {
    public:
        BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup mCullingGroupA;
        BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup mCullingGroupB;
        bool mbCull;
    };
}
}
