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
// Class shape / member names / method set verbatim from the declarations
// gated in the console build ledger. This TU
// bodies Construct/Prepare/Update/SetParameters/GetName; Release/Destruct/
// GetInstanceType/SetupShot and Parameters::Construct are their own ledger
// functions (declaration-only, declaration-gated).
//
namespace BrnDirector
{
    class MomentHardStop : public Moment
    {
    public:
        //  The tuning record.
        struct Parameters : public Moment::Parameters
        {
            f32 mfDuration;
            f32 mfSpeedDiffThreshold;

            // Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, both
        // handle clears, the two shot-selection infos invalidated ({-1,-1}), and
        // the crash-count/shot-index/parameters/ready resets.
        virtual void Construct();

        // reset the per-run state (running
        // time / ready / crashing-last-frame; first-frame raised) and enter
        // SEARCHING. Always reports true.
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame hard-stop state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        virtual const char* GetName() const;

        //  Declaration-only (their own ledger functions).
        virtual bool Release();
        virtual void Destruct();

    protected:
        //  Declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_HARD_STOP == 0).
        virtual EType GetInstanceType();

    private:
        //  Declaration-only (its own ledger function).
        void SetupShot(Camera::BehaviourHandle<Camera::Behaviour>* lpHandle,
                       const void* lpShotRefSpec);

        //  (console offsets in comments; access BY NAME).
        Camera::BehaviourHandle<Camera::Behaviour> mRigCameraHandleA;   // +0x180
        Camera::BehaviourHandle<Camera::Behaviour> mRigCameraHandleB;   // +0x194
        Camera::Camera::ShotSelectionInfo mShotASelectionInfo;          // +0x1A8
        Camera::Camera::ShotSelectionInfo mShotBSelectionInfo;          // +0x1B0
        const Parameters* mpParameters;                                  // +0x1B8
        f32               mfRunningTime;                                 // +0x1BC
        s32               meCrashType;                                   // +0x1C0 (VehicleTracker::ECrashType; raw word -- the tracker enum home is pending)
        f32               mfUltraSloMoTimestepScale;                     // +0x1C4
        u32               muHighEnergyWorldCrashCount;                   // +0x1C8
        bool              mbForceUltraSloMo;                             // +0x1CC
        bool              mbCrashingLastFrame;                           // +0x1CD
        bool              mbFirstFrame;                                  // +0x1CE
        bool              mbReady;                                       // +0x1CF
        bool              mbUsingA;                                      // +0x1D0
        u32               muNextShotIndex;                               // +0x1D4
        CrashAnalysis     mCrashAnalysisUsedAtSelection;                       // +0x1D8 (8 bytes)
    };
}
