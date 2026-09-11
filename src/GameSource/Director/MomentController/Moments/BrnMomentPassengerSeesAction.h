#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"                  // BrnDirector::Moment (base) + Moment::VehicleRef
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                  // Camera::BehaviourHandle<T>
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"     // Camera::BehaviourPassengerCam

// BrnDirector::MomentPassengerSeesAction - the "passenger sees action" camera
// moment: in the opening half second of a crash, ride a passenger cam in the
// witnessing car looking at the incident. Class shape / member names / method
// set verbatim from the declarations (/
// /); gated in the console build ledger. This TU bodies all seven exported
// functions; Parameters::Construct is its own ledger function.
//
namespace BrnDirector
{
    class MomentPassengerSeesAction : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the
        // passenger-cam handle clear, the two vehicle-ref set-flag clears, and
        // the parameter reset.
        virtual void Construct();

        // zero the crash timer and enter
        // SEARCHING. Always reports true.
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame state machine
        // (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // drop the passenger cam if held,
        // clear the gates, raise the searching head bit, park at INACTIVE (0).
        virtual bool Release();

        // adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        virtual const char* GetName() const;

        //  Declaration-only (its own ledger function).
        virtual void Destruct();

    protected:
        // (this TU) -- E_MOMENT_PASSENGER_SEES_ACTION (4).
        virtual EType GetInstanceType();

    private:
        //  (console offsets in comments; access BY NAME).
        f32 mfTimeCrashing;                                                  // +0x180
        const Parameters* mpParameters;                                      // +0x184
        Camera::BehaviourHandle<Camera::BehaviourPassengerCam> mPassengerCam; // +0x188
        VehicleRef mWitness;                                                 // +0x19C
        VehicleRef mIncident;                                                // +0x1AC
    };
}
