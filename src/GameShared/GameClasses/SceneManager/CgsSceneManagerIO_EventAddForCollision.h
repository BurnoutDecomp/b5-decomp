#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventAddForCollision -- the per-event payload stored in the
// EventQueue<InEventAddForCollision, 1536> add-for-collision input queue
// (InSceneUpdateInterface::InAddForCollisionQueue, DWARF home
// CgsSceneManagerIO_SceneUpdate.h:266). This header exists so the explicit-
// instantiation TU for that queue's Construct (EventQueue_InEventAddForCollision_1536.cpp)
// can see a COMPLETE element type (the queue embeds InEventAddForCollision maEvents[1536]
// inline). The full InSceneUpdateInterface aggregate keeps its own placeholder slice and
// its own ledger TU -- this header only adds the leaf element it queues, by name.
//
// LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:172) -- derives from the empty
// SceneManagerIO::Event base:
//     Vector3                       mPadding        (:174)
//     VolumeInstanceId              mVolumeInstanceId(:175)
//     CullingGroup (u32)            mCullGroup      (:176)
//     uint8_t                       muBodyState     (:207, private)
//     uint8_t                       muCacheOptions  (:208, private)
// Carries a Vector3, so the element is 16-byte aligned (alignas(16)). The DWARF declares
// byte-stored BodyState/CacheOptions accessors (Get/SetBodyState, Get/SetCacheOptions);
// those bodies belong to InEventAddForCollision's own functions and are not part of this
// queue-instantiation TU, so only the stored named members are reconstructed here.
#include "BrnCommonTypes.h"                                          // Vector3, EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // SetRaceCarCullingGroupEvent::CullingGroup

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Matches the SceneManagerIO::Event used by the other
    // SceneManager IO event payloads.
    struct Event {};

    // Cache-placement option for a queued add-for-collision request
    // (DWARF CgsSceneManagerIO_SceneUpdate.h:43). Stored as a single byte
    // (muCacheOptions) in the event; the enum supplies the named values.
    enum EAddForCollisionCacheOptions
    {
        E_ADD_TO_CACHE_MANAGER_AS_CACHED     = 0,
        E_ADD_TO_CACHE_MANAGER_AS_NON_CACHED = 1,
        E_DO_NOT_ADD_TO_CACHE_MANAGER        = 2,
        E_NUM_CACHE_OPTIONS                  = 3
    };

    // CgsSceneManagerIO_SceneUpdate.h:172.
    struct alignas(16) InEventAddForCollision : public Event
    {
        // Culling-group tag -- the same u32 typedef the vehicle event family uses
        // (BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup).
        typedef BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup CullingGroup;

        Vector3          mPadding;           // :174
        VolumeInstanceId mVolumeInstanceId;  // :175
        CullingGroup     mCullGroup;         // :176

    private:
        u8 muBodyState;     // :207 (rw::physics::BodyState packed into a byte)
        u8 muCacheOptions;  // :208 (EAddForCollisionCacheOptions packed into a byte)
    };
}
}
