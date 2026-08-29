#ifndef BRN_CRASH_PLAY_DEBUG_COMPONENT_H
#define BRN_CRASH_PLAY_DEBUG_COMPONENT_H

#include "BrnCommonTypes.h"
#include "types.hpp"
#include <cstddef>   // offsetof (CrashPlayManager::_AssertLayout)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"          // FixedRingBuffer<EntityId,N>
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"          // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // CgsSceneManager::VolumeInstanceId

namespace BrnGameState { namespace GameStateModuleIO {
    struct JustBouncedAction; struct RoadRulesEnterRoadAction; struct SendJunctionPlayerIsAtAction;
} }
namespace BrnPhysics { namespace ContactSpy { struct RaceCarContact; } }

namespace BrnWorld
{
    struct CrashPlayManager;
    class  ActiveRaceCar;                 // BrnActiveRaceCar.h declares it `class`
    struct PlayerVehicleControls;


    namespace RaceCarEntityModuleIO { struct OutputBuffer_PrePhysics; }

    static const s32 KI_NUM_FUEL_TRAIL_NODES = 32;

    // BrnCrashPlayManager.h :34 / :36 -- the DecFIGS dump prints both initialisers, and both are
    // the extents of the two FixedRingBuffers below.
    static const s32 KI_MAX_RECENTLY_HIT_CARS   = 32;
    static const s32 KI_MAX_RECENTLY_LEAPT_CARS = 8;

    // BrnCrashPlayManager.h:32. The upper clamp of the showtime boost meter -- the 100.0f
    // (flt_82014808) that every ClampBoostLevel site in BrnCrashPlayManager.cpp loads.
    static const f32 KF_MAX_BOOST = 100.0f;

    // BrnCrashPlayManager.h:34 / :36 (DecFIGS DWARF, values printed by the dump).

    // BrnCrashPlayManager.h:32. The upper clamp of the showtime boost meter -- the 100.0f
    // (flt_82014808) every ClampBoostLevel site loads.

    struct CrashCombo
    {
        f32 mafSpins[3];
    };

    struct CrashBreakerParams
    {
        bool mbIsActive;
        u8 maPad01[15];
        Vector3 mEpiCentre;
        f32 mfTotalRadius;
        s32 miNumFuelTrailNodes;
        s32 miFirstFuelTrailNodeIndex;
        Vector3 maFuelTrail[KI_NUM_FUEL_TRAIL_NODES];

        f32 GetTotalRadius() const { return mfTotalRadius; }
    };

    // ⭐ MOVED ABOVE CrashPlayManager 2026-08-29 (BrnCrashPlayManager.cpp landing). It is the
    // class's FIRST member BY VALUE (console +0x000), so its complete definition has to precede
    // it -- which is the arrangement the original had too (BrnCrashPlayManager.h includes
    // BrnCrashPlayDebugComponent.h; the component only ever holds a CrashPlayManager POINTER,
    // so the forward declaration above breaks the cycle exactly as the original does).
    class CrashPlayDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(CrashPlayManager* lpCrashPlayManager);
        void Destruct();
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay) override;
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay) override;
        void Update() override;

    protected:
        const char* GetName() const override;
        const char* GetPath() const override;
        void OnActivate() override;

    private:
        CrashPlayManager* mpCrashPlayManager;
        bool mbDisplayCrashBreaker;
    };

    // ============================================================================
    // BrnWorld::CrashPlayManager -- the Showtime / crash-play state machine, embedded BY VALUE
    // in RaceCarEntityModule (console +98544; DWARF BrnRaceCarEntityModule.h:355).
    //
    // ⭐⭐ TAIL RE-SEATED 2026-08-11 (player-input wave). The committed model anchored ONE member
    // (mfAftertouchPower @+0x138) and then guessed the rest, which put mbIsCrashPlayActive ~0x240
    // bytes too late. The whole 0x134..0x153 run is laid out from the DecFIGS DWARF member
    // ORDER (references/DecFIGS/.../CrashPlay/BrnCrashPlayManager.h, the fourteen members from
    // mfBoostPercentage :268 to mbBoostChargePending :283) with FIVE independent X360 anchors.
    // The base is asm-literal: RaceCarEntityModule::HandlePrepareForModeAction calls
    // `CrashPlayManager::Activate(module + 98544, ...)`, so module+98544 == this+0.
    //
    // ⭐⭐⭐ HEAD CARVED 2026-08-29 (this wave). The 308-byte `maPad00` that used to stand in for
    // +0x000..+0x134 is GONE: every member in it is now named, from the same DWARF order
    // (:229..:265) plus THIRTEEN independent X360 anchors, and every one lands exactly with no
    // fudge. Nine of them are the stores CrashPlayManager::Activate @0x822C31A8 makes, in
    // declaration order:
    //   [V] +0x018  mPlayerCarVolumeInstanceID  Update @0x82306648 `ld r11,0x18(r30)` +
    //                                           `cmpdi r11,-1`  == VolumeInstanceId::IsValid()
    //   [V] +0x020  mLastPlayerPos              Activate `stvx128 v0(zero), r3, 32`; UpdateMomentum
    //                                           @0x82302110 `addi r30, r31, 0x20` is the lvx/stvx base
    //   [V] +0x030  mRecentCrashSet             Activate `stw 0(0x38/0x3C/0x40)` == RingBuffer::Clear
    //                                           (miReadPos/miWritePos/miLength at hdr +8/+C/+10)
    //   [V] +0x0C4  mRecentLeaptSet             Activate `stw 0(0xCC/0xD0/0xD4)` -- the SAME three
    //                                           Clear slots exactly 0x94 later, which is
    //                                           sizeof(FixedRingBuffer<EntityId,32>) == 20 + 32*4
    //   [V] +0x0F8  miCarsLeaptThisFrame        UpdateCarLeaping @0x822F91E4 `lwz r11, 0xF8(r31)`
    //   [V] +0x100  miLastStreetEntered         OnEnterRoad @0x822A7DC0 `ld r10, 0x100(r31)` (CgsID
    //                                           is u64 -- an 8-byte load, and Activate's `std`)
    //   [V] +0x108  mbSendNewRoadMessage        UpdateNewRoad @0x822F9240 `lbz r11, 0x108(r31)`
    //   [V] +0x10C  muLastJunctionEnteredID     OnEnterJunction @0x822A7E88 `lwz r10, 0x10C(r31)`
    //   [V] +0x110  mbSendNewJunctionMessage    UpdateNewRoad @0x822F926C `lbz r11, 0x110(r31)`
    // and the five scalar timers are pinned SEMANTICALLY as well as positionally -- Update
    // @0x82306624 accumulates lfSimTimerTimeStep into exactly 0x114/0x120/0x124/0x130 (the four
    // "time since" fields), and UpdateMomentum grows 0x11C while the car is airborne and 0x118
    // while it is not, which is what mfTimeSinceLastOnGround / mfTimeSinceLastInAir mean.
    //
    // ⚠️ THE CONSOLE OFFSETS IN THE RIGHT COLUMN ARE PROVENANCE, NOT THE PC LAYOUT. Carving the
    // head introduces two x64 pointer widenings the pad hid (CrashPlayDebugComponent's vptr, and
    // RingBuffer<T>::mpData), so the PC struct is WIDER than the console's and the scalar tail no
    // longer sits at 0x134. That is the project's standard rule (widen pointers; parity is by
    // NAMED MEMBER, never by byte offset) -- so _AssertLayout below now pins the ORDER and
    // ADJACENCY of the tail, which is width-independent and is the property a re-order would
    // break, instead of the console absolutes it used to assert.
    //
    // ⚠️ mCrashCombo / mCrashBreakerParams are NOT in the DWARF's CrashPlayManager member list.
    // They are kept, unchanged, at the TAIL (past the DWARF's last member) rather than deleted --
    // nothing reads them on this build and their real home is unproven. Do not re-insert them
    // into the middle of the run: that is exactly what displaced mbIsCrashPlayActive before.
    // ============================================================================
    struct CrashPlayManager
    {
        // ---- head, DWARF BrnCrashPlayManager.h :229..:265 ---------------------------------
        CrashPlayDebugComponent mCrashPlayDebugComponent;                       // +0x000 :229
        CgsSceneManager::VolumeInstanceId mPlayerCarVolumeInstanceID;           // +0x018 :233 [V]
        Vector3 mLastPlayerPos;                                                 // +0x020 :236 [V]
        CgsContainers::FixedRingBuffer<CgsSceneManager::EntityId, KI_MAX_RECENTLY_HIT_CARS>
                mRecentCrashSet;                                                // +0x030 :240 [V]
        CgsContainers::FixedRingBuffer<CgsSceneManager::EntityId, KI_MAX_RECENTLY_LEAPT_CARS>
                mRecentLeaptSet;                                                // +0x0C4 :244 [V]
        s32   miCarsLeaptThisFrame;                                             // +0x0F8 :245 [V]
        CgsID miLastStreetEntered;                                              // +0x100 :248 [V]
        bool  mbSendNewRoadMessage;                                             // +0x108 :249 [V]
        u32   muLastJunctionEnteredID;                                          // +0x10C :250 [V]
        bool  mbSendNewJunctionMessage;                                         // +0x110 :251 [V]
        f32   mfCrashPlayTime;                                                  // +0x114 :254 [V]
        f32   mfTimeSinceLastInAir;                                             // +0x118 :255 [V]
        f32   mfTimeSinceLastOnGround;                                          // +0x11C :256 [V]
        f32   mfTimeSinceLastVehicleImpact;                                     // +0x120 :259 [V]
        f32   mfTimeSinceLastHitOverheadSign;                                   // +0x124 :260 [V]
        bool  mbTrafficStomp;                                                   // +0x128 :263 [V]
        s32   miFramesUntilAirRam;                                              // +0x12C :264 [V]
        f32   mfTimeSinceLastTrafficStomp;                                      // +0x130 :265 [V]

        // ---- scalar tail, DWARF :268..:283 ------------------------------------------------
        f32  mfBoostPercentage;                 // +0x134 (98852)  [V]
        f32  mfAftertouchPower;                 // +0x138 (98856)  [V]
        f32  mfDifficultyLevel;                 // +0x13C (98860)
        f32  mfBounceBoostTimer;                // +0x140 (98864)  [V]
        f32  mfLoseBoostGracePeriod;            // +0x144 (98868)
        s32  miConsecutiveBouncesOnGround;      // +0x148 (98872)
        bool mbIsCrashPlayActive;               // +0x14C (98876)  [V]
        bool mbIsInShowtime;                    // +0x14D (98877)  [V]
        bool mbInfiniteAftertouch;              // +0x14E (98878)  [V]
        bool mbInfiniteBoost;                   // +0x14F (98879)  [V]
        bool mbEarningAirTimeBoost;             // +0x150 (98880)
        bool mbAboutToLoseBoost;                // +0x151 (98881)
        bool mbBouncePromptNeeded;              // +0x152 (98882)
        bool mbBoostChargePending;              // +0x153 (98883)

        // Not DWARF members of this class -- see the ⚠️ in the banner. Kept at the tail.
        CrashCombo         mCrashCombo;
        CrashBreakerParams mCrashBreakerParams;

        // ---- the out-of-line surface, all fifteen of it (BrnCrashPlayManager.cpp) ----------
        // Every one of these has an X360 symbol in the ledger for this TU. The DWARF-declared
        // methods that the X360 build does NOT export out-of-line (Construct/Prepare/Release/
        // Destruct/Deactivate/OnPlayerCarCrash/OnCarLeapt/OnSmashStunt/GetBoostLevel/
        // IsPlayerInShowtimeOnGround/OnShowtimeStart/OnShowtimeEnd) are deliberately NOT declared
        // here -- declaring them would mint call sites to bodies no TU can define.
        void Activate(ActiveRaceCar* lpPlayerActiveRaceCar, f32 lfInitialBoostPercentage);
        void Update(const Matrix44Affine& lCameraTransform,
                    f32 lfSimTimerTimeStep,
                    RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                    ActiveRaceCar* lpPlayerActiveRaceCar,
                    PlayerVehicleControls* lpPlayerControls);
        void HandlePlayerToVehicleImpact(ActiveRaceCar* lpPlayerActiveRaceCar,
                                         CgsSceneManager::EntityId lHitVehicleID,
                                         const BrnPhysics::ContactSpy::RaceCarContact* lpContact);
        void OnCarCrash(CgsSceneManager::EntityId lHitVehicleID, bool lbPlayerHitCar);
        void OnEnterRoad(const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction* lpRRAction);
        void OnEnterJunction(const BrnGameState::GameStateModuleIO::SendJunctionPlayerIsAtAction* lpJAction);
        void OnBounce(const BrnGameState::GameStateModuleIO::JustBouncedAction* lpBounceAction);
        void OnHitOverheadSign();
        void OnVehicleHitConfirmed(s32 liVehicleBaseScore, s32 liVehicleChainBonus, s32 liTotalVehiclesHit);
        f32  GetShowtimeTrafficDensityScale() const;

    private:
        void UpdateMomentum(f32 lfSimTimerTimeStep,
                            ActiveRaceCar* lpPlayerActiveRaceCar,
                            RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
        void UpdateTrafficStomp(f32 lfSimTimerTimeStep,
                                PlayerVehicleControls* lpPlayerControls,
                                ActiveRaceCar* lpPlayerActiveRaceCar,
                                RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
                                Vector3 lCameraX,
                                Vector3 lCameraZ);
        void UpdateBounceBoost(f32 lfSimTimerTimeStep,
                               PlayerVehicleControls* lpPlayerControls,
                               ActiveRaceCar* lpPlayerActiveRaceCar,
                               RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
        void UpdateCarLeaping(f32 lfSimTimerTimeStep,
                              RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
        void UpdateNewRoad(f32 lfSimTimerTimeStep,
                           RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
        void SetBouncePromptNeeded(bool lbPromptNeeded,
                                   RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
        void ClampBoostLevel();

    public:
        // ---- The DWARF-declared queries the X360 inlines everywhere -------------------------
        // All four are named by the DecFIGS DWARF (BrnCrashPlayManager.h :100 / :156 / :168 /
        // :178) and none has an out-of-line X360 symbol, so they are header-inline here. Their
        // bodies are the flattened code ProcessPlayerVehicleInput @0x822FFE30 emits:
        //   IsActive()          -> the mbIsCrashPlayActive byte
        //   IsInShowtime()      -> `lbz r10, 0x14D(r11)` (the module+98877 read)
        //   IsBounceBoosting()  -> `lfs f0, +0x140 ; fcmpu 0.0 ; bgt` (the mbBoostBounce source,
        //                          and the same three instructions OnBounce @0x822A7F1C opens with)
        //   GetAftertouchLevel()-> the showtime-gated 1.5 / 1.0 / 0.0 ladder at 0x823000E8..0x82300118
        //     (not showtime -> 1.5f; showtime -> mfAftertouchPower > 0.001f ? 1.0f : 0.0f).
        //     FLAG: the OUTLINING is inference -- the DWARF names a `float32_t GetAftertouchLevel()`
        //     and this ladder is the only aftertouch-level computation in the XEX, but the console
        //     emitted it inline so the function boundary itself is not directly attested. The
        //     three constants (1.5 / 1.0 / 0.0) and the 0.001 threshold ARE asm-literal
        //     (flt_82014A8C / flt_82001C98 / flt_82001CC0 / flt_82013F90).
        //   GetMaxAftertouchPower() (DWARF :222, private on the console) -> the bare 1.0f that
        //     Activate/Update/OnHitOverheadSign/UpdateTrafficStomp all store into
        //     mfAftertouchPower and that OnBounce clamps to; inlining reversal, per AGENTS.md.
        // =====================================================================================
        // ⭐⭐⭐ WHY A SHOWTIME SESSION USED TO NEVER END, AND WHAT CLOSED IT (2026-08-29).
        // The only thing that ends an offline showtime session is
        // CrashModeScoring::HasCrashModeEnded @0x823129A0, whose second exit is an idle ladder
        // whose first term is "the player's boost has settled to ~0". That boost is
        // BoostStrategy::GetBoostAmount()/GetMaxBoost() -- recomputed every frame by the LIVE
        // UpdateOutputBoostInfo, and NOT frozen (0.595307 / 0.505987 / 0.5 across three runs).
        // It stopped moving because nothing SPENT it: the ordinary boost request is correctly
        // gated off for a crashing player (`&& !lpRaceCarState->mbCrashing` in
        // ProcessPlayerVehicleInput), and the showtime spend travels mbBoostBounce <-
        // IsBounceBoosting() <- mfBounceBoostTimer, whose only producer is UpdateBounceBoost
        // in THIS class -- which was unreconstructed. It is reconstructed now.
        // ⛔ Do NOT add a timeout to end showtime. The console's terminator is the idle ladder.
        // =====================================================================================
        bool IsActive() const         { return mbIsCrashPlayActive; }
        bool IsInShowtime() const     { return mbIsInShowtime; }
        bool IsBounceBoosting() const { return mfBounceBoostTimer > 0.0f; }

        f32 GetMaxAftertouchPower() const { return 1.0f; }

        f32 GetAftertouchLevel() const
        {
            if( !mbIsInShowtime )
            {
                return 1.5f;
            }
            return ( mfAftertouchPower > 0.001f ) ? 1.0f : 0.0f;
        }

        // Compile-time layout oracle (never called): pins the ORDER and ADJACENCY of the scalar
        // tail so a re-order fails the gate here rather than in a physics body. It no longer
        // asserts the console ABSOLUTES (0x134, 0x138, ...): carving the head in named members
        // widens two console 4-byte pointers to 8 (the debug component's vptr and
        // RingBuffer<T>::mpData), so the tail legitimately moves on this target. Adjacency is the
        // property that actually encodes the DWARF member order, and it is width-independent.
        static void _AssertLayout()
        {
            static_assert(offsetof(CrashPlayManager, mfAftertouchPower)
                          == offsetof(CrashPlayManager, mfBoostPercentage) + 4,
                          "mfAftertouchPower immediately follows mfBoostPercentage (console +0x134/+0x138)");
            static_assert(offsetof(CrashPlayManager, mfDifficultyLevel)
                          == offsetof(CrashPlayManager, mfBoostPercentage) + 8,
                          "mfDifficultyLevel @ console +0x13C");
            static_assert(offsetof(CrashPlayManager, mfBounceBoostTimer)
                          == offsetof(CrashPlayManager, mfBoostPercentage) + 12,
                          "mfBounceBoostTimer @ console +0x140");
            static_assert(offsetof(CrashPlayManager, mfLoseBoostGracePeriod)
                          == offsetof(CrashPlayManager, mfBoostPercentage) + 16,
                          "mfLoseBoostGracePeriod @ console +0x144");
            static_assert(offsetof(CrashPlayManager, miConsecutiveBouncesOnGround)
                          == offsetof(CrashPlayManager, mfBoostPercentage) + 20,
                          "miConsecutiveBouncesOnGround @ console +0x148");
            static_assert(offsetof(CrashPlayManager, mbIsCrashPlayActive)
                          == offsetof(CrashPlayManager, mfBoostPercentage) + 24,
                          "mbIsCrashPlayActive @ console +0x14C");
            static_assert(offsetof(CrashPlayManager, mbIsInShowtime)
                          == offsetof(CrashPlayManager, mbIsCrashPlayActive) + 1,
                          "mbIsInShowtime @ console +0x14D");
            static_assert(offsetof(CrashPlayManager, mbInfiniteAftertouch)
                          == offsetof(CrashPlayManager, mbIsCrashPlayActive) + 2,
                          "mbInfiniteAftertouch @ console +0x14E");
            static_assert(offsetof(CrashPlayManager, mbInfiniteBoost)
                          == offsetof(CrashPlayManager, mbIsCrashPlayActive) + 3,
                          "mbInfiniteBoost @ console +0x14F");
            static_assert(offsetof(CrashPlayManager, mbBoostChargePending)
                          == offsetof(CrashPlayManager, mbIsCrashPlayActive) + 7,
                          "mbBoostChargePending @ console +0x153 -- the whole bool run is contiguous");
            // Head: the two members whose console offsets the bodies read as literals.
            static_assert(offsetof(CrashPlayManager, mfTimeSinceLastInAir)
                          == offsetof(CrashPlayManager, mfCrashPlayTime) + 4,
                          "mfTimeSinceLastInAir @ console +0x118");
            static_assert(offsetof(CrashPlayManager, mfTimeSinceLastTrafficStomp) + 4
                          == offsetof(CrashPlayManager, mfBoostPercentage),
                          "mfTimeSinceLastTrafficStomp is the member immediately before the tail (+0x130)");
        }
    };
}

#endif
