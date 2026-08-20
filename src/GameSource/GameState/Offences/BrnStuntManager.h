// b5-decomp/src/GameSource/GameState/Offences/BrnStuntManager.h  (owning header)
//
// Owning header for BrnGameState::StuntManager - the COLLECTIBLE bookkeeper that
// tracks the player's progress through the open-world stunt-element collectibles:
// Super Jumps, Super Smashes and Billboard smashes. It lives inside
// BrnGameState::GameStateModule (DWARF BrnGameStateModule.h:783:
// `StuntManager mStuntManager;`).
//
// SHAPE is authoritative from the X360 DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/Offences/BrnStuntManager.h):
// a plain `struct StuntManager` (no base) with the full member set + method set
// reproduced below. This pass GROWS the former minimal slice (which modelled the
// leading members as a single mPad_PrecedingMembers[880] blob) into the real
// DWARF-ordered members, and bodies the manager spine: Construct / Prepare /
// Update / OnPropHit / UpdateJumps / StuntElementTriggered / CheckForTrophyUnlocks
// / LoadDistrictMap / LatchJumpElement (+ keeps the previously bodied
// FindTriggersCounty). The remaining methods (OnStuntElement, Clear, ClearPropData,
// AddStunt, ProcessStuntElement, IsStuntElement, CompleteAll*, Destruct, OnCrash)
// are declaration-only -- each owned by its own X360 TU.
//
// X360 OFFSET MAP (verified vs the pseudocode/asm; `this + N` byte offsets):
//   0x000 mStuntManagerDebugComponent  (StuntManagerDebugComponent, 864B incl. base)
//   0x360 meDebugCompletedUnlockType   (== this+864; Construct stores 3 == _COUNT)
//   0x370 mWorldMap2D                  (== this+880; CgsWorld::WorldMap2D, 40B)
//   0x3A0 mDistrictMapResourceHandle   (== this+928; CgsResource::ResourceHandle, 8B)
//   0x3A8 meDistrictMapLoadStage       (== this+936; DistrictMapLoadStage)
//   0x3AC mReceiverQueue               (== this+940; EventReceiverQueue<512,16>)
//   0x5C4 maiTotalStuntElementCounts[3]            (== this+1476; s16[3])
//   0x5CA maaiTotalStuntElementCountsPerCounty[3][5] (== this+1482; s16[3][5])
//   0x5E8 mpProgressionManager .. 0x5F8 mpTrainingManager (5 manager ptrs)
//   0x5FC mpLastStuntOrSmashElement  0x600 meLastStuntElementType
//   0x604 muLastZoneId  0x606 muLastPropId  0x608 mpLastJumpElement
//   0x60C mfJumpLandingTime  0x610 miSignatureTakedownCount
//   0x614..0x619 the six jump-state bools

#ifndef BRN_STUNT_MANAGER_H
#define BRN_STUNT_MANAGER_H

#include "types.hpp"                                          // fixed-width ints, f32
#include "BrnCommonTypes.h"                                   // Vector2 / Vector3
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"       // CgsWorld::WorldMap2D (the +880 live member)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle (mDistrictMapResourceHandle)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // CgsModule::EventReceiverQueue<N,16> (mReceiverQueue)
#include "GameSource/GameState/BrnGameStateTypes.h"           // BrnGameState::StuntElementType (enum values)
#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h" // StuntManagerDebugComponent (mStuntManagerDebugComponent, complete-by-value)
#include "SharedClasses/World/BrnWorldRegion.h"               // BrnWorld::ECounty / EDistrict / WorldRegion (signatures + Prepare)
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // GameStateModuleIO::GameActionQueue (real typedef)

namespace BrnProgression { class ProgressionManager; }   // committed tag: class (BrnProgressionManager.h)
namespace BrnTrigger     { struct GenericRegion; struct SignatureStunt; }
// RCEntityActiveRaceCarOutputInterface is taken by pointer in Update/UpdateJumps, so a forward decl
// suffices here; the .cpp pulls the full BrnRaceCarEntityModuleOutputInterface.h for the accessors.
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGameState
{
    // Forward decls for the four sibling managers held only as pointers (each its own TU).
    // Tags match the committed homes (all `class`) to avoid a struct/class mismatch (C4099).
    class  ModeManager;
    class  GameStateModule;
    class  TrainingManager;
    class  TriggerQueryManager;

    // GameActionQueue is a real typedef (BrnGameStateSharedIO.h, included above).
    namespace GameStateModuleIO { struct OutputBuffer; struct OnStuntElementCompleteAction; }

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

        // DWARF BrnStuntManager.h:156-158 - file-scope tuning constants (defined in the .cpp).
        static const f32     KF_MIN_JUMP_SPEED_MPH;       // :156
        static const int32_t KI_MAX_ELEMENTS_PER_STUNT = 8;  // :157
        static const int32_t KI_MAX_RECENT_PROPS       = 8;  // :158

        // ---- public methods ----
        // THIS TU (X360 0x82365990): wire in the five sibling managers + init the queue/debug component.
        void Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                       TriggerQueryManager*                lpTriggerQueryManager,
                       ModeManager*                        lpModeManager,
                       TrainingManager*                    lpTrainingManager,
                       GameStateModule*                    lpGameStateModule);   // :80
        // THIS TU (X360 0x8239C9C0): stream the district map, then tally every stunt element on the track.
        bool Prepare(GameStateModuleIO::OutputBuffer* lpOutput);                 // :84
        void Destruct();                                                        // :88 (own TU)
        // THIS TU (X360 0x8239F8D0): per-frame spine - drive the jump state machine + dispatch completed elements.
        void Update(GameStateModuleIO::GameActionQueue*                                       lpActionQueue,
                    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                    f32 lfTimeStep, bool lbIsAGameModeActive);                   // :95
        void OnCrash();                                                          // :98 (own TU)
        void OnStuntElement(const BrnTrigger::GenericRegion*, StuntElementType); // :103 (own TU)
        // THIS TU (X360 0x8236EE18): a prop was destroyed - latch it if it sits inside a smash/billboard region.
        void OnPropHit(uint16_t luZoneId, uint16_t luPropId, Vector3 lPosition); // :109
        int32_t GetMaxJumpCount();                                              // :112 (own TU)
        int32_t GetMaxSmashCount();                                             // :115 (own TU)
        int32_t GetMaxStuntCount();                                             // :118 (own TU)
        int32_t GetMaxStuntElementCountByCounty(StuntElementType, BrnWorld::ECounty); // :123 (own TU)
        int32_t GetMaxSignatureTDCount();                                       // :126 (own TU)
        void    ClearActiveJump();                                              // :129 (own TU)
        void    CompleteAllJumps();                                             // :132 (own TU)
        void    CompleteAllStunts();                                            // :135 (own TU)
        void    CompleteAllSmashes();                                           // :138 (own TU)
        void    CompleteAllStuntType(StuntElementType, GameStateModuleIO::GameActionQueue*); // :143 (own TU)

        // De-inlined ProcessPlayerTriggers case 7 (added by the TriggerQueryManager TU): latch this
        // region as the pending jump element when no jump is already active. THIS TU bodies it.
        // PUBLIC: called cross-class by TriggerQueryManager::ProcessPlayerTriggers.
        void    LatchJumpElement(const BrnTrigger::GenericRegion* lpRegion);

        // Additive accessor (FLAG: not its own X360 function -- inlined at the call site).
        // StuntManagerDebugComponent::GetTriggerWorldRegion @ 0x8236F070 samples this manager's
        // district map directly via `mpStuntManager + 880` (== &mWorldMap2D). Exposes that one
        // member BY NAME so the debug component does not reach into the private layout.
        const CgsWorld::WorldMap2D& GetDistrictMap() const { return mWorldMap2D; }

    private:
        // ---- private methods (declaration-only; each owned by its own X360 TU) ----
        void              Clear();                                              // :166 (own TU)
        void              ClearPropData();                                      // :169 (own TU)
        // THIS TU (X360 0x82399458): district-map streaming state machine (load -> acquire -> bind).
        bool              LoadDistrictMap(GameStateModuleIO::OutputBuffer* lpOutput); // :173
        void              AddStunt(const BrnTrigger::SignatureStunt*, int32_t, f32); // :179 (own TU)
        // THIS TU (X360 0x82358BE0): predicate - has a stunt/smash element latched this frame?
        bool              StuntElementTriggered();                              // :182
        // BODIED this wave (X360 0x8239CDB0, partfile StuntManager_gUI_00.cpp). FOUR parameters,
        // as the committed declaration always had and as DWARF spells it
        // (`void ProcessStuntElement(OutputBuffer::GameActionQueue*, bool, bool)`,
        // dwarfdump GameSource/GameState/Offences/BrnStuntManager.h:355).
        // ⚠️ A round-1 banner here claimed "THREE parameters, not four" off the CALLEE's prologue
        // (which reads only r3/r4/r5). That conclusion was WRONG: the prologue tells you what the
        // callee USES, not what the signature is. BOTH console call sites materialise r6 in the
        // argument block immediately before the branch --
        //     0x8239D7C0  mr r6, r24   (UpdateJumps;  r5 = 1, r24 = its own lbIsAGameModeActive,
        //                               `mr r24, r7` @0x8239D48C -- r7 because the f32 timestep
        //                               rides f1 and SKIPS its GPR slot)
        //     0x8239FA3C  mr r6, r27   (Update;        r5 = 0)
        // -- so the source signature carries the trailing bool and the callee simply optimised its
        // only use away. Kept 4-arg; the body ignores lbIsAGameModeActive, exactly as the console's
        // does.
        void              ProcessStuntElement(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                              bool lbIsJump,
                                              bool lbIsAGameModeActive);        // :188 (X360 0x8239CDB0)
        // THIS TU (X360 0x8239D460): advance the active-jump state machine (takeoff/land/abandon).
        void              UpdateJumps(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                                      GameStateModuleIO::GameActionQueue* lpActionQueue,
                                      f32 lfTimeStep, bool lbIsAGameModeActive); // :195
        // THIS TU (X360 0x82399390): on element completion, fire the trophy + special-car unlock checks.
        void              CheckForTrophyUnlocks(GameStateModuleIO::OnStuntElementCompleteAction* lpOnStuntElementAction); // :199

        // PREVIOUSLY BODIED (kept) -- X360 0x8236B310: map a trigger box to its county.
        BrnWorld::ECounty FindTriggersCounty(const BrnTrigger::GenericRegion* lpRegion); // :204

        bool              IsStuntElement(const BrnTrigger::GenericRegion*, StuntElementType*); // :209 (own TU)

        // ---- data members (DWARF order; X360 byte offsets noted in comments, parity-by-name) ----
        // DWARF :161 - the in-game stunt/offence debug menu (complete by value; 864B incl. base).
        StuntManagerDebugComponent mStuntManagerDebugComponent;     // +0x000
        // DWARF :162 - which element type the debug menu last force-completed. Construct stores
        // E_STUNT_ELEMENT_TYPE_COUNT (3) == "nothing pending"; Update fires CompleteAllStuntType
        // when this is != COUNT then resets it to COUNT.
        StuntElementType           meDebugCompletedUnlockType;      // +0x360 (== this+864)

        // DWARF :212 - the 2D district map (district-id grid) sampled to county-classify triggers.
        CgsWorld::WorldMap2D       mWorldMap2D;                      // +0x370 (== this+880)
        // DWARF :213 - handle onto the streamed "Districts.dat" resource backing mWorldMap2D.
        CgsResource::ResourceHandle mDistrictMapResourceHandle;     // +0x3A0 (== this+928)
        // DWARF :214 - district-map streaming state machine cursor.
        DistrictMapLoadStage       meDistrictMapLoadStage;          // +0x3A8 (== this+936)
        // DWARF :215 - receiver queue the resource-load responses arrive on.
        CgsModule::EventReceiverQueue<512, 16> mReceiverQueue;      // +0x3AC (== this+940)

        // DWARF :217 - total stunt elements on the track, per StuntElementType (JUMP/SMASH/BILLBOARD).
        int16_t                    maiTotalStuntElementCounts[3];   // +0x5C4 (== this+1476)
        // DWARF :218 - the same totals broken down per county (E_COUNTY_VALID_COUNT == 5 counties).
        int16_t                    maaiTotalStuntElementCountsPerCounty[3][5]; // +0x5CA (== this+1482)

        // DWARF :220-224 - the five sibling managers (pointers; each its own TU).
        BrnProgression::ProgressionManager* mpProgressionManager;   // +0x5E8 (== this+1512)
        TriggerQueryManager*                mpTriggerQueryManager;  // +0x5EC (== this+1516)
        ModeManager*                        mpModeManager;          // +0x5F0 (== this+1520)
        GameStateModule*                    mpGameStateModule;      // +0x5F4 (== this+1524)
        TrainingManager*                    mpTrainingManager;      // +0x5F8 (== this+1528)

        // DWARF :226-231 - the "last latched element" working set (the active jump / stunt / prop).
        const BrnTrigger::GenericRegion*    mpLastStuntOrSmashElement; // +0x5FC (== this+1532)
        StuntElementType                    meLastStuntElementType;    // +0x600 (== this+1536)
        uint16_t                            muLastZoneId;              // +0x604 (== this+1540)
        uint16_t                            muLastPropId;              // +0x606 (== this+1542)
        const BrnTrigger::GenericRegion*    mpLastJumpElement;         // +0x608 (== this+1544)

        // DWARF :234/237 - jump landing timer + the signature-takedown total.
        f32                                 mfJumpLandingTime;         // +0x60C (== this+1548)
        int32_t                             miSignatureTakedownCount;  // +0x610 (== this+1552)

        // DWARF :240-247 - the active-jump state machine bools.
        bool                                mbJumpActive;              // +0x614 (== this+1556)
        bool                                mbHasPlayerLeftGround;     // +0x615 (== this+1557)
        bool                                mbShowJumpNameNextFrame;   // +0x616 (== this+1558)
        bool                                mbIsAttemptingJumpForFirstTime; // +0x617 (== this+1559)
        bool                                mbNeedToSendJumpFailureMessage; // +0x618 (== this+1560)
        bool                                mbAlwaysToJumpCameras;     // +0x619 (== this+1561)

        // PARITY-BY-NAMED-MEMBER (not X360-faithful byte offsets on the x64 gate): the X360 offsets in
        // the header banner above are exact for the console, but mStuntManagerDebugComponent (contains
        // pointers), mReceiverQueue (8-byte buffer pointer on x64) and the five manager pointers are all
        // wider on the x64 gate, so an absolute `offsetof(...) == 880` static_assert would FAIL the gate.
        // We therefore reproduce the DWARF member ORDER and TYPES (semantic parity) and do NOT bake the
        // absolute byte offsets -- mirroring the BrnGameActions PrepareForModeAction precedent. The
        // reconstructed bodies access these members BY NAME, so the on-x64 layout is irrelevant.
    };
}

#endif // BRN_STUNT_MANAGER_H
