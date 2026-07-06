#pragma once

// BrnAI::BuzzBy -- the AI "buzz-by" director. During free-roam it maintains an active list of
// nearby AI race cars, ticks a per-car buzz timer, and when a car is eligible tells the
// ResetOnTrackManager to teleport ("buzz") it past the player for a near-miss thrill. Prepared
// once with the global race-car array + the reset-on-track manager.
//
// OFFSET AUTHORITY = the X360 asm of Update / ResetActiveList / StartABuzzBy /
// IsPositionInNoBuzzZone. Member NAMES/TYPES/ORDER = the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/World/AI/BrnAIBuzzBy.h). Pinned layout:
//   mfTimeInFreeRoam         @+0x000  f32   (Update reads/writes at this+0)
//   mbIsInGameMode           @+0x004  bool  (Update lbz 4)
//   mbIsInJunkyard           @+0x005  bool  (Update lbz 5)
//   mbResetBuzzTimers        @+0x006  bool  (Update lbz/stb 6)
//   mpGlobalRaceCars         @+0x008  AICar*         (Update lwz 8; stride 0x1560)
//   maeActiveList[35]        @+0x00C  EGlobalRaceCarIndex (s32)  -- ends +0x98
//   mafBuzzTimes[35]         @+0x098  f32                        -- ends +0x124
//   miNumActiveCars          @+0x124  s32   (Update/ResetActiveList lwz 0x124)
//   mpResetOnTrackManager    @+0x128  ResetOnTrackManager*  (StartABuzzBy lwz 0x128)
//   miCarsAwaitingCollection @+0x12C  s32
//
// The DWARF marks mRandom `extern` -- it is a FILE-SCOPE static in the .cpp (mirroring
// BrnRouteRequestManager's mRandom), NOT a per-instance member; it is not part of this layout.
// NOTE: past mpGlobalRaceCars the host widens the pointer (8B vs 4B on X360), so only the
// pointer-free head is static_asserted; the bodies reach every field by name (faithful).

#include <cstddef>   // offsetof

#include "types.hpp"                                    // s32/f32/u8/bool
#include "BrnCommonTypes.h"                             // Vector3
#include "GameSource/BurnoutConstants.h"                // EGlobalRaceCarIndex
#include "GameSource/World/AI/BrnAISharedConstants.h"   // BrnAI::EResetType, EAICarState

namespace BrnAI
{
    struct AICar;                 // pointer-only (opaque, sizeof == 0x1560 == 5472)
    struct ResetOnTrackManager;   // pointer-only collaborator
    namespace AIModuleIO { struct ResetOnTrackRequest; }

    // --- BrnAIBuzzBy.cpp file-scope tuning constants (DWARF names; runtime-initialised in the
    //     X360 data/rodata segment, grounded by the asm uses). Defined in the .cpp. ----------
    extern const f32 KF_ON_COMING_RESET_SPEED;       // flt_820C4318 = 200.0f
    extern const f32 KF_START_FAR_AHEAD;             // flt_8300DBEC (runtime)
    extern const f32 KF_START_FAR_BEHIND;            // flt_8300D7F4 (runtime)
    extern const f32 KF_MIN_SPEED_FOR_BUZZING;       // flt_8300D938 (runtime)
    extern const f32 KF_START_BEHIND_PROBABLITY;     // flt_820C4168 = 0.5f (spelling per DWARF)
    extern const f32 KF_SIDE_TURNING_PROBABILITY;    // flt_820C4330

    static const s32 KI_MAX_CARS_AWAITING_COLLECTION = 3;

    // No-buzz sphere count: the IsPositionInNoBuzzZone loop spans 0x70 bytes at a 16-byte
    // centre stride (byte_8300DBA0 - unk_8300DB30 = 0x70 = 7*16) with a +4 radius stride.
    static const s32 KI_NUM_NO_BUZZ_ZONES = 7;

    struct BuzzBy
    {
        // ---- public API (Update bodied in this batch) ------------------------------------
        void Update(f32 lfTimeStep, AICar* lpPlayerCar, AICar* lpBuzzCar, bool* lpbBuzzOccured);

        void Prepare(AICar* lpGlobalRaceCars, ResetOnTrackManager* lpResetOnTrackManager);
        void SetInGameMode(bool lbInGameMode);
        void SetInJunkyard(bool lbInJunkyard);
        void DrawBuzzTimer();
        void MaintainAheadOrBehind(AIModuleIO::ResetOnTrackRequest* lpRequest,
                                   Vector3 lPosition, Vector3 lDirection, Vector3 lPlayerPosition,
                                   Vector3 lPlayerVelocity, Vector3 lPlayerDirection);
        void SetCarsAwaitingCollection(s32 liCarsAwaitingCollection);
        void AddCarAwaitingCollection();
        void ClearCarsAwaitingCollection();

        // True when lPosition falls inside any of the KI_NUM_NO_BUZZ_ZONES no-buzz spheres
        // (distanceSq < radiusSq). X360 @0x82766FC0. Also called by
        // RouteRequestManager::GenerateFreeRoamingDestination (which holds a BuzzBy* and calls
        // this const -> the const qualifier is load-bearing for that sibling TU).
        bool IsPositionInNoBuzzZone(Vector3 lPosition) const;

        void RequestResetBuzzTimers();

    private:
        void ChooseAheadOrBehind(AIModuleIO::ResetOnTrackRequest* lpRequest, f32 lfPlayerSpeed,
                                 EGlobalRaceCarIndex leGlobalRaceCarToTeleport);
        void RunFreeBurnTimer(f32 lfTimeStep);
        bool IsPlayerBuzzable(AICar* lpPlayerCar);
        void StartABuzzBy(const AICar* lpPlayerCar, EGlobalRaceCarIndex leGlobalRaceCarToTeleport);
        bool AICarCanBuzz(const AICar* lpCar);
        bool BuzzOccured(const AICar* lpPlayerCar, const AICar* lpCar);
        void ResetActiveList();
        f32 GetBuzzFrequency(const AICar* lpAICar) const;

    private:
        f32                  mfTimeInFreeRoam;             // +0x000
        bool                 mbIsInGameMode;               // +0x004
        bool                 mbIsInJunkyard;               // +0x005
        bool                 mbResetBuzzTimers;            // +0x006
        u8                   mPad0007[1];                  // +0x007
        AICar*               mpGlobalRaceCars;             // +0x008
        EGlobalRaceCarIndex  maeActiveList[35];           // +0x00C .. +0x098
        f32                  mafBuzzTimes[35];            // +0x098 .. +0x124
        s32                  miNumActiveCars;              // +0x124
        ResetOnTrackManager* mpResetOnTrackManager;        // +0x128
        s32                  miCarsAwaitingCollection;      // +0x12C

        // Pointer-free spine offset pins (reached from inside the class so the private members
        // are accessible; never called).
        static void _AssertLayout()
        {
            static_assert(offsetof(BuzzBy, mfTimeInFreeRoam)  == 0x000, "BuzzBy::mfTimeInFreeRoam @ +0x000");
            static_assert(offsetof(BuzzBy, mbIsInGameMode)    == 0x004, "BuzzBy::mbIsInGameMode @ +0x004");
            static_assert(offsetof(BuzzBy, mbIsInJunkyard)    == 0x005, "BuzzBy::mbIsInJunkyard @ +0x005");
            static_assert(offsetof(BuzzBy, mbResetBuzzTimers) == 0x006, "BuzzBy::mbResetBuzzTimers @ +0x006");
        }
    };
}
