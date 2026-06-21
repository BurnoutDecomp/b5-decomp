#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.h
// ============================================================================
// The ONLINE stunt-mode sub-scorer. PUBLICLY derives from BrnGameState::StuntModeScoring (NOT from
// BaseOnlineModeScoring -- despite the family name): every recovered body calls into the base via
// `BrnGameState::StuntModeScoring::X(this)` with the SAME `this` pointer (no offset adjustment) and
// the online overrides bind StuntModeScoring's virtual slots (HasStuntModeEnded @ vtable +0x14,
// CalculateMultiplier @ vtable +0x28). So this is single inheritance off StuntModeScoring; the online
// class APPENDS its own members after the base run.
//
// NO Feb-2007 partial source and NO DecFIGS DWARF exists for this TU -- the layout below is recovered
// purely from the X360 pseudocode + asm of its 17 functions (BrnStuntModeScoringOnline.cpp). Byte
// offsets are NOT byte-faithful on the PC gate (the base's PC sizeof differs from X360); member ORDER
// + named access are preserved for semantic parity. Single owner -- grow this slice, do not fork.
//
// ===== X360 online-member offset map (relative to the online `this`, beyond the base run) =====
//   +0x22D0  __int64    mCarsAroundPlayerAtTakeoff       -- 8-car bitset; UpdateCarsAroundPlayerAtTakeoff
//                                                           sets bit per nearby car; Construct/Destruct
//                                                           seed it 0x6_00000000 (free-queue init image)
//   +0x22E0..+0x23DF    mStoredLeapingDataPool           -- ObjectPool<StoredLeapingData,7,s32>
//                                                           (per-car leaping records persisted frame-to-frame)
//   +0x23E0  __int64    (pool free-queue/count tail of the StoredLeapingData pool)
//   +0x23F0..+0x2473    maChainableMultiplierInfo        -- Array<GameStateModuleIO::ChainableMultiplierInfo,8>
//                                                           (128B data @+0x23F0 + count @+0x2470)
//   +0x2478..+0x24F3    maMultiplierData                 -- Array<MultiplierData,3> (120B data + count @+0x24F0)
//   +0x24F8..+0x250F    mPendingMultiplier               -- 6-word(24B) pending-multiplier record (+score@+0x2504, +id@+0x250C)
//   +0x2510  s32        miOnlineDisplayScore             -- targetXcombo+current cached display score
//   +0x2514  bool       mbPendingMultiplierValid         -- a pending multiplier is queued for the game
//   +0x2515  bool       mbStuntModeEndedCached           -- cached HasStuntModeEnded result (online)
//   +0x2516  bool       mbCarsAroundPlayerCaptured       -- the at-takeoff car snapshot has been taken
//
// NOTE / FLAG (committed-base overlap): the *base* StuntModeScoring home (BrnStuntModeScoring.h) already
// modelled the +0x23F0 132-byte region as `maOnlineChainableMultiplierDisplay[132]` and +0x2515 as
// `mbOnlineChainableUpdateSuppressed`, calling them "StuntModeScoring online internals". The X360 asm
// proves those slots are owned by THIS derived class, and that +0x23F0 is a typed
// Array<ChainableMultiplierInfo,8> (data+count == exactly 132 bytes), not raw bytes. This TU declares
// its own typed members for them; reconciling the duplicate base-home approximation is the base home's
// concern (flagged for the conductor). We do NOT grow/retype the base home here.

#include "types.hpp"
#include "BrnCommonTypes.h"                                                    // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"                        // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"                   // CgsContainers::ObjectPool<T,N,TIndex>
#include "GameSource/GameState/BrnGameStateTypes.h"                           // BrnGameState::EStuntType
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"          // GameStateModuleIO::ChainableMultiplierInfo, StuntModeScoringOnline::MultiplierData
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"      // base class StuntModeScoring (+ StuntInfo, MultiplierOutInfo)

namespace BrnGameState
{
    // Forward-only -- the per-frame race-car output interface the Update path reads. Reached only BY
    // POINTER in the method decls; the StuntModeScoring base already publishes the typedef.
    namespace GameStateModuleIO
    {
        // The per-frame stunt-element output action Update unbuffers into (a3). Pointer-only.
        struct OnStuntElementCompleteAction;
    }

    // ------------------------------------------------------------------------
    // The online stunt-mode sub-scorer (embedded by value in ScoringSystem at the online slot).
    // ------------------------------------------------------------------------
    class StuntModeScoringOnline : public StuntModeScoring
    {
    public:
        // The base's ActiveRaceCarOutputInterface typedef (RCEntityActiveRaceCarOutputInterface).
        typedef StuntModeScoring::ActiveRaceCarOutputInterface ActiveRaceCarOutputInterface;

        // Number of active race cars / leaping-pool slots (== E_ACTIVE_RACE_CAR_INDEX_COUNT on this build).
        static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;
        // The StoredLeapingData / CarLeapingData pools hold one slot per non-local car (capacity 7).
        static const s32 KI_LEAPING_POOL_CAPACITY = 7;
        // The pending-multiplier record is a 6-word (24-byte) blob copied verbatim in/out.
        static const s32 KI_PENDING_MULTIPLIER_WORDS = 6;

        // --------------------------------------------------------------------
        // CarLeapingData / StoredLeapingData -- the per-car "is this car leaping over another car"
        // records the leap detector pools. Each is a 32-byte X360 stride (a Vector3 relative-position
        // at +0x00 and the EActiveRaceCarIndex at +0x10). The X360 build instantiates these pools as
        // ObjectPool<...,7,s32>; the explicit-instantiation .cpps already live under ChallengeManager/
        // (ObjectPool_CarLeapingData_7.cpp / ObjectPool_StoredLeapingData_7.cpp), so we name the SAME
        // shared record types here (one instance per layout on X360). Modelled as the byte-stride blob
        // the pool .cpps committed; the leap math reads the leading Vector3 + the +0x10 index.
        struct CarLeapingData
        {
            Vector3 mRelativePosition;   // +0x00 (the car-to-car relative position the leap test uses)
            s32     miRaceCarIndex;      // +0x10 (which car this record describes)
            u8      maPad14[12];         // +0x14 pad to the X360 32-byte stride
        };
        struct StoredLeapingData
        {
            Vector3 mRelativePosition;   // +0x00
            s32     miRaceCarIndex;      // +0x10
            u8      maPad14[12];         // +0x14 pad to the X360 32-byte stride
        };

        // The banked-multiplier element type (X360 stride 40). BankMultiplier (0x8232DE08) reserves a
        // slot via Array<MultiplierData,3>::AddNew() then fills it: a 1.0 weight @+0x00, the 6-word(24B)
        // pending-multiplier record @+0x04, and the cars-around-player-at-takeoff bitset snapshot @+0x20.
        // Update (0x82340C20) ages the weights + unbuffers; EndCombo (0x8232DD28) folds them. Field names
        // provisional (X360-proven offsets). This type owns the Array_MultiplierData_3.cpp instantiation.
        struct MultiplierData                                  // X360 stride 40
        {
            f32 mfWeight;                   // +0x00 (seeded 1.0 by BankMultiplier; aged by Update)
            s32 maRecord[6];                // +0x04 the 6-word pending-multiplier record from the recent stunt
            u64 mCarsAroundPlayerAtTakeoff; // +0x20 snapshot of the cars-around-player bitset at bank time
        };

        // --- the online lifecycle overrides (bind the base StuntModeScoring vtable slots) ---
        // The base spells these non-virtual (DWARF dropped the virtuality); the X360 asm proves the
        // overrides dispatch through the base vtable, so they are virtual on the base and here.
        void Construct();                                                            // X360 0x8232D060
        bool Prepare();                                                              // X360 0x82338B50
        bool Release();                                                              // X360 0x82321878 (ledger "Relase")
        void Destruct();                                                             // X360 0x8232D0F8
        void ClearData();                                                            // X360 0x82321968
        void Update(const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta);     // X360 0x82340C20
        virtual bool HasStuntModeEnded(bool lbTimeUp);                               // X360 0x82313680 (override base +0x14)
        bool WasStuntRecentlyPerformed(StuntInfo* lpStuntInfo);                      // X360 0x82313580

        // Combo overrides (online variant adds the chainable-multiplier banking on top of the base).
        void BeginCombo();                                                           // X360 0x82321908
        void EndCombo();                                                             // X360 0x8232DD28

        // Takedown award hook -- ScoringSystem::UpdateTakedowns forwards a recorded takedown here.
        void DealWithTakedown();                                                     // X360 0x82321890

        // VIRTUAL override of the base multiplier calculator (base slot +0x28). Adds the online
        // "near-miss" bonus + records the awesome-stunt flag into the out record. Signature matches
        // the base (const StuntInfo*, MultiplierOutInfo*); the X360 returns the multiplier total.
        virtual s32 CalculateMultiplier(const StuntInfo* lpStuntInfo,
                                        MultiplierOutInfo* lpMultiplierOutInfo);     // X360 0x82313620

        // BankMultiplier -- reserve a chainable-multiplier slot and seed it from a recent stunt.
        // X360 0x8232DE08: asserts lpRecentStunt + the reserved slot, writes a 1.0 weight + the
        // recent-stunt 6-word body + the carry chain, then resets the cached display score.
        void BankMultiplier(StuntInfo* lpRecentStunt);                               // X360 0x8232DE08

        // CalculateChainedMultipliers -- after a combo, sort the gathered chainable rows and fold the
        // matching stunt-type categories into a per-frame chained multiplier (filling the out record).
        // X360 0x8232D168 (call-site Update 0x82340D68 proves the 4 args: this, stunt-type-mask, the
        // cars-around-player-at-takeoff bitset, the out byte buffer). a3 is &slot.mCarsAroundPlayerAtTakeoff
        // (a u64 bitset, addi r5,r28,0x20), read bit-by-car to gate which rows are folded.
        s32 CalculateChainedMultipliers(u32 luStuntTypeMask, const u64* lpCarsAroundPlayer, u8* lpOut); // X360 0x8232D168

    private:
        // --- leap-detection helpers (file-private; Update drives them) ---
        // X360 0x8232D4E0. Per-frame: detect cars leaping over other cars (build CarLeapingData rows,
        // reconcile against the persisted StoredLeapingData pool, award the car-leap stunt on crossover).
        void UpdateCarLeapStunts(const ActiveRaceCarOutputInterface* lpRaceCar);
        // X360 0x8232DEC8. Snapshot which cars are near the local player at jump take-off (the
        // mCarsAroundPlayerAtTakeoff bitset) -- captured once per airborne window.
        void UpdateCarsAroundPlayerAtTakeoff(const ActiveRaceCarOutputInterface* lpRaceCar);

        // X360 0x823136B8. qsort comparator over GameStateModuleIO::ChainableMultiplierInfo: ranks the
        // higher sort-key (maField[3]) first. Emitted non-static on X360 (no `this` use); reconstructed
        // `static` for the (const void*, const void*) free-function shape qsort requires.
        static int _ChainableMultipliersSort(const void* lpData1, const void* lpData2);

        // --- online-specific data members (X360 declared order; offsets in the map above) ---

        // +0x22D0 -- bitset of cars near the local player when its jump took off (8 cars -> 64-bit word).
        u64 mCarsAroundPlayerAtTakeoff;

        // +0x22E0 -- pool of per-car leaping records persisted across frames (capacity 7).
        CgsContainers::ObjectPool<StoredLeapingData, KI_LEAPING_POOL_CAPACITY, s32> mStoredLeapingDataPool;

        // +0x23F0 -- the gathered/sorted chainable-multiplier rows for the local player's combo (8 max).
        Array<GameStateModuleIO::ChainableMultiplierInfo, KI_MAX_ACTIVE_RACE_CARS> maChainableMultiplierInfo;

        // +0x2478 -- the live banked multipliers awaiting expiry/unbuffering (capacity 3).
        Array<MultiplierData, 3> maMultiplierData;

        // +0x24F8 -- the pending-multiplier record handed to WasStuntRecentlyPerformed (6 words / 24B).
        // Copied verbatim in BankMultiplier-style stores; surfaced when mbPendingMultiplierValid is set.
        s32 maPendingMultiplier[KI_PENDING_MULTIPLIER_WORDS];   // +0x24F8 (score @ word[3]=+0x2504, id @ word[5]=+0x250C as u16)

        // +0x2510 -- cached display score: targetScore*comboMultiplier + currentScore (refreshed by
        // Construct/ClearData/BeginCombo/EndCombo/Update; surfaced to the online HUD).
        s32 miOnlineDisplayScore;

        // +0x2514 -- a banked multiplier is queued to be told to the game (WasStuntRecentlyPerformed drains it).
        bool mbPendingMultiplierValid;
        // +0x2515 -- cached online HasStuntModeEnded result (computed once, then sticky).
        bool mbStuntModeEndedCached;
        // +0x2516 -- the at-takeoff cars-around-player snapshot has been captured this airborne window.
        bool mbCarsAroundPlayerCaptured;

    public:
        // Named access for the cached online "mode ended" flag (Prepare/Relase/ClearData reset it).
        bool GetStuntModeEndedCached() const { return mbStuntModeEndedCached; }
        void SetStuntModeEndedCached(bool lbEnded) { mbStuntModeEndedCached = lbEnded; }
    };
}
