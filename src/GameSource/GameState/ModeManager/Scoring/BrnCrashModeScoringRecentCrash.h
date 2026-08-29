#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // EntityId, CgsID (typedef u64)
#include "GameSource/BurnoutConstants.h"                      // EActiveRaceCarIndex
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"      // BrnTraffic::VehicleClass
#include "GameShared/GameClasses/Containers/CgsArray.h"       // Array<RecentCrash,64> maRecentCrashes
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"  // CgsContainers::FixedRingBuffer<u16,8> mRecentlyHitPropSet

// --- crash-mode event / interface parameter types (pointer-only in the decls below) ---
// These belong to other (not-yet-homed in this scope) TUs; CrashModeScoring names them only by
// pointer, so a forward declaration keeps the keystone's by-value embed leak-free. The handlers
// that must DEREFERENCE one of these are bodied to what compiles and FLAGGED in the .cpp.
struct CrashComboItemEvent;
struct TriggerCrashBreakerEvent;
struct PickupEvent;
struct VehicleLeaptEvent;

// [showtime-score 2026-08-29] DealWithShowtimeStunt names its argument by pointer only, so a
// forward declaration keeps BrnGameActions.h out of this keystone header (the by-value embed
// rule above). The definition lives in GameSource/GameState/BrnGameActions.h.
namespace BrnGameState { namespace GameStateModuleIO { struct WorldStuntAction; } }

// ⛔⛔ THE CLASS-KEY IS LOAD-BEARING ON MSVC, AND THIS ONE WAS WRONG (fixed 2026-08-29).
// The real type is `struct RCEntityActiveRaceCarOutputInterface`
// (BrnRaceCarEntityModuleOutputInterface.h:139). This forward declaration said `class`, and
// MSVC mangles the class-key into the decorated name -- `PEBV...` for class, `PEBU...` for
// struct -- so Update's DEFINITION (this header included first, class-key latched to `class`)
// and its CALL SITE (the real header included first, `struct`) decorated to two different
// symbols. It was latent for as long as nothing called Update; the moment the ModeManager arm
// was unparked it surfaced as one LNK2019 against a function whose body is right here.
// [[shadowing-redeclarations]] -- only a LINK finds it, and it names the caller, not the cause.
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }
namespace VehicleOutputInterface { class PhysicalTrafficStateQueue; }

// Minimal element-type home for the fixed-capacity
// Array<BrnGameState::CrashModeScoring::RecentCrash, 64> leaf instantiation (the IsFull/
// Append/Erase explicit instantiations of CrashModeScoring's recent-hit-cars set).
// RecentCrash is a nested record of the BrnGameState::CrashModeScoring struct; only the
// nested element type this Array needs is declared here (CrashModeScoring's own
// members/methods belong to the BrnCrashModeScoring.cpp TU -- grow this home in place,
// do not fork). Fields/offsets recovered from DecFIGS DWARF
// (BrnCrashModeScoring.h:200-204): X360 element stride 8 bytes, matching the X360
// Array layout (64 * 8 == 512 == count-word offset 0x200). Capacity 64 ==
// BrnGameState::KI_MAX_RECENTLY_HIT_CARS (BrnCrashModeScoring.h:44).
namespace BrnGameState
{
// The crash-score debug overlay is embedded as CrashModeScoring's first member and reads its private
// scoring state directly (BrnCrashModeScoring.h:215); declared here for the friend grant below.
class CrashScoreDebugComponent;

struct CrashModeScoring
{
    // Fixed capacity of the recent-hit-cars set (BrnCrashModeScoring.h:44). 64 * 8-byte stride
    // == 0x200 == the count-word offset within the Array<> instantiation.
    static const u32 KI_MAX_RECENTLY_HIT_CARS = 64;

    struct RecentCrash
    {
        u16 muTrafficCarIndex; // 0x00  BrnCrashModeScoring.h:202
        u16 muCrashChainCount; // 0x02  BrnCrashModeScoring.h:203
        f32 mfTimeOfCrash;     // 0x04  BrnCrashModeScoring.h:204
    };

    // --- slice grown for BrnCrashScoreDebugComponent.cpp (the "Showtime score" debug HUD) ---
    // Live crash-scoring accessors the debug readout reads (DWARF BrnCrashModeScoring.h:171-186).
    // Declared-only: the bodies (and CrashModeScoring's full member layout / remaining methods) belong
    // to the BrnCrashModeScoring.cpp TU -- grow this home in place when that lands, do NOT fork.
    s32 GetScoreMultiplier() const;     // BrnCrashModeScoring.h:171
    s32 GetCurrentComboCount() const;   // BrnCrashModeScoring.h:174
    s32 GetNumCarsLeapt() const;        // BrnCrashModeScoring.h:180
    f32 GetBestAirTime() const;         // BrnCrashModeScoring.h:183 (reads mfLongestJumpAirTime, +0x31C)
    f32 GetDistanceTravelled() const;   // BrnCrashModeScoring.h:186 (+0x308)

    // Members the debug overlay touches that have no accessor in the DWARF: OnActivate registers the
    // infinite-crash flag, and DisplayScores reads mfHighestJump directly (the X360 reads +0x320 inline;
    // CrashModeScoring's getter set is closed -- there is no GetHighestJump). Both members are now
    // carved at their asm-proven offsets in the full layout block below (mbInfiniteCrashMode @+0x55,
    // mfHighestJump @+0x320) so the debug overlay still names them by hand.

    // --- slice grown so BrnGameState::ScoringSystem can embed CrashModeScoring BY VALUE and compile ---
    // ScoringSystem holds a CrashModeScoring sub-scorer member; it drives the lifecycle and reads the
    // remaining live-score accessors through it. Declared-only (bodies + full layout live in the
    // BrnCrashModeScoring.cpp TU -- grow this home in place when that lands, do NOT fork).
    //
    // Only the methods whose signatures use already-available types (primitives + the nested RecentCrash)
    // are sliced in here so the header stays self-contained / leak-free. The remaining DWARF methods take
    // not-yet-homed event/traffic types (Update's interface params, DealWith* event ptrs,
    // DealWithScoreForVehicleClass / GetVehicleScoreData's BrnTraffic::VehicleClass &
    // VehicleScoreCategory & CgsID, DealWithShowtimeStunt's WorldStuntAction); those land with this
    // type's own TU rather than dragging their includes into the keystone's by-value embed.
    void Construct();                       // BrnCrashModeScoring.h:70
    bool Prepare();                         // BrnCrashModeScoring.h:74
    bool Release();                         // BrnCrashModeScoring.h:85
    void Destruct();                        // BrnCrashModeScoring.h:89
    void ClearData();                       // BrnCrashModeScoring.h:93
    void DealWithHitOverheadSign();         // BrnCrashModeScoring.h:125
    void DealWithDetachedWheel();           // BrnCrashModeScoring.h:141
    bool HasCrashModeEnded(f32 lfTime) const; // BrnCrashModeScoring.h:161
    s32  GetRawScore() const;               // BrnCrashModeScoring.h:165
    s32  GetOverallScore() const;           // BrnCrashModeScoring.h:168
    s32  GetNumCarsCrashed() const;         // BrnCrashModeScoring.h:177

    // --- slice grown for this type's own TU (BrnCrashModeScoring.cpp) ---
    // GetRecentCrash linearly scans the live recent-hit-cars set for the element whose
    // muTrafficCarIndex matches luTrafficCarIndex, returning a mutable pointer to it (or
    // null if absent). The X360 body (0x8232BEF8) walks maRecentCrashes via the bounds-
    // checked Array<>::operator[] (the explicit instantiation in Array_RecentCrash_64.cpp).
    // Called by DealWithHitTrafficCar / DealWithScoreForVehicleClass.
    RecentCrash* GetRecentCrash(u16 luTrafficCarIndex);   // BrnCrashModeScoring.h (X360 0x8232BEF8)

    // --- slice grown for this type's own TU (BrnCrashModeScoring.cpp), batch of 13 ---
    // The crash-event handlers + per-frame Update that mutate the live-scoring members named
    // below. Signatures are DWARF-authoritative; bodies are reconstructed store-for-store from
    // each method's X360 dossier (see BrnCrashModeScoring.cpp). Event/interface parameter types
    // that are NOT yet homed in this scope are forward-declared (pointer-only) so the keystone's
    // by-value embed stays leak-free; the handlers that must DEREFERENCE such a type are bodied
    // to what compiles and FLAGGED in the .cpp.
    void DealWithComboItem(const CrashComboItemEvent* lpComboItemEvent);            // X360 0x82312918
    void DealWithCrashbreakerRequest(const TriggerCrashBreakerEvent* lpEvent);      // X360 0x82320EB8
    void DealWithHitProp(u16 luPropIndex, u8 luPropFlags);                          // X360 0x82320DC8
    bool DealWithHitTrafficCar(EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex,
                               EntityId lEntityIdA, EntityId lEntityIdB,
                               u16* lpOutVictimIndex);                              // X360 0x82338558
    void DealWithPickup(const PickupEvent* lpPickupEvent);                          // X360 0x82312970
    void DealWithScoreForVehicleClass(u16 luTrafficEntityIndex,
                                      BrnTraffic::VehicleClass leVehicleClass,
                                      CgsID lVehicleTypeID,
                                      s32* lpiVehicleTypeCrashed,
                                      s32* lpiVehicleBaseScore,
                                      BrnTraffic::VehicleScoreCategory* lpeVehicleScoreCategory,
                                      s32* lpiScoreMultiplierEarned,
                                      s32* lpiComboBonusEarned);                    // X360 0x82338778
    void DealWithVehicleLeaping(const VehicleLeaptEvent* lpLeapEvent);             // X360 0x82312980

    // [showtime-score 2026-08-29] X360 0x82320F38. The recent-stunt de-dupe: record this stunt
    // element's 64-bit CgsID in mRecentStuntSet unless the live window already holds it.
    // Sole caller: BrnGameModule::TranslateShowtimeActionToGuiEvent @0x823E1988 case 127.
    void DealWithShowtimeStunt(const GameStateModuleIO::WorldStuntAction* lpStuntAction);
    void GetVehicleScoreData(BrnTraffic::VehicleClass leVehicleClass,
                             CgsID lVehicleTypeID,
                             s32* lpiScore, s32* lpiMultiplier,
                             BrnTraffic::VehicleScoreCategory* lpeCategory);        // X360 0x82312AB0
    bool IsActiveCrash(const RecentCrash* lpCrash) const;                          // X360 0x82312A30
    void Update(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                const VehicleOutputInterface::PhysicalTrafficStateQueue* lpTrafficStateQueue,
                f32 lfSimTimeStep);                                                 // X360 0x82320808

    // === Full live-scoring member layout (BrnCrashModeScoring.cpp TU) ===
    // The byte offsets below are PROVEN by the X360 asm of this TU's methods (the ClearData /
    // DealWith* / Update store offsets pin every named member; see the OFFSET->MEMBER MAP in
    // BrnCrashModeScoring.cpp). DWARF member ORDER (BrnCrashModeScoring.h:215-258) is authoritative
    // for naming; the X360 store offsets are authoritative for placement. This GROWS the prior
    // NOMINAL opaque reserve ADDITIVELY: every member is carved at its proven offset, the
    // maRecentCrashes@+0x7C anchor (count word @+0x27C) is preserved, and the total footprint is the
    // asm-proven 0x32C (the prior 0x320 reserve was explicitly NOMINAL / not byte-verified; the
    // crash-mode-scorer's own TU asm reaches +0x328 as the last member, so the true size is 0x32C).
    //
    // Three sub-objects are kept as opaque byte storage to keep this home leak-free / decoupled from
    // the keystone's by-value embed (they are not touched by name by any of this TU's 13 methods):
    //   - the embedded CrashScoreDebugComponent (0x20 bytes; its real type derives from
    //     CgsDev::DebugComponent and is homed in BrnCrashScoreDebugComponent.h),
    //   - the CgsID stunt-set ring (only Construct/Clear, not in this batch, touch it).
    // (The two Vector3 player-position members WERE in that opaque list; they are real Vector3s
    //  as of the showtime-score wave 2026-08-29, which bodies the Update half that reads them.
    //  Same 16 bytes each, same offsets -- nothing is shifted; only the spelling changes, so the
    //  distance math can be written with named members instead of a byte blob.)

    u8  maCrashScoreDebugComponent[0x20];     // +0x00  CrashScoreDebugComponent (h:215; opaque sub-object)
    Vector3 mPlayerPosLastFrame;              // +0x20  h:219  (Update's `stvx128 v127, r0, this+0x20` tail)
    Vector3 mPlayerPosLastStored;             // +0x30  h:220  (the distance-direction anchor, re-stored every 20 m)
    f32 mfTimeSincePlayerCarMoved;            // +0x40  h:221
    f32 mfTimeSinceLastEvent;                 // +0x44  h:222
    f32 mfTimeSinceModeStart;                 // +0x48  h:223  (the running crash clock IsActiveCrash reads)
    f32 mfDistanceUntilStorePosition;         // +0x4C  h:224
    f32 mfPlayerBoostPercentage;              // +0x50  h:225
    bool mbPlayerIsCrashing;                  // +0x54  h:226
    bool mbInfiniteCrashMode;                 // +0x55  h:227  (debug overlay registers this flag)
    u8  maPad0x56[2];                         //         alignment pad up to the prop ring

    CgsContainers::FixedRingBuffer<u16, 8> mRecentlyHitPropSet; // +0x58  h:231 (KI_MAX_RECENTLY_HIT_PROPS == 8)

    Array<RecentCrash, KI_MAX_RECENTLY_HIT_CARS> maRecentCrashes; // +0x7C  h:234 the live recent-hit-cars set
                                                                  //        (count word @+0x27C; ends @+0x280)

    // +0x280 h:237 -- the recent-stunt de-dupe window. UN-OPAQUED 2026-08-29 (showtime-score
    // wave) from `u8 maRecentStuntSet[0x58]`, because DealWithShowtimeStunt is its only user
    // and cannot be written against a byte blob. The guest header/stride is proven twice over:
    // ScoringSystem::Construct's inlined attach (@0x8233807C, `addi r7, r10, 0x18` -- the CgsID
    // element array is 8-aligned so it starts 0x18 in, not 0x14), and DealWithShowtimeStunt's
    // own walk (@0x82320F70: mpData +0x00, miMaxLength +0x04, miReadPos +0x08, miLength +0x10,
    // elements `slwi 3` == 8 bytes). Host sizeof is 0x58 as well, though nothing depends on it.
    CgsContainers::FixedRingBuffer<CgsID, 8> mRecentStuntSet;   // +0x280

    s32 miNumWheelsLastFrame;                 // +0x2D8 h:239
    s32 miBaseScore;                          // +0x2DC h:242  (raw crash score accumulator)
    s32 miCurrentComboCount;                  // +0x2E0 h:243  (cars in the current chain)
    s32 miScoreMultiplier;                    // +0x2E4 h:244  (accumulated multiplier; starts at 1)
    s32 maiNumCarsCrashed[4];                 // +0x2E8 h:245  (per VehicleClass 0..3)
    s32 miNumCarsLeaped;                      // +0x2F8 h:246
    s32 miNumPropsDestroyed;                  // +0x2FC h:247
    bool mbAboutToResetCombo;                 // +0x300 h:248
    u8  maPad0x301[3];                        //         alignment pad
    f32 mfResetComboGracePeriod;              // +0x304 h:249
    f32 mfDistanceTravelled;                  // +0x308 h:250
    f32 mfTimeSinceLastHitOverheadSign;       // +0x30C h:251
    f32 mfTimeContactingWall;                 // +0x310 h:252
    f32 mfTotalAirTime;                       // +0x314 h:253
    f32 mfCurrentJumpAirTime;                 // +0x318 h:254
    f32 mfLongestJumpAirTime;                 // +0x31C h:255
    f32 mfHighestJump;                        // +0x320 h:256  (debug overlay reads this directly)
    s32 miStuntsPerformed;                    // +0x324 h:257
    s32 miCarDestructionBonus;                // +0x328 h:258

    friend class CrashScoreDebugComponent;
};
}
