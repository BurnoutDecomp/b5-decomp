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
// method set verbatim from the DecFIGS DWARF (BrnMomentTumbling.h:47/:93-:106/
// :116-:134); gated on the X360 ledger. This TU bodies Construct/Update/
// Release/SetParameters/GetName/SignalIsGoodTimeToPlant; Prepare/Destruct/
// GetInstanceType/SetGyroCamParameters and Parameters::Construct are their own
// ledger functions (declaration-only, DWARF-gated).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentTumbling (its Parameters modelled there for the
// parameter bank); this real home and that stub define the same class and CANNOT
// be included into one TU. The stub's nested Parameters is layout-identical.
namespace BrnDirector
{
    class MomentTumbling : public Moment
    {
    public:
        // DWARF BrnMomentTumbling.h:116 -- the tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:119.
            enum ESubType
            {
                E_SUBTYPE_TRUCKING_SIDE  = 0,
                E_SUBTYPE_TRUCKING_FRONT = 1,
                E_SUBTYPE_FOLLOW         = 2,
                E_SUBTYPE_LEAD           = 3,
                E_SUBTYPE_SIDE           = 4,
            };

            ESubType meSubType;        // h:128
            bool     mbCrashMoment;    // h:130
            bool     mbTakedownMoment; // h:131

            // DWARF h:134 -- its own ledger function (declaration-only).
            void Construct();
        };

        // @0x8225ED28 (this TU, DWARF cpp:46) -- the inlined base Construct, the
        // gyro handle clear, and the try/first-crash latch seeds.
        virtual void Construct();

        // DWARF cpp:70 -- its own ledger function (declaration-only).
        virtual bool Prepare(void* lrBehaviourController);

        // @0x82271F28 (this TU, DWARF cpp:87) -- the per-frame tumbling state
        // machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223A990 (this TU, DWARF cpp:356) -- drop the gyro cam if held, clear
        // the gates, raise the searching head bit, and reset to INACTIVE (this
        // moment's Release parks at state 0, unlike its siblings' SEARCHING).
        virtual bool Release();

        // @0x821F75A8 (this TU, DWARF cpp:388) -- adopt the tuning record.
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x821F75B0 (this TU).
        virtual const char* GetName() const;

        // DWARF cpp:374 -- declaration-only (its own ledger function).
        virtual void Destruct();

        // @0x8220A078 (this TU, DWARF cpp:265's IsValid assert) -- on a
        // LEAD-subtype tumble, raise the gyro rig's plant request pair.
        void SignalIsGoodTimeToPlant();

    protected:
        // DWARF -- declaration-only (its own ledger function).
        virtual EType GetInstanceType();

    private:
        // DWARF cpp:4xx -- its own ledger function (declaration-only): push the
        // subtype-selected gyro parameter block onto the freshly allocated rig.
        void SetGyroCamParameters(const void* lSharedInfo);

        // DWARF h:93-:106 (X360 offsets in comments; access BY NAME).
        Vector3 mSmoothedAngularVelocity;                            // h:93  +0x180
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mGyroCam;  // h:94  +0x190
        f32 mfRunningTime;                                           // h:96  +0x1A4
        const Parameters* mpParameters;                              // h:98  +0x1A8
        bool mbTryTrucking;                                          // h:100 +0x1AC
        bool mbTryLeft;                                              // h:101 +0x1AD
        bool mbFirstTryThisCrash;                                    // h:102 +0x1AE
        bool mbUseLeftForThisCrash;                                  // h:103 +0x1AF
        bool mbUseRightForThisCrash;                                 // h:104 +0x1B0
        bool mbLookingAtTakedown;                                    // h:106 +0x1B1
    };
}
