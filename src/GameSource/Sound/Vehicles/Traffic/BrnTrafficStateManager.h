#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // VecFloat, Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (TrafficClassToSize)
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"           // CgsSound::Logic::Content (x4 banks)
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"             // BrnTraffic::E_VEHICLECLASS_COUNT
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"     // BrnSound::Logic::BrnStateManager (base)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h" // TrafficSoundEntity

// =============================================================================
// BrnSound::Logic::Traffic::TrafficStateManager
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.{h,cpp}
//
// The traffic-vehicle sound domain (state manager 3): 32 attachable traffic-entity
// slots, the engine / horn AEMS banks and their CSIS interfaces. Each frame it matches
// the traffic module's sound entities (nearest the camera microphone first) against
// its slots, detaches slots whose entity left the list, and attaches one of its six
// TrafficStates to each new entity, culling the farthest attached entity when no
// state is free.
//
// Console layout (32-bit; members are pinned BY NAME on the host):
//   +0x90 IResourceRequester sub-object, +0x94 miCpuMonitor (BrnStateManager)
//   +0xA0 maSlots[32]  (96-byte Slot: mEntity +0x00, mbActive +0x50, mpAttachedState +0x54)
//   +0xCA0 mEngineAemsBank, +0xCAC mHornAemsBank, +0xCB8 mEngineCsisInterface,
//   +0xCC4 mHornCsisInterface; sizeof 0xCD0.
// =============================================================================

namespace CgsSound { namespace Logic { struct State; } }

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{
    enum ETrafficSize
    {
        E_SMALL     = 0,
        E_MEDIUM    = 1,
        E_LARGE     = 2,
        E_MAX_SIZES = 3,
    };

    struct TrafficEngine;
    struct TrafficHorn;
    struct TrafficSkid;
    struct TrafficControl;
    struct Traffic3DControl;
    struct TrafficState;

    struct TrafficStateManager : public BrnStateManager
    {
        // The distance-sort scratch record: the squared distance (splat over the four
        // lanes) and the index it was measured for.
        struct SortResult
        {
            VecFloat mfDistance;
            u16      muIndex;

            // True when every lane of lrA's distance is below lrB's.
            static bool LessThanDistance(const SortResult& lrA, const SortResult& lrB);
        };

        // One attachable traffic-entity slot. The console constructor clears mbActive
        // and mpAttachedState of all 32 slots; the entity copy is left as allocated.
        struct Slot
        {
            typedef BrnTraffic::BrnTrafficIO::TrafficSoundEntity TrafficSoundEntity;

            TrafficSoundEntity       mEntity;
            bool                     mbActive;
            CgsSound::Logic::State*  mpAttachedState;

            Slot() : mbActive(false), mpAttachedState(0) {}
        };

        static const u32 KU_NUM_SLOTS = 32;

        // The number of TrafficStates Prepare asks PrepareStates for.
        static const u32 KU_NUMBER_OF_TRAFFIC_CARS = 6;

        // PrepareStates effect mask: TrafficEngine (0), TrafficHorn (1), TrafficSkid (2)
        // and TrafficInAir (3).
        static const s32 KI_TRAFFIC_STATE_EFFECT_MASK = 15;

        TrafficStateManager();
        virtual ~TrafficStateManager();

        // ---- RTTI hooks. STATIC GetStaticTypeInfo / CreateObject so &CreateObject is
        // storable in ClassTypeInfo<StateManager>::createObject. ----
        virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
        virtual const char* GetTypeName() const;
        static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
        static CgsSound::Logic::StateManager* CreateObject( u32 luType );

        virtual bool Prepare();
        virtual void UpdateParams( f32 lfTimeStep );

        // Introduced here (console vtable +0x2C, after IsStateAlias): drops the four
        // banks. No caller in the console image reaches it.
        virtual void ExitGamePlay();

        // IResourceRequester: the traffic bundles resolved -- construct the two AEMS banks.
        virtual void ResourcesAreReady();

        const CgsSound::Logic::Content& GetEngineAemsBank() { return mEngineAemsBank; }
        const CgsSound::Logic::Content& GetHornAemsBank()   { return mHornAemsBank; }

        // A header inline on the console, emitted out of line and called with the class
        // byte alone (no `this`), so it is static. The four-entry table maps a car to
        // small, a van to medium, a bus or a big rig to large.
        static ETrafficSize TrafficClassToSize( u8 lu8VehicleClass )
        {
            static const ETrafficSize KAE_VEHICLE_CLASS_TO_SIZE[BrnTraffic::E_VEHICLECLASS_COUNT] =
                { E_SMALL, E_MEDIUM, E_LARGE, E_LARGE };
            CGS_ASSERT( lu8VehicleClass < BrnTraffic::E_VEHICLECLASS_COUNT,
                        "lu8VehicleClass < BrnTraffic::E_VEHICLECLASS_COUNT" );
            return KAE_VEHICLE_CLASS_TO_SIZE[lu8VehicleClass];
        }

    protected:
        bool AttachEntity( Slot::TrafficSoundEntity lEntity );

        // Inlined into both callers on the console (UpdateParams and CullIfFurtherThan).
        bool DetachEntity( Slot& lrActiveSlot );

        bool CullIfFurtherThan( VecFloat lfDistance, Vector3 lPosition );

        void SortEntitiesDistanceFromPosition(
            Vector3 lPosition,
            const BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface& lrTrafficInterface,
            SortResult* lpResults ) const;

        Slot                     maSlots[KU_NUM_SLOTS];
        CgsSound::Logic::Content mEngineAemsBank;
        CgsSound::Logic::Content mHornAemsBank;
        CgsSound::Logic::Content mEngineCsisInterface;
        CgsSound::Logic::Content mHornCsisInterface;
    };
}
}
}
