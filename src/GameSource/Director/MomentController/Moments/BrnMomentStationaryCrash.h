#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"   // Camera::BehaviourIceAnim

// BrnDirector::MomentStationaryCrash - the "stationary crash" camera moment:
// while the player wrecks at (near) standstill, play the stationary- or
// tumbling-crash ICE camera take (the tumbling variant once the integrated
// crash rotation exceeds a full turn). Class shape / member names / method set
// verbatim from the declarations;
// gated in the console build ledger. This TU bodies Construct/Prepare/Update/Release/
// GetName; SetParameters/Destruct/GetInstanceType and Parameters::Construct are
// their own ledger functions (declaration-only, declaration-gated).
//
namespace BrnDirector
{
    class MomentStationaryCrash : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  (cpp) -- its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the
        // ICE-cam handle clear, and the take/rotation/tumbling/parameters resets.
        virtual void Construct();

        // zero the crash timer and enter
        // SEARCHING. Always reports true.
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame stationary-crash
        // state machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // drop the ICE cam if held (the
        // inlined BehaviourHandle::Release) and reset to SEARCHING. Returns true.
        virtual bool Release();

        virtual const char* GetName() const;

        //  Declaration-only (their own ledger functions).
        virtual void SetParameters(const Moment::Parameters* lpParameters);
        virtual void Destruct();

    protected:
        //  Declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_STATIONARY_CRASH == 11).
        virtual EType GetInstanceType();

    private:
        //  (console offsets in comments; access BY NAME).
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mIceCameraHandle;   // +0x180
        const Parameters* mpParameters;                                        // +0x194
        bool              mbIsTumblingCrash;                                   // +0x198 (a full turn integrated while crashing)
        u32               muTake;                                              // +0x19C
        f32               mfTimeCrashing;                                      // +0x1A0
        f32               mfRotationAngle;                                     // +0x1A4 (the integrated crash rotation, radians)
    };
}
