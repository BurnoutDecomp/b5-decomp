#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"  // Camera::BehaviourFixedCam

// BrnDirector::MomentStaticCamImpact - the "static cam impact" camera moment: on an
// impact it allocates a fixed camera and mirrors its produced camera while valid.
// DWARF home BrnMomentStaticCamImpact.h:46. This TU bodies Construct/Update/Release/
// GetName; Prepare/Destruct/SetParameters/GetInstanceType are their own ledger
// functions (declaration-only overrides here, DWARF-gated).
namespace BrnDirector
{
    class MomentStaticCamImpact : public Moment
    {
    public:
        // DWARF BrnMomentStaticCamImpact.h:93 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:97 / cpp: its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225F390 (this TU, DWARF cpp:34) -- the inlined base Construct plus the
        // fixed-cam handle clear and the parameter-pointer reset.
        virtual void Construct();

        // DWARF cpp:52/165/179 -- declaration-only (their own ledger functions).
        virtual bool Prepare(void* lrBehaviourController);
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x82266AB0 (this TU, DWARF cpp:68) -- the per-frame static-cam state machine.
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223AFF8 (this TU, DWARF cpp:147).
        virtual bool Release();

        // @0x821F7660 (this TU, DWARF cpp:205).
        virtual const char* GetName() const;

    protected:
        // DWARF cpp:191 -- declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_STATIC_CAM_IMPACT == 9).
        virtual EType GetInstanceType();

    private:
        Camera::BehaviourHandle<Camera::BehaviourFixedCam> mFixedCam; // DWARF h:81 (X360 this+0x180)
        const Parameters* mpParameters;                                // DWARF h:83 (X360 this+0x194)
    };
}
