#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"     // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"     // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Camera.h"                  // Camera::Camera::ShotSelectionInfo
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h" // CrashAnalysis (the 8-byte snapshot)

// BrnDirector::MomentHardStop - the "hard stop" crash camera moment: on an
// eligible crash it selects TWO candidate crash shots (a preferred-side A and a
// fallback B), races both behaviours to readiness, then mirrors the winner's
// produced camera (with the crash analysis attached and, for repeated
// high-energy world crashes, a randomised ultra-slow-mo sim-time scale).
// Class shape / member names / method set verbatim from the DecFIGS DWARF
// (BrnMomentHardStop.h:35/:72-:101/:111); gated on the X360 ledger. This TU
// bodies Construct/Prepare/Update/SetParameters/GetName; Release/Destruct/
// GetInstanceType/SetupShot and Parameters::Construct are their own ledger
// functions (declaration-only, DWARF-gated).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentHardStop for the NewMoment/parameter-bank TUs;
// this real home and that stub define the same class and CANNOT be included into
// one TU. The stub's nested Parameters is layout-identical to the one below.
namespace BrnDirector
{
    class MomentHardStop : public Moment
    {
    public:
        // DWARF BrnMomentHardStop.h:111 -- the tuning record.
        struct Parameters : public Moment::Parameters
        {
            f32 mfDuration;             // h:117
            f32 mfSpeedDiffThreshold;   // h:118

            // DWARF h:115 (cpp:40) -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225EC08 (this TU, DWARF cpp:49) -- the inlined base Construct, both
        // handle clears, the two shot-selection infos invalidated ({-1,-1}), and
        // the crash-count/shot-index/parameters/ready resets.
        virtual void Construct();

        // @0x821F7518 (this TU, DWARF cpp:71) -- reset the per-run state (running
        // time / ready / crashing-last-frame; first-frame raised) and enter
        // SEARCHING. Always reports true.
        virtual bool Prepare(void* lrBehaviourController);

        // @0x82271438 (this TU, DWARF cpp:90) -- the per-frame hard-stop state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x821F7548 (this TU, DWARF cpp:421) -- adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x821F7550 (this TU, DWARF cpp:457).
        virtual const char* GetName() const;

        // DWARF cpp:397/:411 -- declaration-only (their own ledger functions).
        virtual bool Release();
        virtual void Destruct();

    protected:
        // DWARF cpp:430 -- declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_HARD_STOP == 0).
        virtual EType GetInstanceType();

    private:
        // DWARF cpp:440 -- declaration-only (its own ledger function).
        void SetupShot(Camera::BehaviourHandle<Camera::Behaviour>* lpHandle,
                       const void* lpShotRefSpec);

        // DWARF h:72-:101 (X360 offsets in comments; access BY NAME).
        Camera::BehaviourHandle<Camera::Behaviour> mRigCameraHandleA;   // h:72  +0x180
        Camera::BehaviourHandle<Camera::Behaviour> mRigCameraHandleB;   // h:73  +0x194
        Camera::Camera::ShotSelectionInfo mShotASelectionInfo;          // h:75  +0x1A8
        Camera::Camera::ShotSelectionInfo mShotBSelectionInfo;          // h:76  +0x1B0
        const Parameters* mpParameters;                                  // h:83  +0x1B8
        f32               mfRunningTime;                                 // h:84  +0x1BC
        s32               meCrashType;                                   // h:86  +0x1C0 (VehicleTracker::ECrashType; raw word -- the tracker enum home is pending)
        f32               mfUltraSloMoTimestepScale;                     // h:92  +0x1C4
        u32               muHighEnergyWorldCrashCount;                   // h:93  +0x1C8
        bool              mbForceUltraSloMo;                             // h:95  +0x1CC
        bool              mbCrashingLastFrame;                           // h:96  +0x1CD
        bool              mbFirstFrame;                                  // h:97  +0x1CE
        bool              mbReady;                                       // h:98  +0x1CF
        bool              mbUsingA;                                      // h:99  +0x1D0
        u32               muNextShotIndex;                               // h:100 +0x1D4
        CrashAnalysis     mCrashAnalysisUsedAtSelection;                       // h:101 +0x1D8 (8 bytes)
    };
}
