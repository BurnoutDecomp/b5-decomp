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
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_Event.h" // CgsSceneManager::SceneManagerIO::Event (single canonical definition)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // SetRaceCarCullingGroupEvent::CullingGroup
// The two byte-packed field accessors added 2026-08-18 name these directly. Both were
// ALREADY in this header's transitive closure (via BrnVehicleEvents.h) -- proved by
// scratchpad/waveQ4/probe_collision/probe_incl2.cpp, which compiles the accessors against
// this header ALONE -- so naming them here changes no include closure, it only stops the
// header depending on a sibling's private choice of includes.
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (the two setters' tripwires)
#include "rw/physics/rigidbody.h"                     // rw::physics::BodyState

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores events by
    // byte image). SceneManagerIO::Event now lives in CgsSceneManagerIO_Event.h as a single
    // canonical definition (was formerly redefined here -- the redefinition tripped C2011 once
    // SceneUpdate.h began co-including this header alongside CgsSceneManagerModuleIO.h).

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

        // ---- the two byte-packed field accessors ------------------------------------------
        // ADDED 2026-08-18 (wave Q4, collision seam). The header banner above already noted
        // that the DWARF declares these four and that they were deliberately left out; the
        // producer InSceneUpdateInterface::AddForCollision @0x822B1860 cannot be written
        // without the two setters (muBodyState / muCacheOptions are private), so they land now.
        //
        // DWARF (CgsSceneManagerIO_SceneUpdate.h, the element's own home in the original tree):
        //     :180  rw::physics::BodyState GetBodyState() const;
        //     :186  void SetBodyState(rw::physics::BodyState);
        //     :193  EAddForCollisionCacheOptions GetCacheOptions() const;
        //     :199  void SetCacheOptions(EAddForCollisionCacheOptions);
        //
        // X360 ATTESTATION of the two setters' tripwires -- both are inlined into
        // AddForCollision @0x822B1860, which bakes the file + line of each:
        //     0x822B18C0  cmpwi cr6, r29, 0xFF ; ble skip
        //                 -> "(int)leBodyState <= 0xff"   line 0xBC == 188  (== :186 + 2)
        //     0x822B18F0  cmpwi cr6, r27, 0xFF ; ble skip
        //                 -> "(int)leOptions <= 0xff"     line 0xC9 == 201  (== :199 + 2)
        // The stores that follow each tripwire are `stb` into the event's +0x1C / +0x1D bytes,
        // i.e. the two private members below. Both are HEADER INLINES: no out-of-line symbol
        // for any of the four exists in the X360 export set.
        //
        // ⚠️ The assert MESSAGES are the console's own rodata, including the second one's
        // "leOptions" (not "leCacheOptions") -- transcribed, not tidied.
        rw::physics::BodyState GetBodyState() const
        {
            return static_cast<rw::physics::BodyState>(muBodyState);
        }

        void SetBodyState(rw::physics::BodyState leBodyState)
        {
            CGS_ASSERT(static_cast<int>(leBodyState) <= 0xff, "(int)leBodyState <= 0xff");
            muBodyState = static_cast<u8>(leBodyState);
        }

        EAddForCollisionCacheOptions GetCacheOptions() const
        {
            return static_cast<EAddForCollisionCacheOptions>(muCacheOptions);
        }

        void SetCacheOptions(EAddForCollisionCacheOptions leOptions)
        {
            CGS_ASSERT(static_cast<int>(leOptions) <= 0xff, "(int)leOptions <= 0xff");
            muCacheOptions = static_cast<u8>(leOptions);
        }

    private:
        u8 muBodyState;     // :207 (rw::physics::BodyState packed into a byte)
        u8 muCacheOptions;  // :208 (EAddForCollisionCacheOptions packed into a byte)
    };
}
}
