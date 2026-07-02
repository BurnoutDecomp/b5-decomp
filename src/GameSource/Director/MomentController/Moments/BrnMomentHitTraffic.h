#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"   // Camera::BehaviourGyroCam

// BrnDirector::MomentHitTraffic - the "hit traffic" camera moment: while the
// hit-traffic condition holds, allocate a gyro-cam behaviour, mirror its produced
// camera, and stay valid for up to a second past the condition. DWARF home
// BrnMomentHitTraffic.h:47. This TU bodies Construct/Update/Release/GetName;
// Prepare/Destruct/SetParameters/GetInstanceType are their own ledger functions
// (declaration-only overrides here, DWARF-gated).
namespace BrnDirector
{
    class MomentHitTraffic : public Moment
    {
    public:
        // DWARF BrnMomentHitTraffic.h:96 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:100 / cpp: its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225ECB0 (this TU, DWARF cpp:34) -- the inlined base Construct plus the
        // heli-cam handle clear and the parameter-pointer reset.
        virtual void Construct();

        // DWARF cpp:52/156/170 -- declaration-only (their own ledger functions).
        virtual bool Prepare(void* lrBehaviourController);
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x82271D90 (this TU, DWARF cpp:69) -- the per-frame hit-traffic state machine.
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223A918 (this TU, DWARF cpp:138).
        virtual bool Release();

        // @0x821F7578 (this TU, DWARF cpp:196).
        virtual const char* GetName() const;

    protected:
        // DWARF cpp:182 -- declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_HIT_TRAFFIC == 1).
        virtual EType GetInstanceType();

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mHeliCamHandle; // DWARF h:82 (X360 this+0x180)
        const Parameters* mpParameters;                                    // DWARF h:84 (X360 this+0x194)
        f32               mfRunningTime;                                   // DWARF h:86 (X360 this+0x198)
    };
}
