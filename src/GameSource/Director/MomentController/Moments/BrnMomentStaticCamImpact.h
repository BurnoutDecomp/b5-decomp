#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"  // Camera::BehaviourFixedCam

// BrnDirector::MomentStaticCamImpact - the "static cam impact" camera moment: on an
// impact it allocates a fixed camera and mirrors its produced camera while valid.
// This TU bodies Construct/Update/Release/
// GetName; Prepare/Destruct/SetParameters/GetInstanceType are their own ledger
// functions (declaration-only overrides here, declaration-gated).
namespace BrnDirector
{
    class MomentStaticCamImpact : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct plus the
        // fixed-cam handle clear and the parameter-pointer reset.
        virtual void Construct();

        //  Declaration-only (their own ledger functions).
        virtual bool Prepare(void* lrBehaviourController);
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // the per-frame static-cam state machine.
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        virtual bool Release();

        virtual const char* GetName() const;

    protected:
        //  Declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_STATIC_CAM_IMPACT == 9).
        virtual EType GetInstanceType();

    private:
        Camera::BehaviourHandle<Camera::BehaviourFixedCam> mFixedCam; // +0x180
        const Parameters* mpParameters;                                // +0x194
    };
}
