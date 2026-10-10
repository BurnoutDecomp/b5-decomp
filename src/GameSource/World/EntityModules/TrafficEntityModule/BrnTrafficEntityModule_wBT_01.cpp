// BrnTrafficEntityModule_wBT_01.cpp -- TrafficEntityModule::DEBUGValidateSoaData, the debug
// consistency check between each param / vehicle record's own flags and the module's SoA bit sets.

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTraffic
{
    // Every param's alive / dying flags must match the alive / dying sets, and a live param's
    // zombie flag the zombie set. Every vehicle slot's alive flag must match the alive set, and a
    // live vehicle's collidable / physical / has-entity flags their sets. Both walks cover the
    // first KU_MAX_PARAMS slots; GetParam / GetVehicle and IsBitSet carry their own range asserts.
    void TrafficEntityModule::DEBUGValidateSoaData()
    {
        for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
        {
            CGS_ASSERT(GetParam(luParam)->IsAlive() == mParamSoaData.mAliveParams.IsBitSet(luParam),
                       "GetParam( luParam )->IsAlive() == mParamSoaData.mAliveParams.IsBitSet( luParam )");
            CGS_ASSERT(GetParam(luParam)->IsDying() == mParamSoaData.mDyingParams.IsBitSet(luParam),
                       "GetParam( luParam )->IsDying() == mParamSoaData.mDyingParams.IsBitSet( luParam )");

            if (GetParam(luParam)->IsAlive())
            {
                CGS_ASSERT(GetParam(luParam)->IsZombie() == mParamSoaData.mZombieParams.IsBitSet(luParam),
                           "GetParam( luParam )->IsZombie() == mParamSoaData.mZombieParams.IsBitSet( luParam )");
            }
        }

        for (u32 luVehicle = 0; luVehicle < KU_MAX_PARAMS; ++luVehicle)
        {
            CGS_ASSERT(GetVehicle(luVehicle)->IsAlive() == mVehicleSoaData.mAliveVehicles.IsBitSet(luVehicle),
                       "GetVehicle( luVehicle )->IsAlive() == mVehicleSoaData.mAliveVehicles.IsBitSet( luVehicle )");

            if (GetVehicle(luVehicle)->IsAlive())
            {
                CGS_ASSERT(GetVehicle(luVehicle)->IsCollidable() == mVehicleSoaData.mCollidableVehicles.IsBitSet(luVehicle),
                           "GetVehicle( luVehicle )->IsCollidable() == mVehicleSoaData.mCollidableVehicles.IsBitSet( luVehicle )");
                CGS_ASSERT(GetVehicle(luVehicle)->IsPhysical() == mVehicleSoaData.mPhysicalVehicles.IsBitSet(luVehicle),
                           "GetVehicle( luVehicle )->IsPhysical() == mVehicleSoaData.mPhysicalVehicles.IsBitSet( luVehicle )");
                CGS_ASSERT(GetVehicle(luVehicle)->HasEntity() == mVehicleSoaData.mVehiclesWithEntities.IsBitSet(luVehicle),
                           "GetVehicle( luVehicle )->HasEntity() == mVehicleSoaData.mVehiclesWithEntities.IsBitSet( luVehicle )");
            }
        }
    }
}
