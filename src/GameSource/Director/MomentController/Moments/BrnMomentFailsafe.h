#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"   // BrnDirector::Moment (base)

// BrnDirector::MomentFailSafe - the fallback camera moment the moment controller runs
// when nothing more specific applies. DWARF home BrnMomentFailsafe.h:46. This TU bodies
// Construct/Update/GetName; Prepare/Release/Destruct/SetParameters/GetInstanceType are
// their own ledger functions (declaration-only overrides here, DWARF-gated).
namespace BrnDirector
{
    class MomentFailSafe : public Moment
    {
    public:
        // DWARF BrnMomentFailsafe.h:89 -- the fail-safe's (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:93 / cpp: its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225F190 (this TU, DWARF cpp:34) -- the inlined base Construct plus the
        // fail-safe's own parameter-pointer reset.
        virtual void Construct();

        // DWARF cpp:50/105/120/134 -- declaration-only (their own ledger functions).
        virtual bool Prepare(void* lrBehaviourController);
        virtual bool Release();
        virtual void Destruct();
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x8220A2B0 (this TU, DWARF cpp:67) -- the per-frame fail-safe state machine.
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x821F7618 (this TU, DWARF cpp:159).
        virtual const char* GetName() const;

    protected:
        // DWARF cpp:146 -- declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_FAILSAFE == 6).
        virtual EType GetInstanceType();

    private:
        const Parameters* mpParameters;   // DWARF h:79 (X360 this+0x180)
    };
}
