#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"                  // BrnDirector::Moment (base) + Moment::VehicleRef
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                  // Camera::BehaviourHandle<T>
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"     // Camera::BehaviourPassengerCam

// BrnDirector::MomentPassengerSeesAction - the "passenger sees action" camera
// moment: in the opening half second of a crash, ride a passenger cam in the
// witnessing car looking at the incident. Class shape / member names / method
// set verbatim from the DecFIGS DWARF (BrnMomentPassengerSeesAction.h:46/
// :81-:88/:98); gated on the X360 ledger. This TU bodies all seven exported
// functions; Parameters::Construct is its own ledger function.
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentPassengerSeesAction for the NewMoment TU; this
// real home and that stub define the same class and CANNOT share a TU.
namespace BrnDirector
{
    class MomentPassengerSeesAction : public Moment
    {
    public:
        // DWARF BrnMomentPassengerSeesAction.h:98 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:102 -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225EE38 (this TU, DWARF cpp:34) -- the inlined base Construct, the
        // passenger-cam handle clear, the two vehicle-ref set-flag clears, and
        // the parameter reset.
        virtual void Construct();

        // @0x821F75D0 (this TU, DWARF cpp:55) -- zero the crash timer and enter
        // SEARCHING. Always reports true.
        virtual bool Prepare(void* lrBehaviourController);

        // @0x8225EEB8 (this TU, DWARF cpp:74) -- the per-frame state machine
        // (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223AA80 (this TU, DWARF cpp:211) -- drop the passenger cam if held,
        // clear the gates, raise the searching head bit, park at INACTIVE (0).
        virtual bool Release();

        // @0x821F75F0 (this TU, DWARF cpp:243) -- adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x821F75F8 (this TU, DWARF cpp:269).
        virtual const char* GetName() const;

        // DWARF cpp:229 -- declaration-only (its own ledger function).
        virtual void Destruct();

    protected:
        // @0x829DA908 (this TU) -- E_MOMENT_PASSENGER_SEES_ACTION (4).
        virtual EType GetInstanceType();

    private:
        // DWARF h:81-:88 (X360 offsets in comments; access BY NAME).
        f32 mfTimeCrashing;                                                  // h:81  +0x180
        const Parameters* mpParameters;                                      // h:83  +0x184
        Camera::BehaviourHandle<Camera::BehaviourPassengerCam> mPassengerCam; // h:85  +0x188
        VehicleRef mWitness;                                                 // h:87  +0x19C
        VehicleRef mIncident;                                                // h:88  +0x1AC
    };
}
