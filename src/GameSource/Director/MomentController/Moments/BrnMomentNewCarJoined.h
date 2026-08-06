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
// Class shape / member names / method set verbatim from the DecFIGS DWARF
// (BrnMomentNewCarJoined.h:46/:81-:89/:99); gated on the X360 ledger. This TU
// bodies Construct/Update/GetName/GetInstanceType (the 4 X360-ledger functions);
// Prepare/Destruct/SetParameters and Parameters::Construct have NO standalone
// X360 symbol (ICF-folded with their byte-identical siblings) and are
// declaration-only, DWARF-gated. Release @0x8223B078 IS a standalone X360
// function but is ledgered under the class:BrnBehaviourManager.h TU key (a
// DWARF-inlining misattribution -- the body is three inlined
// BehaviourHandle::Release runs); declared here at its DWARF home (cpp:200).
//
// NOTE (mutual exclusion, the MomentHitTraffic precedent): BrnMomentSubclasses.h
// carries a layout-stubbed MomentNewCarJoined for the NewMoment TU; this real
// home and that stub define the same class and CANNOT be included into one TU.
namespace BrnDirector
{
    class MomentNewCarJoined : public Moment
    {
    public:
        // DWARF BrnMomentNewCarJoined.h:99 -- the (member-less) tuning record.
        struct Parameters : public Moment::Parameters
        {
            // DWARF h:103 -- no standalone X360 symbol (declaration-only).
            void Construct();
        };

        // @0x8225F408 (this TU, DWARF cpp:34) -- the inlined base Construct, the
        // three behaviour-handle clears (the loose-attachment clear is stored
        // TWICE by the X360), the interpolate parameter defaults
        // {rotate-about-player-car, exponential-out-x-cubed}, the loose-
        // attachment parameter defaults (height 0.75 / distance 3 / +0x54 40),
        // and the parameters-pointer clear.
        virtual void Construct();

        // DWARF cpp:64 -- no standalone X360 symbol (ICF-folded; declaration-only).
        virtual bool Prepare(void* lrBehaviourController);

        // @0x82266C30 (this TU, DWARF cpp:81) -- the per-frame new-car-joined
        // state machine (see the .cpp).
        virtual void Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo);

        // @0x8223B078 (DWARF cpp:200) -- release all three behaviour handles,
        // drop the gates, raise the searching head bit, park at SEARCHING.
        // ⚠️ LEDGER KEY: this function is ledgered under the
        // GameSource/Director/Camera/BrnBehaviourManager.h TU (misattribution);
        // declared at its DWARF home. Declaration-only until its body lands.
        virtual bool Release();

        // DWARF cpp:220 -- no standalone X360 symbol (declaration-only).
        virtual void Destruct();

        // DWARF cpp:234 -- no standalone X360 symbol (declaration-only).
        virtual void SetParameters(const Moment::Parameters* lpParameters);

        // @0x821F7678 (this TU, DWARF cpp:260).
        virtual const char* GetName() const;

    protected:
        // @0x828A80F0 (this TU, DWARF cpp:246) -- E_MOMENT_NEW_CAR_JOINED == 10.
        virtual EType GetInstanceType();

    private:
        // DWARF h:81-:89 (X360 console offsets in comments; access BY NAME).
        const Parameters* mpParameters;                                            // h:81  +0x180
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>     mInterpolaterA;  // h:83  +0x184 (gameplay -> loose attachment)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>     mInterpolaterB;  // h:84  +0x198 (loose attachment -> gameplay)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mLooseAttachment;// h:85  +0x1AC
        Camera::BehaviourInterpolate::Parameters     mInterpolateParams;           // h:86  +0x1C0
        Camera::BehaviourLooseAttachment::Parameters mLooseAttachmentParameters;   // h:87  +0x1D0
        f32 mfTimeInState;                                                         // h:89  +0x234 (time spent in the VALID hold)
    };
}
