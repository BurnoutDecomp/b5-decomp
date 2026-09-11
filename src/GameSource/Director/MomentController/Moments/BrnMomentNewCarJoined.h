#pragma once

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"                       // BrnDirector::Moment (base)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                       // Camera::BehaviourHandle<T> / BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"        // Camera::BehaviourInterpolate (+ Parameters, by value)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"    // Camera::BehaviourLooseAttachment (+ Parameters, by value)

// BrnDirector::MomentNewCarJoined - the "new car joined" camera moment: when a
// rival car streams into the player's world (online join), blend the camera from
// the gameplay view (helper slot 1) out to a loose-attachment framing of the
// join, hold it (with the 2/7 slow-mo + "Rival_Join" PFX hook), then blend back.
// Class shape / member names / method set verbatim from the declarations
// gated in the console build ledger. This TU
// bodies Construct/Update/GetName/GetInstanceType (the 4 console-ledger functions);
// Prepare/Destruct/SetParameters and Parameters::Construct have NO standalone
// console symbol (ICF-folded with their byte-identical siblings) and are
// declaration-only, declaration-gated. Release IS a standalone console
// function but is ledgered under the class:BrnBehaviourManager.h TU key (a
// Declaration-vs-inlining misattribution -- the body is three inlined
// BehaviourHandle::Release runs); declared here at its declared home .
//
namespace BrnDirector
{
    class MomentNewCarJoined : public Moment
    {
    public:
        //  The (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            //  No standalone console symbol (declaration-only).
            void Construct();
        };

        // the inlined base Construct, the
        // three behaviour-handle clears (the loose-attachment clear is stored
        // TWICE by the console build), the interpolate parameter defaults
        // {rotate-about-player-car, exponential-out-x-cubed}, the loose-
        // attachment parameter defaults (height 0.75 / distance 3 / +0x54 40),
        // and the parameters-pointer clear.
        virtual void Construct();

        //  No standalone console symbol (ICF-folded; declaration-only).
        virtual bool Prepare(void* lrBehaviourController);

        // the per-frame new-car-joined
        // state machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // release all three behaviour handles,
        // drop the gates, raise the searching head bit, park at SEARCHING.
        // ⚠️ LEDGER KEY: this function is ledgered under the
        // GameSource/Director/Camera/BrnBehaviourManager.h TU (misattribution);
        // declared at its declared home. Declaration-only until its body lands.
        virtual bool Release();

        //  No standalone console symbol (declaration-only).
        virtual void Destruct();

        //  No standalone console symbol (declaration-only).
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        virtual const char* GetName() const;

    protected:
        // E_MOMENT_NEW_CAR_JOINED == 10.
        virtual EType GetInstanceType();

    private:
        //  (console offsets in comments; access BY NAME).
        const Parameters* mpParameters;                                            // +0x180
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>     mInterpolaterA;  // +0x184 (gameplay -> loose attachment)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>     mInterpolaterB;  // +0x198 (loose attachment -> gameplay)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mLooseAttachment;// +0x1AC
        Camera::BehaviourInterpolate::Parameters     mInterpolateParams;           // +0x1C0
        Camera::BehaviourLooseAttachment::Parameters mLooseAttachmentParameters;   // +0x1D0
        f32 mfTimeInState;                                                         // +0x234 (time spent in the VALID hold)
    };
}
