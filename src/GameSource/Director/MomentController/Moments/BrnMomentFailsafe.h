#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"   // BrnDirector::Moment (base)

// BrnDirector::MomentFailSafe - the fallback camera moment the moment controller runs
// when nothing more specific applies. This TU bodies
// Construct/Update/GetName; Prepare/Release/Destruct/SetParameters/GetInstanceType are
// their own ledger functions (declaration-only overrides here, declaration-gated).
namespace BrnDirector
{
    class MomentFailSafe : public Moment
    {
    public:
        //  The fail-safe's (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct plus the
        // fail-safe's own parameter-pointer reset.
        virtual void Construct();

        //  Declaration-only (their own ledger functions).
        virtual bool Prepare(void* lrBehaviourController);
        virtual bool Release();
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // the per-frame fail-safe state machine.
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        virtual const char* GetName() const;

    protected:
        //  Declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_FAILSAFE == 6).
        virtual EType GetInstanceType();

    private:
        const Parameters* mpParameters;   // +0x180
    };
}
