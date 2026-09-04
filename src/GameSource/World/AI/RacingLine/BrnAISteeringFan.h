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

    // DWARF BrnAISteeringFan.h:41 (aiwave R6: the DecFIGS dwarfdump DOES carry the enumerators --
    // references/DecFIGS/dwarfdump/GameSource/World/AI/RacingLine/BrnAISteeringFan.h:3). The X360
    // SetBiasMode asserts `mode < 10` ("Bad Bias mode set in Steering Fan" @0x827693E8); the
    // AIDriver fan-choosers produce 0/1/2/4/5/6/8/9. E_BIAS_MODE_COUNT is kept as an alias of
    // eBiasMode_Count so the pre-R6 call sites keep compiling.
    enum EBiasMode
    {
        eBiasMode_First               = 0,
        eBiasMode_Race                = 0,
        eBiasMode_RaceDangerous       = 1,
        eBiasMode_Slam                = 2,
        eBiasMode_RoadRage            = 3,
        eBiasMode_CloseToPlayer       = 4,
        eBiasMode_SlamRivals          = 5,
        eBiasMode_HitOncoming         = 6,
        eBiasMode_SlamDangerous       = 7,
        eBiasMode_DontCentreWithinHNG = 8,
        eBiasMode_VeerAwayFromPlayer  = 9,
        eBiasMode_Count               = 10,
        E_BIAS_MODE_COUNT             = 10,
    };

    // DWARF BrnAISteeringFan.h:60 -- the contributor rows of mfWeighting[14][17]. Enumerator names
    // from the DecFIGS dwarfdump (BrnAISteeringFan.h:20); each row is confirmed by the kfBias
    // column UpdateWeightings @0x82794600 tests before calling that row's Include* member, and by
    // the row base each Include* body writes (e.g. IncludePreferCurrentDirection @0x827694A8 writes
    // this+0x5D4 == mfWeighting[8], IncludeRouteParallelTracking @0x82786DB8 writes this+0x590 ==
    // mfWeighting[7], IncludeCentreLineTracking @0x82786BC8 writes this+0x3B4 == mfWeighting[0]).
    enum EFan_Contributors
    {
        eFan_First                   = 0,
        eFan_SteerToCentre           = 0,   // IncludeCentreLineTracking       @0x82786BC8
        eFan_AvoidHNG                = 1,   // IncludeHardNoGo                 @0x82779D98
        eFan_ExitHNG                 = 2,   // IncludeHardNoGo                 @0x82779D98
        eFan_FavourHNGDanger         = 3,   // (IncludeHardNoGo's own third row)
        eFan_AvoidTraffic            = 4,   // IncludeConstantBearing          @0x827873A0
        eFan_AvoidOncomingTraffic    = 5,   // IncludeConstantBearing          @0x827873A0
        eFan_AvoidEdges              = 6,   // IncludeRouteEdgeIntersection    @0x8277A378
        eFan_DriveParallel           = 7,   // IncludeRouteParallelTracking    @0x82786DB8
        eFan_PreferCurrentDirection  = 8,   // IncludePreferCurrentDirection   @0x827694A8
        eFan_DriftFinalDirection     = 9,   // IncludeDriftDirectionTracking   @0x827881B0
        eFan_DriftFinalLocation      = 10,  // IncludeDriftLocationTracking    @0x82788738
        eFan_SmashIntoPlayer         = 11,  // IncludeSmashIntoPlayer          @0x82791230
        eFan_DriveCloseToPlayer      = 12,  // IncludeDriveCloseToPlayer       @0x82787E58
        eFan_SmashIntoRivals         = 13,  // IncludeSmashIntoNearbyAI        @0x82791338
        eFan_Count                   = 14,
        E_FAN_CONTRIBUTORS_COUNT     = 14,
    };

    // DWARF BrnAISteeringFan.cpp:68 -- `float32_t[10][14] kfBias`, the per-bias-mode multiplier
    // AccumulateWeightings @0x82779088 applies to each contributor row
    // (`base + meBiasMode*0x38 + contributor*4`, image address flt_82F30468). DEFINED in
    // BrnAISteeringFan_Target.cpp; NON-const because the console's is (see that file's banner for
    // where each cell's value came from).
    extern f32 kfBias[E_BIAS_MODE_COUNT][E_FAN_CONTRIBUTORS_COUNT];

    // DWARF BrnAISteeringFan.h:99
    struct SteeringFan
    {
        // ---- bodied in BrnAISteeringFan.cpp (this wave) ------------------------------------
        void  Prepare();                                   // @0x82778E40
        void  SetBiasMode(EBiasMode leBiasMode);           // @0x827693E8
        s32   GetBestIndex();                              // @0x82768D48
        f32   GetSpeedRatio();                             // @0x82779B90

        // ---- bodied in BrnAISteeringFan_Target.cpp / _Weightings.cpp (aiwave R6) -----------
        // The fan target machinery. Signatures are the DecFIGS DWARF's
        // (dwarfdump/.../BrnAISteeringFan.cpp), each gated on the X360 register map in the
        // reconstructing .cpp. Members that still have NO body carry a [FLAG PC bring-up] park in
        // BrnAISteeringFan_Weightings.cpp -- they are never silently absent.
        Vector2 GetDrivingTarget(AICar* lpCar, RacingLine* lpRacingLine,
                                 RacingLineGenerator* lpRacingLineGenerator,
                                 const NearbyVehicles* lpNearbyVehicles,
                                 bool lbFlag);                                    // @0x82779C30 (DWARF :112)
        void  UpdateWeightings(AICar* lpCar, RacingLine* lpRacingLine,
                               RacingLineGenerator* lpRacingLineGenerator,
                               const NearbyVehicles* lpNearbyVehicles,
                               EGlobalRaceCarIndex leVictim);                     // @0x82794600 (DWARF :127)
        // @0x82779088 -- returns `this` (the X360 threads r3 straight back out; GetDrivingTarget
        // @0x82779C5C feeds that return value into BestTargetInArea as its r3).
        SteeringFan* AccumulateWeightings();
        // DWARF: GetMinMaxWeightings(float* lfWeighting, const float32_t& lfBestWeighting,
        // const float32_t& lfWorstWeighting) -- arg 2 is the MAX (best), arg 3 the MIN (worst);
        // the pre-R6 names had them the wrong way round. Returns the index of the max (-1 if none).
        s32   GetMinMaxWeightings(f32* lpWeightings, f32& lrBestWeighting, f32& lrWorstWeighting); // @0x82768AC8
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

        // Named by UpdateWeightings @0x82794640 / @0x8279479C; NEITHER has an IDA export
        // (.ida-exports has no 0x82768CB0.json / 0x8277A378.json) -- both are parked bodies in
        // BrnAISteeringFan_Weightings.cpp.
        void  CalculateFanAngle(AICar* lpCar);                                    // @0x82768CB0
        void  IncludeRouteEdgeIntersection(RacingLineGenerator*, RacingLine*);    // @0x8277A378

        // DWARF BrnAISteeringFan.cpp:1248 -- the rival picker IncludeSmashIntoNearbyAI uses.
        const NearbyVehicle* FindNeabyAIInTraffic(const NearbyVehicles*, AICar*); // (sic: DWARF spelling)
        void  RenderEdge(s32 liIndex, u32 luColour);                              // @0x82779770
        void  RenderContributor(s32 liContributor);                               // @0x82779830

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
