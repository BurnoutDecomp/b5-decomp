#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector3
#include "GameSource/Director/MomentController/BrnMoment.h"              // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<T>
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"   // Camera::BehaviourGyroCam

// BrnDirector::MomentTumbling - the "tumbling" camera moment: while the player
// (or a takedown victim) wrecks at speed, run a gyro cam on the tumbling car,
// smoothing the angular velocity each frame and signalling the gyro rig when a
// LEAD-subtype tumble is a good time to plant. Class shape / member names /
// method set verbatim from the declarations (BrnMomentTumbling.h);
// gated in the console build ledger. This TU bodies Construct/Prepare/Update/
// Release/SetParameters/GetName/SignalIsGoodTimeToPlant; Destruct/
// GetInstanceType and Parameters::Construct are their own ledger functions
// (declaration-only); SetGyroCamParameters is bodied with this TU.
//
namespace BrnDirector
{
    class MomentTumbling : public Moment
    {
    public:
        //  The tuning record.
        struct Parameters : public Moment::Parameters
        {
            enum ESubType
            {
                E_SUBTYPE_TRUCKING_SIDE  = 0,
                E_SUBTYPE_TRUCKING_FRONT = 1,
                E_SUBTYPE_FOLLOW         = 2,
                E_SUBTYPE_LEAD           = 3,
                E_SUBTYPE_SIDE           = 4,
            };

            ESubType meSubType;
            bool     mbCrashMoment;
            bool     mbTakedownMoment;

            //  Its own ledger function (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the
        // gyro handle clear, and the try/first-crash latch seeds.
        virtual void Construct();

        // (this TU) -- park at SEARCHING and clear the
        // smoothed angular velocity; allocates nothing.
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame tumbling state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // drop the gyro cam if held, clear
        // the gates, raise the searching head bit, and reset to INACTIVE (this
        // moment's Release parks at state 0, unlike its siblings' SEARCHING).
        virtual bool Release();

        // adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // (this TU).
        virtual const char* GetName() const;

        //  Declaration-only (its own ledger function).
        virtual void Destruct();

        // (this TU, 's IsValid assert) -- on a
        // LEAD-subtype tumble, raise the gyro rig's plant request pair.
        void SignalIsGoodTimeToPlant();

    protected:
        // The declaration -- declaration-only (its own ledger function).
        virtual EType GetInstanceType();

    private:
        // Bodied in this TU -- push the subtype-selected gyro parameter block onto
        // the freshly allocated rig.
        void SetGyroCamParameters(const void* lSharedInfo);

        //  (console offsets in comments; access BY NAME).
        Vector3 mSmoothedAngularVelocity;                            // +0x180
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mGyroCam;  // +0x190
        f32 mfRunningTime;                                           // +0x1A4
        const Parameters* mpParameters;                              // +0x1A8
        bool mbTryTrucking;                                          // +0x1AC
        bool mbTryLeft;                                              // +0x1AD
        bool mbFirstTryThisCrash;                                    // +0x1AE
        bool mbUseLeftForThisCrash;                                  // +0x1AF
        bool mbUseRightForThisCrash;                                 // +0x1B0
        bool mbLookingAtTakedown;                                    // +0x1B1
    };
}
