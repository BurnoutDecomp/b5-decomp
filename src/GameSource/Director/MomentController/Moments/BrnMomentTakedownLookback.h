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
// returns to SEARCHING. Class shape / member names / method set verbatim from the
// DecFIGS DWARF (BrnMomentTakedownLookback.h:46/:81-:86/:96); gated on the X360
// ledger. This TU bodies Construct/Update/Release/GetName; Prepare/Destruct/
// SetParameters/GetInstanceType and Parameters::Construct are their own ledger
// functions (declaration-only, DWARF-gated).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentTakedownLookback for the NewMoment/parameter-bank
// TUs; this real home and that stub define the same class and CANNOT be included
// into one TU.
namespace BrnDirector
{
    class MomentTakedownLookback : public Moment
    {
    public:
        // DWARF BrnMomentTakedownLookback.h:96 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:100 (cpp) -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225EDB8 (this TU, DWARF cpp:34) -- the inlined base Construct, the rig
        // handle clear, the authored lookback-rig Parameters::Construct, the victim
        // ref clear, and the parameters reset.
        virtual void Construct();

        // DWARF cpp:55 -- declaration-only (its own ledger function).
        virtual bool Prepare(void* lrBehaviourController);

        // @0x822662F0 (this TU, DWARF cpp:72) -- the per-frame look-back state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223AA08 (this TU, DWARF cpp:171) -- drop the rig cam if held (the
        // inlined guarded BehaviourHandle::Release), clear the gates, raise the
        // searching head bit, and reset to INACTIVE (state 0). Returns true.
        virtual bool Release();

        // @0x821F75C0 (this TU, DWARF cpp:229).
        virtual const char* GetName() const;

        // DWARF cpp:203/:189 -- declaration-only (their own ledger functions).
        virtual void SetParameters(const Moment::Parameters* lpParameters);
        virtual void Destruct();

    protected:
        // DWARF cpp:215 -- declaration-only (its own ledger function); the value is
        // pinned by the EType table (E_MOMENT_TAKEDOWN_LOOKBACK == 3).
        virtual EType GetInstanceType();

    private:
        // DWARF h:81-:86 (X360 offsets in comments; access BY NAME).
        const Parameters*                    mpParameters;         // h:81  +0x180
        Camera::BehaviourRig::Parameters     mLookbackRigParams;   // h:83  +0x190 (0x120-byte authored block)
        Camera::BehaviourHandle<Camera::BehaviourRig> mRigCameraHandle;  // h:84  +0x2B0 (5-word handle)
        Moment::VehicleRef                   mVictim;              // h:86  +0x2C4 (the taken-down car)
    };
}
