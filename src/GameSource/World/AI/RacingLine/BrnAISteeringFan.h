#pragma once

// BrnAI::SteeringFan -- the 17-ray "fan" of candidate driving targets an AIDriver weighs each
// round-robin tick (UpdateWeightings) and reads back every frame (GetDrivingTarget /
// GetSpeedRatio). Embedded BY VALUE in AIDriver @guest+0x710 (BrnAIDriver.h DWARF :540).
//
// SHAPE: DecFIGS DWARF GameSource/World/AI/RacingLine/BrnAISteeringFan.h:99 (member names,
// order, types) -- every one gated on the X360 asm below. The class is POINTER-FREE (Vector2/3
// SIMD slots, float tables, one enum, two ints, two bools), so its host size equals the console
// size (0x810 == 2064 bytes, pinned by the static_assert at the bottom): AIDriver can hold it in
// place at its guest offset.
//
// OFFSETS (X360 asm, the DWARF order agrees byte-for-byte):
//   SetBiasMode @0x827693E8:  meBiasMode == this[236] (+944 == 0x3B0); miStateCounter == this[513]
//                              (+2052 == 0x804).
//   Prepare     @0x82778E40:  mfWeighting @+948 (14 rows of 17), mfCumulativeWeighting @+1900,
//                              mTravelDirectionBias @+1968, mfReciprocalSteps @+2036,
//                              mfLookAheadRadius @+2040, mfFanAngle @+2048, mbPointAheadKnown @+2056.
//   GetBestIndex @0x82768D48: scans this+1900 .. +1968 (mfCumulativeWeighting[17]).
//   The three 17-slot Vector2 tables (0x00 / 0x110 / 0x220) and the eight single Vector2/3 slots
//   (0x330 .. 0x3A0) precede meBiasMode; their offsets fall out of the DWARF order and land
//   meBiasMode exactly on 0x3B0 (the pinned anchor).
//
// VISIBILITY: the data members are public here so the (few) bodied methods and the AIDriver
// bodies can reach them by name; the DWARF class keeps them private behind inlined accessors.

#include <cstddef>            // offsetof

#include "types.hpp"
#include "BrnCommonTypes.h"                 // Vector2 / Vector3 (rw::math::vpu, 16-byte SIMD)
#include "GameSource/BurnoutConstants.h"    // EGlobalRaceCarIndex

namespace BrnAI
{
    struct AICar;
    class  RacingLine;
    class  RacingLineGenerator;
    struct NearbyVehicles;
    struct NearbyVehicle;

    // DWARF BrnAISteeringFan.h:58
    const s32 KI_FAN_STEPS = 17;

    // DWARF BrnAISteeringFan.h:41 -- the enumerators are not carried by the DWARF dump. The X360
    // SetBiasMode asserts `mode < 10` ("Bad Bias mode set in Steering Fan" @0x827693E8), and the
    // AIDriver fan-choosers produce 0/1/2/4/5/6/7/8/9. Only the count is pinned here; the callers
    // pass the raw console values.
    enum EBiasMode
    {
        E_BIAS_MODE_COUNT = 10,
    };

    // DWARF BrnAISteeringFan.h:60 -- the contributor rows of mfWeighting[14][17]. Enumerator
    // names are not carried by the DWARF dump; only the row count is load-bearing.
    enum EFan_Contributors
    {
        E_FAN_CONTRIBUTORS_COUNT = 14,
    };

    // DWARF BrnAISteeringFan.h:99
    struct SteeringFan
    {
        // ---- bodied in BrnAISteeringFan.cpp (this wave) ------------------------------------
        void  Prepare();                                   // @0x82778E40
        void  SetBiasMode(EBiasMode leBiasMode);           // @0x827693E8
        s32   GetBestIndex();                              // @0x82768D48
        f32   GetSpeedRatio();                             // @0x82779B90

        // ---- X360-attested members bodied by their own recon pass (declare-only) -----------
        // [FLAG PC bring-up] BrnAISteeringFan.cpp's weighting/target machinery (26 functions) is
        // NOT reconstructed this wave; AIDriver gates every call to it behind
        // BRN_AI_RACINGLINE_STACK_PRESENT (BrnAIDriver.h). DELETE-WHEN those bodies land.
        Vector2 GetDrivingTarget(AICar* lpCar, RacingLine* lpRacingLine,
                                 RacingLineGenerator* lpRacingLineGenerator,
                                 const NearbyVehicles* lpNearbyVehicles,
                                 bool lbFlag);                                    // @0x82779C30 (DWARF :112)
        void  UpdateWeightings(AICar* lpCar, RacingLine* lpRacingLine,
                               RacingLineGenerator* lpRacingLineGenerator,
                               const NearbyVehicles* lpNearbyVehicles,
                               EGlobalRaceCarIndex leVictim);                     // @0x82794600 (DWARF :127)
        void  AccumulateWeightings();                                             // @0x82779088
        s32   GetMinMaxWeightings(float* lpWeightings, f32& lrMin, f32& lrMax);   // @0x82768AC8
        void  GenerateFanVectors(AICar* lpCar);                                   // @0x827792C0
        void  RenderFanVectors(s32 liIndex);                                      // @0x82779A20
        void  IncludeCentreLineTracking(RacingLineGenerator*, RacingLine*);       // @0x82786BC8
        void  IncludeHardNoGo(RacingLineGenerator*, RacingLine*);                 // @0x82779D98
        void  IncludeConstantBearing(RacingLine*, AICar*, const NearbyVehicles*); // @0x827873A0
        void  IncludeDriftDirectionTracking(RacingLineGenerator*, RacingLine*);   // @0x827881B0
        void  IncludeSmashIntoPlayer(AICar*, const NearbyVehicles*, EGlobalRaceCarIndex); // @0x82791230
        void  IncludeSmashIntoNearbyAI(AICar*, const NearbyVehicles*);            // @0x82791338
        void  IncludeSmashIntoTarget(AICar*, const NearbyVehicle*, EFan_Contributors, f32, f32); // @0x82787968
        const NearbyVehicle* FindPlayerInTraffic(const NearbyVehicles*);          // @0x82769510
        const NearbyVehicle* FindVictimInTraffic(const NearbyVehicles*, EGlobalRaceCarIndex); // @0x82769580
        void  IncludeDriveCloseToPlayer(RacingLine*, AICar*, const NearbyVehicles*); // @0x82787E58
        f32   AccumulateInArea(s32 liFrom, s32 liTo);                             // @0x82769000
        s32   BestTargetInArea(s32 liFrom, s32 liTo);                             // @0x82768E50
        void  IncludePreferCurrentDirection();                                    // @0x827694A8
        void  WriteWeightingValues(s32 liContributor);                            // @0x827691E0
        f32   FanIntersectsEdge(Vector2* lpEdge, s32 liIndex, Vector2 lA, Vector2 lB); // @0x8277A208
        void  IncludeRouteParallelTracking(RacingLineGenerator*, RacingLine*);    // @0x82786DB8
        void  IncludeDriftLocationTracking(RacingLineGenerator*, RacingLine*);    // @0x82788738
        void  CachePointAhead(RacingLineGenerator*, RacingLine*);                 // @0x827913C8

        // ---- storage (DWARF declaration order == layout order) ------------------------------
        Vector2     mTarget[KI_FAN_STEPS];                          // +0x000  DWARF :303
        Vector2     mHNGTarget[KI_FAN_STEPS];                       // +0x110  DWARF :304
        Vector2     mUnitDirection[KI_FAN_STEPS];                   // +0x220  DWARF :305
        Vector2     mCentreTarget;                                  // +0x330  DWARF :306
        Vector3     mFanOrigin;                                     // +0x340  DWARF :308
        Vector2     mFanOrigin2D;                                   // +0x350  DWARF :309
        Vector2     mPerpendicular;                                 // +0x360  DWARF :310
        Vector2     mCentreFarAhead;                                // +0x370  DWARF :312
        Vector2     mCentreAheadFarAhead;                           // +0x380  DWARF :313
        Vector2     mCentreHere;                                    // +0x390  DWARF :315
        Vector2     mCentreAhead;                                   // +0x3A0  DWARF :316
        EBiasMode   meBiasMode;                                     // +0x3B0  DWARF :319 (SetBiasMode: this[236])
        f32         mfWeighting[E_FAN_CONTRIBUTORS_COUNT][KI_FAN_STEPS]; // +0x3B4 DWARF :321 (Prepare: this+948)
        f32         mfCumulativeWeighting[KI_FAN_STEPS];            // +0x76C  DWARF :322 (GetBestIndex: this+1900)
        f32         mTravelDirectionBias[KI_FAN_STEPS];             // +0x7B0  DWARF :323 (Prepare: this+1968)
        f32         mfReciprocalSteps;                              // +0x7F4  DWARF :325 (Prepare: 1/16)
        f32         mfLookAheadRadius;                              // +0x7F8  DWARF :327 (Prepare: 10.0)
        f32         mfLookAheadHNGRadius;                           // +0x7FC  DWARF :328
        f32         mfFanAngle;                                     // +0x800  DWARF :329 (Prepare: flt_82F30428)
        s32         miStateCounter;                                 // +0x804  DWARF :331 (SetBiasMode: this[513])
        bool        mbPointAheadKnown;                              // +0x808  DWARF :333 (Prepare: this+2056)
        bool        mbCentreHereKnown;                              // +0x809  DWARF :334
        // (padding to the 16-byte SIMD alignment brings the size to 0x810.)
    };

    // The X360 layout anchors (pointer-free type, so the host offsets are the console offsets).
    static_assert(offsetof(SteeringFan, meBiasMode)            == 0x3B0, "SteeringFan::meBiasMode @ +944 (SetBiasMode this[236])");
    static_assert(offsetof(SteeringFan, mfWeighting)           == 0x3B4, "SteeringFan::mfWeighting @ +948 (Prepare)");
    static_assert(offsetof(SteeringFan, mfCumulativeWeighting) == 0x76C, "SteeringFan::mfCumulativeWeighting @ +1900 (GetBestIndex)");
    static_assert(offsetof(SteeringFan, mTravelDirectionBias)  == 0x7B0, "SteeringFan::mTravelDirectionBias @ +1968 (Prepare)");
    static_assert(offsetof(SteeringFan, mfReciprocalSteps)     == 0x7F4, "SteeringFan::mfReciprocalSteps @ +2036 (Prepare)");
    static_assert(offsetof(SteeringFan, mfFanAngle)            == 0x800, "SteeringFan::mfFanAngle @ +2048 (Prepare)");
    static_assert(offsetof(SteeringFan, miStateCounter)        == 0x804, "SteeringFan::miStateCounter @ +2052 (SetBiasMode this[513])");
    static_assert(sizeof(SteeringFan)                          == 0x810, "SteeringFan is 0x810 bytes (AIDriver @0x710 .. @0xF20)");
}
