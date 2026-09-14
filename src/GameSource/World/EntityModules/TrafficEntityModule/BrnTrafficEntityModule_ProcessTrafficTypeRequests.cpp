// ============================================================================
// BrnTrafficEntityModule_ProcessTrafficTypeRequests.cpp
//
// TrafficEntityModule::ProcessTrafficTypeRequests @0x8272B880 -- the ANSWERING half of the
// traffic-type query, and the middle of the three hops that decide whether a takedown is a
// "takedown car" / "takedown van" / "takedown bus".
//
// THE CHAIN, END TO END:
//   * PRODUCER OF THE REQUEST. VehicleManager::SetRaceCarCrashing's owner==2 arm (the crashing
//     car was hit by a TRAFFIC vehicle) republishes that traffic car's GLOBAL entity index onto
//     VehicleManagerOutputInterface::mTrafficTypeRequestQueue @+0x750, via
//     AddRemappedEntityIdEvent (BrnVehicleManagerOutputInterface.cpp).
//   * CARRIER. WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics @0x827AB910 leg 2
//     copies the whole VehicleManagerOutputInterface -- mTrafficTypeRequestQueue included, its
//     operator= Clear()+Append()s it -- into the traffic module's InputBuffer_PostPhysics.
//   * THIS FUNCTION. TrafficEntityModule::PostPhysicsUpdate @0x8274EEA4..0x8274EEC4 fetches
//     `GetVehicleManagerOutputInterface() + 0x750` and the output buffer's
//     GetTrafficTypeResponseQueue() (the WRITE overload @0x82711EE0) and calls it.
//   * CONSUMERS. The response queue rides WorldBridgeEntityModulesToOutput leg 7 @0x827AF02C
//     into UpdateOutputBuffer::mTrafficTypeResponseQueue, then BridgeWorldToGameState into
//     GameStateModule's takedown cache, and lands as TakedownManager::DetectStandardTakedown's
//     `lpLastTrafficTypeResponseQueue`. There, GetTakedownTypeFromTrafficVehicleIndex
//     @0x82366288 matches the crasher's entity index against muVehicleIndex and maps meType ->
//     E_TAKEDOWN_TYPE_INTO_CAR / _INTO_VAN / _INTO_BUS. With this queue empty it falls to
//     E_VEHICLECLASS_CAR *and* fires "Missing traffic vehicle check!" (BrnTakedownManager.cpp:842).
//
// ⚠️ THIS FUNCTION IS AN ARTIST EXPORT HOLE -- no 0x8272B880.json, no ledger row, no
// pseudocode. The body below is transcribed from the 96 instructions read out of the image with
// `python tools/re/ppcdis.py 0x8272B880`, cross-checked against the PS3 twin
// (.ida-exports/Burnout_External_PS3.ELF/0x4C4C1C.json), whose mangled name
// `_ZN10BrnTraffic19TrafficEntityModule26ProcessTrafficTypeRequestsEPKN9CgsModule10EventQueueItLi32EEEPNS2_INS_12BrnTrafficIO19TrafficTypeResponseELi32EEE`
// gives the exact signature, and against the DWARF declaration
// (BrnTrafficEntityModule.h:1809 / BrnTrafficEntityModule.cpp:15533).
//
// THE X360 BODY, instruction for instruction:
//   8272B8A0  lwz   r11, 8(r26)          -- lpRequestQueue->GetLength(), re-read each iteration
//   8272B8A8  ble   cr6, <return>           (0x8272B9F0 reloads it and 0x8272B9FC re-compares)
//   8272B8EC  bl    EventQueue<u16,32>::GetEvent(r26, i)
//   8272B8F0  lhz   r11, 0(r3)           -- the requested vehicle index
//   8272B8F8  cmplwi r31, 0x258 / assert -- 600 == KU_MAX_TOTAL_TRAFFIC, BrnTrafficEntityModule.h:2459
//   8272B8FC  sth   r11, 0x50(r1)        -- response.muVehicleIndex, stored UNCONDITIONALLY,
//                                           before the alive test, at record offset +0
//   8272B91C  addi  r11, r31, 0x55 / slwi 7 / add r25
//                                        -- &maVehicles[index] (base element 85, stride 128)
//   8272B928  lbz   5(r30) & 1 / beq     -- Vehicle::IsAlive()  (E_FLAG_ALIVE)
//   8272B93C  lbz   0(r30)               -- Vehicle::GetVehicleType()
//   8272B948  lhz   0x16(r3) / assert    -- < mpData->muNumVehicleTypes, baked .cpp line 15763
//   8272B970  lbz   5(r30) & 1 / assert  -- "IsAlive()", BrnTrafficVehicle.h:786
//   8272B9B0  lwz   0x2C(r11)            -- mpData->mpaVehicleTypes
//   8272B9B4  add   r31, r11, type*8     -- &mpaVehicleTypes[type]  (stride 8)
//   8272B9B8  lbz   3(r31) / stw 0x54(r1)-- muVehicleClass -> response.meType (record +4)
//   8272B9C4  lbz   5(r31)               -- muAssetId
//   8272B9CC  lwz   0x34(r3) / ldx       -- mpData->mpaVehicleAssets[assetId].GetVehicleId()
//   8272B9D4  std   0x58(r1)             -- -> response.mTypeId (record +8)
//   8272B9DC  stw/std r17(==0)           -- the NOT-alive arm: class 0 and id 0
//   8272B9EC  bl    EventQueue<TrafficTypeResponse,32>::AddEvent(r18, &response)
//
// Every console offset above is provenance for WHICH member; nothing below is offset arithmetic.
// mpaVehicleTypes +3 / +5 are muVehicleClass / muAssetId (SharedClasses/Traffic/
// BrnTrafficVehicleType.h:92/:94), exactly as the two sibling lookups in
// _wG_NearbyTrafficResults.cpp and _wQ7_02.cpp already spell them.
//
// The "IsAlive()" tripwire is reproduced at the USE SITE rather than inside
// Vehicle::GetVehicleType(), which is this tree's standing convention for this accessor
// (BrnTrafficVehicle.h:370 -- "no IsAlive guard inside the getter, since the caller asserts
// IsAlive() at each use site"; same disposition as _SympatheticCrash.cpp:214).
//
// ⛔ THE NOT-ALIVE ARM IS NOT AN ERROR PATH AND MUST NOT BE SKIPPED. The console appends a
// response for EVERY request, alive or not -- a dead slot answers class 0
// (E_VEHICLECLASS_CAR) and a zero CgsID. Dropping it would desynchronise nothing by itself,
// but it WOULD make the miss silent on the consumer side, which is exactly the failure this
// wave is here to remove.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"  // TrafficTypeRequestQueue
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"                      // TrafficData
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"                           // VehicleTypeData, VehicleClass
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdlib>                                                                 // std::getenv

namespace BrnTraffic
{
    void TrafficEntityModule::ProcessTrafficTypeRequests(
        const CgsModule::EventQueue<u16, 32>* lpRequestQueue,
        CgsModule::EventQueue<BrnTrafficIO::TrafficTypeResponse, 32>* lpResponseQueue)
    {
        // The console re-reads the source length at the bottom of every iteration
        // (0x8272B9F0 `lwz r11, 8(r26)`), so the loop condition is a live read, not a cached count.
        for (s32 liRequest = 0; liRequest < lpRequestQueue->GetLength(); ++liRequest)
        {
            const u32 luVehicleIndex = lpRequestQueue->GetEvent(liRequest);

            BrnTrafficIO::TrafficTypeResponse lResponse;
            lResponse.muVehicleIndex = static_cast<u16>(luVehicleIndex);

            // Inlines TrafficEntityModule::GetVehicle -- the same
            // "luIndex < KU_MAX_TOTAL_TRAFFIC" tripwire at BrnTrafficEntityModule.h:2459.
            const Vehicle* const lpVehicle = GetVehicle(luVehicleIndex);

            if (lpVehicle->IsAlive())
            {
                CGS_ASSERT(lpVehicle->GetVehicleType() < mpData->muNumVehicleTypes,
                           "lpVehicle->GetVehicleType() < mpData->muNumVehicleTypes");  // baked .cpp 15763
                CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");                          // BrnTrafficVehicle.h:786

                const VehicleTypeData* const lpVehicleTypeData =
                    &mpData->mpaVehicleTypes[lpVehicle->GetVehicleType()];

                lResponse.meType  = static_cast<VehicleClass>(lpVehicleTypeData->muVehicleClass);
                lResponse.mTypeId = mpData->mpaVehicleAssets[lpVehicleTypeData->muAssetId].GetVehicleId();
            }
            else
            {
                // The console fills both fields from the same zeroed register (r17, `li r17, 0`
                // at 0x8272B890; the stores are at 0x8272B9DC/0x8272B9E0). Class 0 IS
                // E_VEHICLECLASS_CAR, so it is spelled by name -- but note the meaning is
                // "nothing to report for this slot", not a measured car.
                lResponse.meType  = E_VEHICLECLASS_CAR;
                lResponse.mTypeId = static_cast<CgsID>(0);
            }

            lpResponseQueue->AddEvent(lResponse);

            // [td-type] PC witness (BRN_TD_DIAG), NOT in the X360 binary: the one line that tells
            // a run whether the request reached the traffic module and what class it answered.
            // Without it the only observable is the takedown manager's silent fall-through to
            // E_VEHICLECLASS_CAR. [FLAG PC witness]
            // DELETE-WHEN: "takedown bus" has been seen in a real run.
            {
                static const bool sbTdDiag = (std::getenv("BRN_TD_DIAG") != 0);
                static s32 siPrinted = 0;
                if (sbTdDiag && siPrinted < 64 && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++siPrinted;
                    static const char* const kpacClassNames[] = { "car", "van", "bus", "bigrig" };
                    const s32 liClass = static_cast<s32>(lResponse.meType);
                    *CgsDev::Log::gpDebugPrint
                        << "[td-type] answered index=" << static_cast<s32>(luVehicleIndex)
                        << " alive=" << (lpVehicle->IsAlive() ? 1 : 0)
                        << " class=" << liClass
                        << "(" << ((liClass >= 0 && liClass < 4) ? kpacClassNames[liClass] : "?") << ")"
                        << " requests=" << lpRequestQueue->GetLength()
                        << " responses=" << lpResponseQueue->GetLength()
                        << " [FLAG PC witness]\n";
                }
            }
        }
    }
}
