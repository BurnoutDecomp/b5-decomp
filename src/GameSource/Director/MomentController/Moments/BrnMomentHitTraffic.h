#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"   // Camera::BehaviourGyroCam

// BrnDirector::MomentHitTraffic - the "hit traffic" camera moment: while the
// hit-traffic condition holds, allocate a gyro-cam behaviour, mirror its produced
// camera, and stay valid for up to a second past the condition. declared home
// This TU bodies Construct/Update/Release/GetName;
// Prepare/Destruct/SetParameters/GetInstanceType are their own ledger functions
// (declaration-only overrides here, declaration-gated).
namespace BrnDirector
{
    class MomentHitTraffic : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct plus the
        // heli-cam handle clear and the parameter-pointer reset.
        virtual void Construct();

        //  Declaration-only (their own ledger functions).
        virtual bool Prepare(void* lrBehaviourController);
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // the per-frame hit-traffic state machine.
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        virtual bool Release();

        virtual const char* GetName() const;

    protected:
        //  Declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_HIT_TRAFFIC == 1).
        virtual EType GetInstanceType();

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mHeliCamHandle; // +0x180
        const Parameters* mpParameters;                                    // +0x194
        f32               mfRunningTime;                                   // +0x198
    };
}
