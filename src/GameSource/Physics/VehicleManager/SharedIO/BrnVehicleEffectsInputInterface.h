#pragma once

// BrnPhysics::Vehicle::VehicleEffectsInputInterface -- the per-frame effects-input surface the
// race-car entity module fills (air-ram / spin stunt impulses) and VehicleManager drains
// (UpdateVehicleEffects @0x82629E18 walks mAirRamQueue directly through the inlined accessor:
// its loop reads queue+8 == BaseEventQueue::miLength and GetEvent's 64-byte stride).
//
// PROMOTED 2026-08-06 (PhysicsModule::Update leaves wave). This header carried a 256-byte
// NOMINAL blob ("only returned-by-pointer from the IO buffers, so a reserved-byte blob
// suffices"). That claim EXPIRED twice over: (1) BrnRaceCarEntityModuleIO.h embeds this type
// BY VALUE (mVehicleEffectsInterface), so the blob under-sized a real sub-object; (2)
// UpdateVehicleEffects iterates the air-ram queue by name, which a blob cannot express. The
// real layout is DWARF-verbatim (BrnVehicleEffectsInputInterface.h:83..:122 via
// references/DecFIGS/dwarfdump/.../SharedIO/BrnVehicleEffectsInputInterface.h): exactly two
// private event queues + the accessor/lifecycle surface. Both element types are committed in
// BrnVehicleEvents.h with X360-attested strides (CreateAirRamEvent 64, CreateSpinEvent 48 --
// VehicleEvents_embed_check.cpp pins both), and both EventQueue instantiation Construct slices
// already exist (EventQueue_CreateAirRamEvent_20_Construct.cpp / .._CreateSpinEvent_10_..).
//
// No standalone X360 symbol exists for ANY method of this type (name-indexed sweep of the
// export set, 2026-08-06) -- everything is inlined at the call sites, so Construct is defined
// inline here as the only shape it can have: the two queue Constructs, in member order.
// CreateAirRam/CreateSpin/Clear (DWARF :94..:117) are NOT declared yet -- declaring them
// without recovered bodies would be dead API; add them when their inline sites are read.
// Append (DWARF :88) IS declared as of 2026-08-10 (pre-physics bridge wave), because its
// inline site HAS now been read -- see the body below.

#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // CreateAirRamEvent (64B) / CreateSpinEvent (48B)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                  // CgsModule::EventQueue

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) VehicleEffectsInputInterface
    {
        // DWARF BrnVehicleQueues.h:43/:44 -- the two queue typedefs.
        typedef CgsModule::EventQueue<CreateAirRamEvent, 20> CreateAirRamEventQueue;
        typedef CgsModule::EventQueue<CreateSpinEvent, 10>   CreateSpinEventQueue;

        // DWARF :91. Inlined on the console (the owning IO buffer's Construct reaches the two
        // queue Constructs directly); the shape is fully determined by the member list.
        void Construct()
        {
            mAirRamQueue.Construct();
            mSpinQueue.Construct();
        }

        // DWARF :88. ADDED 2026-08-10 (pre-physics bridge wave). No out-of-line symbol -- the
        // console inlines it, and WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics
        // @0x827AAEC0 inlines it TWICE, once per source (race-car then traffic), each time as
        // exactly this pair of queue merges:
        //   0x827AB138  bl BaseEventQueue<CreateAirRamEvent>::Append (dst+0,      src+0)
        //   0x827AB144  bl BaseEventQueue<CreateSpinEvent>::Append   (dst+0x510,  src+0x510)
        // 0x510 == 1296 == the console gap between mAirRamQueue and mSpinQueue (the same gap
        // BrnTrafficIO::OutputBuffer_PrePhysics::Construct @0x827618A0 walks: air-ram Construct
        // at +142192, spin Construct at +143488). Reached BY MEMBER here, not by that offset.
        void Append(const VehicleEffectsInputInterface* lpInterfaceToAppend)
        {
            mAirRamQueue.Append(lpInterfaceToAppend->mAirRamQueue);
            mSpinQueue.Append(lpInterfaceToAppend->mSpinQueue);
        }

        // DWARF :97/:100 -- the read accessors UpdateVehicleEffects' inlined queue walk proves.
        const CreateAirRamEventQueue* GetAirRamEventQueue() const { return &mAirRamQueue; }
        const CreateSpinEventQueue*   GetSpinEventQueue()   const { return &mSpinQueue; }

    private:
        // DWARF :121/:122, in declaration order (mAirRamQueue @+0 -- UpdateVehicleEffects
        // @0x82629E18 receives the interface pointer and indexes the air-ram queue at +0).
        CreateAirRamEventQueue mAirRamQueue;   // console +0    (12B base + 20 * 64B events)
        CreateSpinEventQueue   mSpinQueue;     // console +1296 (12B base + pad + 10 * 48B events)
    };
}
}
