#ifndef BRN_CRASH_PLAY_DEBUG_COMPONENT_H
#define BRN_CRASH_PLAY_DEBUG_COMPONENT_H

#include "BrnCommonTypes.h"
#include "types.hpp"
#include <cstddef>   // offsetof (CrashPlayManager::_AssertLayout)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnWorld
{
    static const s32 KI_NUM_FUEL_TRAIL_NODES = 32;

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

    // ============================================================================
    // BrnWorld::CrashPlayManager -- the Showtime / crash-play state machine, embedded BY VALUE
    // in RaceCarEntityModule (console +98544; DWARF BrnRaceCarEntityModule.h:355).
    //
    // ⭐⭐ TAIL RE-SEATED 2026-08-11 (player-input wave). The committed model anchored ONE member
    // (mfAftertouchPower @+0x138) and then guessed the rest, which put mbIsCrashPlayActive ~0x240
    // bytes too late. The whole 0x134..0x153 run is now laid out from the DecFIGS DWARF member
    // ORDER (references/DecFIGS/.../CrashPlay/BrnCrashPlayManager.h, the fourteen members from
    // mfBoostPercentage :268 to mbBoostChargePending :283) with FIVE independent X360 anchors,
    // all of which land exactly and none of which needs a fudge. The base is asm-literal:
    // RaceCarEntityModule::HandlePrepareForModeAction calls `CrashPlayManager::Activate(module +
    // 98544, ...)`, so module+98544 == this+0.
    //   [V] +0x134 (98852) mfBoostPercentage        HandleStopModeAction @0x82307A30 Deactivate arm
    //                                               (`*(module+98852) = 0.0` next to the
    //                                               "SHOWTIME! CrashPlayManager::Deactivate
    //                                               called." log)
    //   [V] +0x138 (98856) mfAftertouchPower        ProcessPlayerVehicleInput @0x823000F8
    //                                               `lfs f13, 0x138(r11)`, r11 == module+98544
    //   [V] +0x140 (98864) mfBounceBoostTimer       ProcessPlayerVehicleInput @0x823002A4 (the
    //                                               `> 0.0f` that feeds mbBoostBounce)
    //   [V] +0x14C (98876) mbIsCrashPlayActive      HandleStopModeAction: cleared in the SAME
    //                                               arm as mfBoostPercentage (Deactivate)
    //   [V] +0x14D (98877) mbIsInShowtime           HandlePrepareForModeAction @0x823092F0 sets
    //                                               it under the mode's 0x200 flag; HandleStopMode
    //                                               clears it and dispatches the strategy's
    //                                               OnShowtimeEnd (vtable +56)
    // mbInfiniteAftertouch/mbInfiniteBoost keep the +0x14E/+0x14F the committed model already had
    // (they are the debug component's two RegisterVariable targets), which is the sixth anchor:
    // the DWARF order reaches them at exactly those two bytes.
    //
    // ⚠️ mCrashCombo / mCrashBreakerParams are NOT in the DWARF's CrashPlayManager member list.
    // They are kept, unchanged, at the TAIL (past the DWARF's last member) rather than deleted --
    // nothing reads them on this build and their real home is unproven. Do not re-insert them
    // into the middle of the run: that is exactly what displaced mbIsCrashPlayActive before.
    //
    // Parity here is BY NAME; the console offsets in the right column are the provenance.
    // ============================================================================
    struct CrashPlayManager
    {
        // +0x000 .. +0x134: mCrashPlayDebugComponent / mPlayerCarVolumeInstanceID /
        // mLastPlayerPos / the two FixedRingBuffers / miCarsLeaptThisFrame / miLastStreetEntered /
        // mbSendNewRoadMessage / muLastJunctionEnteredID / mbSendNewJunctionMessage /
        // mfCrashPlayTime / mfTimeSinceLastInAir / mfTimeSinceLastOnGround /
        // mfTimeSinceLastVehicleImpact / mfTimeSinceLastHitOverheadSign / mbTrafficStomp /
        // miFramesUntilAirRam / mfTimeSinceLastTrafficStomp (DWARF :229..:265). Types not
        // reconstructed in this tree -> honest padding sized to the console gap.
        u8   maPad00[308];                      // +0x000 (98544)  .. +0x134 (98852)

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

        // ---- The three DWARF-declared queries the X360 inlines everywhere -------------------
        // All three are named by the DecFIGS DWARF (BrnCrashPlayManager.h :100 / :156 / :168 /
        // :178) and none has an out-of-line X360 symbol, so they are header-inline here. Their
        // bodies are the flattened code ProcessPlayerVehicleInput @0x822FFE30 emits:
        //   IsActive()          -> the mbIsCrashPlayActive byte
        //   IsInShowtime()      -> `lbz r10, 0x14D(r11)` (the module+98877 read)
        //   IsBounceBoosting()  -> `lfs f0, +0x140 ; fcmpu 0.0 ; bgt` (the mbBoostBounce source)
        //   GetAftertouchLevel()-> the showtime-gated 1.5 / 1.0 / 0.0 ladder at 0x823000E8..0x82300118
        //     (not showtime -> 1.5f; showtime -> mfAftertouchPower > 0.001f ? 1.0f : 0.0f).
        //     FLAG: the OUTLINING is inference -- the DWARF names a `float32_t GetAftertouchLevel()`
        //     and this ladder is the only aftertouch-level computation in the XEX, but the console
        //     emitted it inline so the function boundary itself is not directly attested. The
        //     three constants (1.5 / 1.0 / 0.0) and the 0.001 threshold ARE asm-literal
        //     (flt_82014A8C / flt_82001C98 / flt_82001CC0 / flt_82013F90).
        bool IsActive() const         { return mbIsCrashPlayActive; }
        bool IsInShowtime() const     { return mbIsInShowtime; }
        bool IsBounceBoosting() const { return mfBounceBoostTimer > 0.0f; }

        f32 GetAftertouchLevel() const
        {
            if( !mbIsInShowtime )
            {
                return 1.5f;
            }
            return ( mfAftertouchPower > 0.001f ) ? 1.0f : 0.0f;
        }

        // Compile-time layout oracle (never called): pins the five X360-anchored offsets inside
        // the scalar tail so a re-order fails the gate here rather than in a physics body.
        // Offsets are relative to the class, which is what the module-relative anchors reduce to
        // once the console base (module+98544) is subtracted.
        static void _AssertLayout()
        {
            static_assert(offsetof(CrashPlayManager, mfBoostPercentage)  == 0x134, "mfBoostPercentage @+0x134 (module+98852)");
            static_assert(offsetof(CrashPlayManager, mfAftertouchPower)  == 0x138, "mfAftertouchPower @+0x138 (module+98856)");
            static_assert(offsetof(CrashPlayManager, mfBounceBoostTimer) == 0x140, "mfBounceBoostTimer @+0x140 (module+98864)");
            static_assert(offsetof(CrashPlayManager, mbIsCrashPlayActive)== 0x14C, "mbIsCrashPlayActive @+0x14C (module+98876)");
            static_assert(offsetof(CrashPlayManager, mbIsInShowtime)     == 0x14D, "mbIsInShowtime @+0x14D (module+98877)");
            static_assert(offsetof(CrashPlayManager, mbInfiniteAftertouch) == 0x14E, "mbInfiniteAftertouch @+0x14E");
            static_assert(offsetof(CrashPlayManager, mbInfiniteBoost)    == 0x14F, "mbInfiniteBoost @+0x14F");
        }
    };

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
}

#endif
