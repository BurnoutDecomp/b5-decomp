#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h
// ============================================================================
// Canonical home for BrnGameState::AchievementManagerBase -- the platform-neutral
// achievement-tracking base (DISTINCT from the data-table-blocked PS3 variant
// BrnGameState::AchievementManagerPS3, which derives from this and supplies the
// concrete AchievementEarnt/IsAchievementEarnt + a BitArray<60> send-queue).
//
// SHAPE is DWARF-authoritative
// (references/DecFIGS/dwarfdump/.../AchievementManager/BrnGameStateAchievementManagerBase.h):
//   vptr, then four protected back-pointer members in declared order
//   (mpProgressionManager, mpStreetManager, mpScoringSystem, mpGameStateModule),
//   then two protected virtuals (AchievementEarnt / IsAchievementEarnt).
// The X360 attests the back-pointer offsets via Construct (0x8235A948): the four
// stores land at this+4 / this+8 / this+0xC / this+0x10 -- i.e. directly after the
// 4-byte vptr -- which matches the DWARF member order exactly.
//
// EAchievement: the project-owned achievement enum. The X360 build passes the
// achievement IDs to the virtuals as raw integers (e.g. OnFlatSpin -> 9), and those
// integer IDs DO NOT line up with the PS3 DWARF EAchievement enumerator NAMES
// (PS3 E_ACHIEVEMENT_FLATSPIN == 12, not 9). The X360 SKU therefore numbers its
// achievements differently from the PS3 SKU. We do NOT have an X360-specific
// EAchievement enum dump, so the enumerator names below are the PS3 DWARF names
// (recovered verbatim) but the *values* are NOT trustworthy for the X360 SKU.
// To keep every achievement id in this TU a NAMED constant (no bare integer
// literals in the bodies) while staying faithful to the X360-attested numbering,
// this TU's .cpp references its ids through the E_X360_ACHIEVEMENT_* constants
// defined alongside the bodies (their values ARE X360-asm-attested). The PS3-named
// EAchievement enum is retained here for the shared base/PS3 signatures.
//
// FLAG (best-effort / out of scope): KAE_LICENSE_ACHIEVEMENTS is a const data table
// (X360 @ 0x8202AF68) that OnLicenseUpgrade indexes by (rank-1). Its six entries are
// rodata we cannot recover from the code exports; it is declared extern here and the
// .cpp gives a best-effort definition (the six license-grade ids in rank order) that
// is FLAGGED, not asm-verified.
// ----------------------------------------------------------------------------

#include "types.hpp"                                       // u8, s32, f32, etc.
#include "BrnCommonTypes.h"                                 // CgsID (== u64)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateTypes.h"         // BrnGameState::StuntElementType
#include "GameSource/GameState/BrnGameStateSharedIO.h"      // GameStateModuleIO::EGameModeType
#include "SharedClasses/StreetData/BrnChallengeData.h"      // BrnStreetData::ScoreType

namespace BrnProgression { class ProgressionManager; }

namespace BrnGameState
{
    class ScoringSystem;
    class GameStateModule;

    namespace GameStateModuleIO
    {
        struct PreWorldInputBuffer;
        struct OutputBuffer;
    }
}

// StreetManager lives in the BrnGameState namespace in the DWARF; forward-declare it
// at that scope (its committed home is BrnGameStateStreetManager.h). Used by pointer.
namespace BrnGameState { struct StreetManager; }

// ---------------------------------------------------------------------------
// EAchievement (PS3 DWARF names; see header banner for the X360-numbering caveat).
// Recovered verbatim from BrnGameStateAchievementManagerPS3.h:24.
// ---------------------------------------------------------------------------
enum EAchievement
{
    E_ACHIEVEMENT_INVALID                              = -1,
    E_ACHIEVEMENT_START                                = 0,
    E_ACHIEVEMENT_REPAIR_FIRST_WRECKED_CAR             = 0,
    E_ACHIEVEMENT_SET_ROAD_RULE_TIME_ON_WATT_ST        = 1,
    E_ACHIEVEMENT_SET_ROAD_RULE_CRASH_ON_EAST_CRAWFORD_DRIVE = 2,
    E_ACHIEVEMENT_WIN_FIRST_RACE                       = 3,
    E_ACHIEVEMENT_COLLECT_5_STUNTS                     = 4,
    E_ACHIEVEMENT_COLLECT_25_SMASHES                   = 5,
    E_ACHIEVEMENT_WIN_RACE_WITHOUT_CRASHING            = 6,
    E_ACHIEVEMENT_LICENSE_GRADE_D                      = 7,
    E_ACHIEVEMENT_SHUTDOWN_SI_7                        = 8,
    E_ACHIEVEMENT_5X_MULTIPLIER_IN_SHOWTIME            = 9,
    E_ACHIEVEMENT_ROADRAGE_PERFECT_RAGE                = 10,
    E_ACHIEVEMENT_REPAIR_CRITICAL_DAMAGE_IN_ROADRAGE   = 11,
    E_ACHIEVEMENT_FLATSPIN                             = 12,
    E_ACHIEVEMENT_TAKEDOWN_FRENZY                      = 13,
    E_ACHIEVEMENT_LICENSE_GRADE_C                      = 14,
    E_ACHIEVEMENT_COLLECT_5_JUMPS                      = 15,
    E_ACHIEVEMENT_2X_BOOST_CHAIN                       = 16,
    E_ACHIEVEMENT_SHUTDOWN_ROSSOLINI_LM                = 17,
    E_ACHIEVEMENT_WIN_MARKED_MAN_WITHOUT_TAKENDOWN     = 18,
    E_ACHIEVEMENT_WIN_7_UNIQUE_STUNT_RUNS              = 19,
    E_ACHIEVEMENT_POWER_PARK_80                        = 20,
    E_ACHIEVEMENT_10X_MULTIPLIER_IN_SHOW_TIME          = 21,
    E_ACHIEVEMENT_COLLECT_200_SMASHES                  = 22,
    E_ACHIEVEMENT_30_SHOWTIME_RULES                    = 23,
    E_ACHIEVEMENT_30_ROAD_RULES                        = 24,
    E_ACHIEVEMENT_WIN_10_XS_CARS                       = 25,
    E_ACHIEVEMENT_TAKEDOWN_RAMPAGE                     = 26,
    E_ACHIEVEMENT_1_MILE_ONCOMING                      = 27,
    E_ACHIEVEMENT_LICENSE_GRADE_B                      = 28,
    E_ACHIEVEMENT_SHUTDOWN_HUNTER_MANHATTAN            = 29,
    E_ACHIEVEMENT_10X_BOOST_CHAIN                      = 30,
    E_ACHIEVEMENT_POWER_PARK                           = 31,
    E_ACHIEVEMENT_SHUTDOWN_INFERNO_VAN                 = 32,
    E_ACHIEVEMENT_MILLIONAIRES_CLUB                    = 33,
    E_ACHIEVEMENT_COLLECT_25_JUMPS                     = 34,
    E_ACHIEVEMENT_COLLECT_60_STUNTS                    = 35,
    E_ACHIEVEMENT_3_BARREL_ROLL_JUMP                   = 36,
    E_ACHIEVEMENT_20X_BOOST_CHAIN                      = 37,
    E_ACHIEVEMENT_LICENSE_GRADE_A                      = 38,
    E_ACHIEVEMENT_SHUTDOWN_MONTGOMERY_GT2400           = 39,
    E_ACHIEVEMENT_40X_MULTIPLIER_IN_STUNT_RUN          = 40,
    E_ACHIEVEMENT_WIN_25_XS_CARS                       = 41,
    E_ACHIEVEMENT_SHUTDOWN_JANSEN_X12                  = 42,
    E_ACHIEVEMENT_GET_500_TAKEDOWNS                    = 43,
    E_ACHIEVEMENT_SHUTDOWN_HUNTER_4X4                  = 44,
    E_ACHIEVEMENT_LICENSE_GRADE_BURNOUT                = 45,
    E_ACHIEVEMENT_WIN_RACE_IN_WATSON_25_V16            = 46,
    E_ACHIEVEMENT_WIN_ALL_CAR_CHALLENGES               = 47,
    E_ACHIEVEMENT_FIND_ALL_EVENTS                      = 48,
    E_ACHIEVEMENT_SECRET_FIND_ALL_CARPARKS             = 49,
    E_ACHIEVEMENT_DRIVE_LONG_HAUL_MILES                = 50,
    E_ACHIEVEMENT_COLLECT_ALL_STUNTS                   = 51,
    E_ACHIEVEMENT_COLLECT_ALL_SMASHES                  = 52,
    E_ACHIEVEMENT_WIN_MARKED_MAN_IN_KRIEGER_WTR_07     = 53,
    E_ACHIEVEMENT_COLLECT_ALL_JUMPS                    = 54,
    E_ACHIEVEMENT_SET_ROAD_RULE_TIME_ON_ALL            = 55,
    E_ACHIEVEMENT_SET_ROAD_RULE_CRASH_ON_ALL           = 56,
    E_ACHIEVEMENT_FIND_ALL_DRIVE_THRUS                 = 57,
    E_ACHIEVEMENT_LICENSE_GRADE_ELITE                  = 58,
    E_ACHIEVEMENT_COMPLETE_OFFLINE                     = 59,
    E_ACHIEVEMENT_COUNT                                = 60,
};

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// AchievementManagerBase
//
// Watches gameplay events and, for each, fires AchievementEarnt(id) (vtable slot 0)
// the first time IsAchievementEarnt(id) (vtable slot 1) reports the id is NOT yet
// earnt and the event's threshold is met. The X360 dispatches every id through these
// two virtuals: `(*(*this + 4))(this, id)` is IsAchievementEarnt (the second slot,
// at vtable+4) and `(**this)(this, id)` is AchievementEarnt (the first slot).
// ---------------------------------------------------------------------------
class AchievementManagerBase
{
public:
    // X360 0x8235A948. Stores the four manager back-pointers (each CGS_ASSERT-guarded
    // non-null) into mpProgressionManager / mpStreetManager / mpScoringSystem /
    // mpGameStateModule. Returns `this` (sret-style; the X360 hands result back in r3).
    void Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                   StreetManager* lpStreetManager,
                   ScoringSystem* lpScoringSystem,
                   GameStateModule* lpGameStateModule);

    bool Prepare();                                                             // DWARF :60 (other TU)
    bool Release();                                                             // DWARF :64 (other TU)
    void Destruct();                                                            // DWARF :68 (other TU)
    void Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                GameStateModuleIO::OutputBuffer* lpOutput);                     // DWARF :74 (other TU)

    // ===== gameplay event hooks owned by THIS TU (X360-attested bodies) =====
    void OnBodyShop(GameStateModuleIO::EGameModeType leGameMode);               // X360 0x8235AA18
    void OnSetRoadRule(BrnStreetData::ScoreType leScoreType, CgsID lRoadId);    // X360 0x8235AC08
    void OnSetAllRoadRules(BrnStreetData::ScoreType leScoreType);               // X360 0x8235ACF8
    void OnTakedown();                                                          // X360 0x8235AAE0
    void OnEventWin(GameStateModuleIO::EGameModeType leGameMode);               // X360 0x82372978
    void OnLicenseUpgrade(u8 luNewLicenseRank);                                 // X360 0x8235ADC8
    void OnCollectStunt(StuntElementType leStuntType);                          // X360 0x82366EB8
    void OnFindAllDriveThrus();                                                 // X360 0x8235AE78
    void OnFindAllCarParks();                                                   // X360 0x8235AED8
    void OnCollectAllStunts(StuntElementType leStuntType);                      // X360 0x8235AF38
    void OnBarrelRoll(s32 liNumRolls);                                          // X360 0x8235B040
    void OnFlatSpin(f32 lfDegrees);                                             // X360 0x8235B0B8
    void OnPowerParking(s32 liNumCars);                                         // X360 0x8235B138
    void OnGameCompletion();                                                    // X360 0x8235B1B0

    // ===== remaining base hooks declared in the DWARF (bodies in other TUs) =====
    void OnShutdown(CgsID lCarId);                                              // DWARF :98
    void OnDrivenDistance();                                                    // DWARF :117
    void OnFindAllEvents();                                                     // DWARF :128
    void OnStuntRunMultiplier(s32 liMultiplier);                               // DWARF :148
    void OnShowTimeMultiplier(s32 liMultiplier);                              // DWARF :153
    void OnBoostChain(s32 liChainLength);                                       // DWARF :163
    void OnTakedownChain(s32 liChainLength);                                    // DWARF :168

    // ADDITIVE GROW (FLAG): X360 BurnoutSkillzManager::UpdateBurnoutSkillzTotals
    // (0x82322C30) calls this through the achievement-manager back-pointer when a
    // local player's free-burn skillz total changes (it forwards the network player
    // count + the new total). The PS3-derived AchievementManagerPS3 supplies the body;
    // declared-only here (no body in this base / in the SkillzManager TU). The DWARF
    // member list for this base did not name it; added additively (existing members and
    // method order left untouched). If the leak later attests a different signature,
    // reconcile here.
    void OnFreeburnSkillzTotalChange(s32 liNumberOfNetworkPlayers, f32 lfNewSkillzTotal);

    // ===== ADDITIVE GROW: the seven remaining gameplay-event hooks bodied in the
    // PS3 unit BrnGameStateAchievementManagerPS3.cpp (X360-attested). Each fires its
    // achievement id(s) through the two virtuals once the event threshold is met.
    // Signatures pinned from the X360 call-site arg-register setup (see that .cpp's
    // per-func X360 address comment). Appended additively; existing members untouched.
    void OnCaughtFever();                                                       // X360 0x8235B590
    void OnFreeburnChallengeBlockComplete();                                    // X360 0x8235B370
    void OnFreeburnChallengeComplete(u32 luCompletedChallengeCount);            // X360 0x8235B2D8
    void OnMugshotAdded(u32 luMugshotCount);                                    // X360 0x8235B3D0
    void OnMugshotSent(s32 liMugshotCount);                                     // X360 0x8235B5F0
    void OnOnlineRaceComplete(s32 liGameModeType, bool lbFlag,
                              s32 liChainA, s32 liChainB);                      // X360 0x8235B6A8
    void OnRivalAdded(s32 liRivalCount);                                        // X360 0x8235B448

protected:
    // Vtable slot 0 / slot 1 respectively (the X360 calls slot 0 at vtable+0 and
    // slot 1 at vtable+4). Pure in this base in spirit; the PS3 variant supplies
    // the concrete bodies. Declared pure so the abstract base never needs a body
    // (this TU's gate is compile-only and never instantiates the base).
    virtual void AchievementEarnt(EAchievement leAchievement)        = 0;       // DWARF :219 (vtable slot 0)
    virtual bool IsAchievementEarnt(EAchievement leAchievement)      = 0;       // DWARF :224 (vtable slot 1)

protected:
    // ⚠️ [FLAG PC bring-up] the four back-pointers carry `= 0` in-class initialisers. GameStateModule
    // now embeds this manager BY VALUE (X360 this+181680, DWARF BrnGameStateModule.h:226) but does
    // NOT yet call Construct: mounting this TU costs eight unresolved externals that have no
    // definition anywhere in the tree (five ScoringSystem + three ProgressionManager accessors the
    // gameplay-event hooks below call), and /OPT:REF does not excuse an unresolved external from an
    // unreferenced COMDAT -- verified with a minimal LNK2019 repro. Without the initialisers the
    // embedded subobject's back-pointers would be INDETERMINATE rather than merely unset; `0` is
    // also what every consumer's CGS_ASSERT is written to catch. They do not change the layout.
    // DELETE-WHEN GameStateModule::Construct calls Construct for real (see its DELETE-WHEN block).
    BrnProgression::ProgressionManager* mpProgressionManager = 0;               // this+0x04 (DWARF :228)
    StreetManager*                      mpStreetManager      = 0;               // this+0x08 (DWARF :229)
    ScoringSystem*                      mpScoringSystem      = 0;                // this+0x0C (DWARF :230)
    GameStateModule*                    mpGameStateModule    = 0;                // this+0x10 (DWARF :231)
};

// ---------------------------------------------------------------------------
// BrnGameState::AchievementManagerPS3 -- the concrete platform achievement manager
// (DWARF: derives from AchievementManagerBase, supplies the two pure virtuals + a
// BitArray<60> send-queue). Its full data-table-gated layout is BLOCKED (the X360
// achievement-id tables are not in the exports), so this is a MINIMAL complete slice:
// it derives from the base and stubs the two pure virtuals so callers that hold a
// StuntModeScoring::AchievementManager* (== this) can resolve the inherited base
// methods (e.g. SkillzManager's OnFreeburnSkillzTotalChange forward) under cl /c.
// Grow this in place when the PS3 manager's own TU is reconstructed; do NOT fork.
// FLAG: the override bodies + the send-queue member are placeholders, not the real PS3
// behaviour (compile-only completeness for pointer-method dispatch).
class AchievementManagerPS3 : public AchievementManagerBase
{
protected:
    void AchievementEarnt(EAchievement leAchievement) override;
    bool IsAchievementEarnt(EAchievement leAchievement) override;
};

}

// ---------------------------------------------------------------------------
// KAE_LICENSE_ACHIEVEMENTS (X360 @ 0x8202AF68) -- const rodata indexed by
// (liNewLicenseRank - 1) in OnLicenseUpgrade. Six entries (rank 1..6). The bytes
// are NOT in the code exports; the definition in the .cpp is FLAGGED best-effort.
// ---------------------------------------------------------------------------
extern const EAchievement KAE_LICENSE_ACHIEVEMENTS[6];
