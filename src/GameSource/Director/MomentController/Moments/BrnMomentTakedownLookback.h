#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"             // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"         // Camera::BehaviourRig (+ its Parameters, held by value)

// BrnDirector::MomentTakedownLookback - the "takedown look-back" camera moment:
// when the player takes a rival down, run a rig camera that looks back at the
// dispatched victim car. The moment stays valid while the victim is roughly
// BEHIND the player (the along-track dot is negative) and still within ~60m;
// once the victim drifts ahead or out of range the rig is dropped and the moment
// returns to SEARCHING. Class shape / member names / method set verbatim from
// the declarations; gated in the console build
// ledger. This TU bodies Construct/Update/Release/GetName; Prepare/Destruct/
// SetParameters/GetInstanceType and Parameters::Construct are their own ledger
// functions (declaration-only, declaration-gated).
//
namespace BrnDirector
{
    class MomentTakedownLookback : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  (cpp) -- its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the rig
        // handle clear, the authored lookback-rig Parameters::Construct, the victim
        // ref clear, and the parameters reset.
        virtual void Construct();

        //  Declaration-only (its own ledger function).
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame look-back state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // drop the rig cam if held (the
        // inlined guarded BehaviourHandle::Release), clear the gates, raise the
        // searching head bit, and reset to INACTIVE (state 0). Returns true.
        virtual bool Release();

        virtual const char* GetName() const;

        //  Declaration-only (their own ledger functions).
        virtual void SetParameters(const Moment::Parameters* lpParameters);
        virtual void Destruct();

    protected:
        //  Declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_TAKEDOWN_LOOKBACK == 3).
        virtual EType GetInstanceType();

    private:
        //  (console offsets in comments; access BY NAME).
        const Parameters*                    mpParameters;         // +0x180
        Camera::BehaviourRig::Parameters     mLookbackRigParams;   // +0x190 (0x120-byte authored block)
        Camera::BehaviourHandle<Camera::BehaviourRig> mRigCameraHandle;  // +0x2B0 (5-word handle)
        Moment::VehicleRef                   mVictim;              // +0x2C4 (the taken-down car)
    };
}
