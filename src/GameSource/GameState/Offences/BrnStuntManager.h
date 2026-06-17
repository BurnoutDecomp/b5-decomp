// b5-decomp/src/GameSource/GameState/Offences/BrnStuntManager.h  (NEW owning header)
//
// Owning header for BrnGameState::StuntManager - the stunt / offence (jumps,
// smashes, signature takedowns, billboard smashes) tracking manager that lives
// inside BrnGameState::GameStateModule (DWARF BrnGameStateModule.h:783:
// `StuntManager mStuntManager;`).
//
// SHAPE is authoritative from the X360 DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/Offences/BrnStuntManager.h):
// it is a plain `struct StuntManager` (no base), with the full member set and the
// public/private method set reproduced below as DECLARATION-ONLY entries.
//
// MINIMAL SLICE: this pass reconstructs only StuntManager::FindTriggersCounty
// (X360 0x8236B310), which touches exactly ONE data member -- mWorldMap2D. Per the
// per-TU minimal-slice rule we therefore declare every method declaration-only and
// model the data members as: an explicit padding blob covering all members that
// precede mWorldMap2D (so mWorldMap2D lands at its X360 offset 880 == 0x370, the
// `a1 + 880` the pseudocode samples), the live mWorldMap2D member, and the
// remaining members declaration-/comment-only. The preceding members' real types
// (StuntManagerDebugComponent, ResourceHandle, EventReceiverQueue<512,16>,
// int16 count arrays, several manager pointers, ...) are intentionally NOT
// imported here -- several of those element types are not yet committed and none
// are touched by this TU. INTEGRATOR: when the rest of the StuntManager passes
// land, replace mPad_PrecedingMembers[880] with the real ordered members from the
// DWARF (listed in the trailing comment block) so the layout is fully named.
//
// The DWARF nested enum DistrictMapLoadStage is reproduced (it is part of the
// type's public shape and cheap/safe), but meDistrictMapLoadStage itself sits in
// the padded region.

#ifndef BRN_STUNT_MANAGER_H
#define BRN_STUNT_MANAGER_H

#include "types.hpp"                                          // fixed-width ints
#include "BrnCommonTypes.h"                                   // Vector2 / Vector3
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"       // CgsWorld::WorldMap2D (committed home; the one live member)
#include "SharedClasses/World/BrnWorldRegion.h"               // BrnWorld::ECounty / EDistrict / WorldRegion (used in signatures)

namespace BrnProgression { struct ProgressionManager; }
namespace BrnTrigger     { struct GenericRegion; struct SignatureStunt; }

namespace BrnGameState
{
    // Forward decls for types named only in declaration-only method signatures /
    // padded members (not defined or dereferenced by this TU).
    struct StuntManagerDebugComponent;
    enum   StuntElementType;
    struct ModeManager;
    struct GameStateModule;
    struct TrainingManager;
    struct OnStuntElementCompleteAction;

    struct StuntManager
    {
    public:
        // DWARF BrnStuntManager.h:147 - district-map streaming state machine.
        enum DistrictMapLoadStage
        {
            E_DISTRICT_MAP_LOAD_REQUEST     = 0,
            E_DISTRICT_MAP_LOAD_RESPONSE    = 1,
            E_DISTRICT_MAP_ACQUIRE_REQUEST  = 2,
            E_DISTRICT_MAP_ACQUIRE_RESPONSE = 3,
            E_DISTRICT_MAP_DONE             = 4,
        };

        // ---- public methods (declaration-only; each owned by its own X360 pass) ----
        void    Construct(BrnProgression::ProgressionManager*, void* /*TriggerQueryManager*/,
                          ModeManager*, TrainingManager*, GameStateModule*);   // :80
        bool    Prepare(void* /*GameStateModuleIO::OutputBuffer*/);            // :84
        void    Destruct();                                                    // :88
        void    Update(void* /*InputBuffer::GameActionQueue*/, void* /*RCEntityActiveRaceCarOutputInterface*/,
                       f32, bool);                                             // :95
        void    OnCrash();                                                     // :98
        void    OnStuntElement(const BrnTrigger::GenericRegion*, StuntElementType); // :103
        void    OnPropHit(uint16_t, uint16_t, Vector3);                        // :109
        int32_t GetMaxJumpCount();                                             // :112
        int32_t GetMaxSmashCount();                                            // :115
        int32_t GetMaxStuntCount();                                            // :118
        int32_t GetMaxStuntElementCountByCounty(StuntElementType, BrnWorld::ECounty); // :123
        int32_t GetMaxSignatureTDCount();                                      // :126
        void    ClearActiveJump();                                             // :129
        void    CompleteAllJumps();                                            // :132
        void    CompleteAllStunts();                                           // :135
        void    CompleteAllSmashes();                                          // :138
        void    CompleteAllStuntType(StuntElementType, void* /*GameActionQueue*/); // :143

    private:
        // ---- private methods (declaration-only) ----
        void              Clear();                                             // :166
        void              ClearPropData();                                     // :169
        bool              LoadDistrictMap(void* /*GameStateModuleIO::OutputBuffer*/); // :173
        void              AddStunt(const BrnTrigger::SignatureStunt*, int32_t, f32);  // :179
        bool              StuntElementTriggered();                             // :182
        void              ProcessStuntElement(void* /*GameActionQueue*/, bool, bool); // :188
        void              UpdateJumps(void* /*RCEntityActiveRaceCarOutputInterface*/,
                                       void* /*GameActionQueue*/, f32, bool);  // :195
        void              CheckForTrophyUnlocks(OnStuntElementCompleteAction*); // :199

        // THIS TU (X360 0x8236B310): map a trigger's world position to its county.
        BrnWorld::ECounty FindTriggersCounty(const BrnTrigger::GenericRegion* lpRegion); // :204

        bool              IsStuntElement(const BrnTrigger::GenericRegion*, StuntElementType*); // :209

        // ---- data members ----
        // Explicit padding for every member preceding mWorldMap2D so it lands at
        // its X360 offset 880 (0x370). The real ordered members (DWARF) are, in
        // order: StuntManagerDebugComponent mStuntManagerDebugComponent (:161),
        // StuntElementType meDebugCompletedUnlockType (:162) -- these two occupy
        // [0, 880).  Replace this blob with the named members when those passes land.
        uint8_t              mPad_PrecedingMembers[880];

        // DWARF BrnStuntManager.h:212 - the 2D district map sampled by this TU.
        // X360 offset 880 (0x370); GetValue() is called via `a1 + 880`.
        CgsWorld::WorldMap2D mWorldMap2D;

        // Trailing members (DWARF :213..:247), declaration-/comment-only here
        // because this TU does not touch them:
        //   ResourceHandle                        mDistrictMapResourceHandle;        // :213
        //   DistrictMapLoadStage                  meDistrictMapLoadStage;            // :214
        //   EventReceiverQueue<512,16>            mReceiverQueue;                    // :215
        //   int16_t                               maiTotalStuntElementCounts[3];     // :217
        //   int16_t                               maaiTotalStuntElementCountsPerCounty[3][5]; // :218
        //   BrnProgression::ProgressionManager*   mpProgressionManager;              // :220
        //   TriggerQueryManager*                  mpTriggerQueryManager;             // :221
        //   BrnGameState::ModeManager*            mpModeManager;                     // :222
        //   BrnGameState::GameStateModule*        mpGameStateModule;                 // :223
        //   BrnGameState::TrainingManager*        mpTrainingManager;                 // :224
        //   const GenericRegion*                  mpLastStuntOrSmashElement;         // :226
        //   StuntElementType                      meLastStuntElementType;            // :227
        //   uint16_t muLastZoneId; uint16_t muLastPropId;                            // :228/:229
        //   const GenericRegion*                  mpLastJumpElement;                 // :231
        //   float32_t mfJumpLandingTime; int32_t miSignatureTakedownCount;          // :234/:237
        //   bool mbJumpActive .. mbAlwaysToJumpCameras;                             // :240..:247
        // (KF_MIN_JUMP_SPEED_MPH / KI_MAX_ELEMENTS_PER_STUNT=8 / KI_MAX_RECENT_PROPS=8
        //  are file-scope static consts in BrnStuntManager.cpp per DWARF :156-158.)
    };
}

#endif // BRN_STUNT_MANAGER_H
